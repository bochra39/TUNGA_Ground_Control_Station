#include <tf/transform_datatypes.h>
#include <cmath>
#include <QDebug>
#include "RosBridge.h"


RosBridge::RosBridge(QObject* parent, const QString& ns) : QObject(parent), ns_(ns) {
	if (!ros::isInitialized()) { int argc=0; char** argv=nullptr; ros::init(argc, argv, "ui_bridge"); }
	nh_ = std::make_shared<ros::NodeHandle>();
	// Prefix helper
	auto tp = [this](const std::string& tail)->std::string{
		if (ns_.isEmpty()) return tail;
		return std::string("/") + ns_.toStdString() + tail;
	};
	// Subscriptions with optional namespace
	sub_gps_   = nh_->subscribe(tp("/mavros/global_position/global"), 10, &RosBridge::gpsCb, this);
	sub_odom_  = nh_->subscribe(tp("/mavros/local_position/odom"),   10, &RosBridge::odomCb, this);
	sub_state_ = nh_->subscribe(tp("/mavros/state"),                 10, &RosBridge::stateCb, this);
	setmode_   = nh_->serviceClient<mavros_msgs::SetMode>(tp("/mavros/set_mode"));

	// Yaw sources
	sub_compass_ = nh_->subscribe(tp("/mavros/global_position/compass_hdg"), 10, &RosBridge::compassCb, this);
	sub_vfr_     = nh_->subscribe(tp("/mavros/vfr_hud"), 10, &RosBridge::vfrCb, this);

	spinner_   = std::make_shared<ros::AsyncSpinner>(1);
	spinner_->start();
	
	// Sabit timer ile filtreleme
	tick_ = new QTimer(this);
	connect(tick_, &QTimer::timeout, this, [this](){
		double lat = raw_lat_.load();
		double lon = raw_lon_.load();
		if (!std::isnan(lat) && !std::isnan(lon)) {
			// İlk değer ata
			if (std::isnan(filt_lat_) || std::isnan(filt_lon_)) {
				filt_lat_ = lat; filt_lon_ = lon;
			} else {
				// sıçrama kontrolü
				double step = haversine_m(filt_lat_, filt_lon_, lat, lon);
				if (step > max_step_m_) {
					// aşırı zıplamayı yok say
					lat = filt_lat_;
					lon = filt_lon_;
				}
				// exponential smoothing
				filt_lat_ = alpha_ * lat + (1.0 - alpha_) * filt_lat_;
				filt_lon_ = alpha_ * lon + (1.0 - alpha_) * filt_lon_;
			}
			// qDebug kaldırıldı: PosUpdated
			emit posUpdated(filt_lat_, filt_lon_);
		}

		// --- YAW ---
		double raw_yaw = raw_yaw_deg_.load();
		if (!std::isnan(raw_yaw)) {
			raw_yaw = normalize360(raw_yaw);
			if (std::isnan(filt_yaw_deg_)) {
				filt_yaw_deg_ = raw_yaw;
			} else {
				filt_yaw_deg_ = smoothAngleDeg(filt_yaw_deg_, raw_yaw, yaw_alpha_);
			}
			// qDebug kaldırıldı: Yaw
			emit yawUpdated(filt_yaw_deg_);
		}
	});
	tick_->start(100); // 10 Hz
}

void RosBridge::gpsCb(const sensor_msgs::NavSatFix::ConstPtr& m) {
	if (m->status.status < sensor_msgs::NavSatStatus::STATUS_FIX) return; // fix yoksa atla
	raw_lat_.store(m->latitude);
	raw_lon_.store(m->longitude);
	// qDebug kaldırıldı: GPS
	// emit yok - sadece timer ile yayın
}

void RosBridge::compassCb(const std_msgs::Float64::ConstPtr& m) {
	have_compass_ = true;
	raw_yaw_deg_.store(m->data);
}

void RosBridge::vfrCb(const mavros_msgs::VFR_HUD::ConstPtr& m) {
	if (!have_compass_) {
		have_vfr_ = true;
		raw_yaw_deg_.store(double(m->heading));
	}
}

void RosBridge::odomCb(const nav_msgs::Odometry::ConstPtr& m) {
	if (have_compass_ || have_vfr_) return;  // daha iyi kaynak varsa odom’u kullanma
	const auto& q = m->pose.pose.orientation;
	tf::Quaternion qt(q.x, q.y, q.z, q.w);
	double r,p,y; tf::Matrix3x3(qt).getRPY(r,p,y);
	double yaw_deg = y * 180.0 / M_PI;      // -180..180
	raw_yaw_deg_.store(yaw_deg);
}

void RosBridge::stateCb(const mavros_msgs::State::ConstPtr& s) {
	emit modeUpdated(QString::fromStdString(s->mode), s->armed);
}

bool RosBridge::setMode(const QString& modeText) {
	mavros_msgs::SetMode srv; srv.request.custom_mode = modeText.toStdString();
	return setmode_.call(srv) && srv.response.mode_sent;
}

// Yardımcı: haversine
double RosBridge::haversine_m(double lat1, double lon1, double lat2, double lon2){
	static constexpr double R=6371000.0; // m
	double dlat=(lat2-lat1)*M_PI/180.0;
	double dlon=(lon2-lon1)*M_PI/180.0;
	double a=sin(dlat/2)*sin(dlat/2)+cos(lat1*M_PI/180.0)*cos(lat2*M_PI/180.0)*sin(dlon/2)*sin(dlon/2);
	double c=2*atan2(sqrt(a),sqrt(1-a));
	return R*c;
}

double RosBridge::normalize360(double deg) {
	double d = fmod(deg, 360.0);
	if (d < 0) d += 360.0;
	return d;
}

// sin-cos karışımı ile açısal EMA (wrap-around güvenli)
double RosBridge::smoothAngleDeg(double prev_deg, double meas_deg, double alpha) {
	double a = prev_deg * M_PI/180.0;
	double b = meas_deg * M_PI/180.0;
	double cx = (1.0 - alpha) * cos(a) + alpha * cos(b);
	double sx = (1.0 - alpha) * sin(a) + alpha * sin(b);
	double out = atan2(sx, cx) * 180.0 / M_PI;
	return normalize360(out);
}
