import QtQuick
import QtQuick.Controls
import QtLocation 6.9
import QtPositioning 6.9

Rectangle {
    anchors.fill: parent
    id: window
    color: "white"

    // ================== GENEL STATE ==================
    property var selectedIha: null
    property bool isEditModeActive: false
    property int  currentIndex: 0
    // Harita hazır olana kadar gelen rakip güncellemelerini kuyrukla
    property bool mapReady: false
    property var pendingRivalUpdates: [] // {teamNo, lat, lon, yaw}
    
    // IHA bilgi paneli için state
    property bool ihaInfoPanelVisible: false
    property var selectedIhaInfo: null

    // Kendi İHA başlangıcı
    property real ihaLatLocation: 40.201111
    property real ihaLonLocation: 25.882223
    

    // Sınır kutusu
    property real borderDelta: 0.015
    property var coords: ({
        "topLeft":     { "latitude": 40.201111 + borderDelta, "longitude": 25.882223 - borderDelta },
        "bottomRight": { "latitude": 40.201111 - borderDelta, "longitude": 25.882223 + borderDelta }
    })

    // Kendi İHA yaw
    property real previousRotation: 0
    property real ihaYaw: 0
    
    // Kendi İHA altitude ve speed
    property real ihaAltitude: 0
    property real ihaSpeed: 0
    
    // ================== TRAIL SİSTEMİ ==================
    // İz veri yapısı ve ayarları
    property var  trailPoints: []        // {lat, lon, ts} dizisi
    property int  trailMaxAgeSec: 180    // izin görünür kalacağı süre (saniye) - 3 dakika
    property int  trailMaxPoints: 2000   // yalnızca kapasite güvenliği
    property real trailMinStepM: 2.0     // küçük jitter'ı yutmak için
    property real trailWidthPx: 1.8      // ince çizgi (1.2–2.2 önerilir)
    property real trailGamma: 1.3        // (kullanılmayabilir)
    property color trailColor: "#00A8FF" // (eski sistem kalıntısı)
    property bool  trailUseTimeFade: true // (eski sistem kalıntısı)
    property bool  _trailPrimed: false    // ilk nokta yüklendi mi?

    // *** ANLIK KUYRUK (ZAMAN BAZLI) ***
    property bool  ephemeralTail: true       // açık
    property int   recentAgeSec: 20          // sadece SON 20 saniye kalsın
    property color colRecent: "#FFD600"   // sarı
    property bool  showHeading: false        // heading vektörü kapalı
    property int   fadeOutMs: 0              // 0 = fade yok, aniden yok olma
    

    // HSS (Hassas Güvenlik Sahası) koordinatları
    property real hssLat: 29.329423
    property real hssLon: 2.875411
    property real hssRadius: 100  // metre cinsinden yarıçap

    // İz sistemi yardımcı fonksiyonları
    function _toRad(x) { return x * Math.PI/180.0 }
    function _haversine_m(lat1, lon1, lat2, lon2) {
        var R = 6378137.0
        var dLat = _toRad(lat2 - lat1)
        var dLon = _toRad(lon2 - lon1)
        var a = Math.sin(dLat/2)*Math.sin(dLat/2) +
                Math.cos(_toRad(lat1))*Math.cos(_toRad(lat2)) *
                Math.sin(dLon/2)*Math.sin(dLon/2)
        var c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a))
        return R * c
    }

    // --- Ephemeral tail trimming ---
    function _hav_m(lat1, lon1, lat2, lon2){
        var R=6378137.0, dLat=_toRad(lat2-lat1), dLon=_toRad(lon2-lon1)
        var a=Math.sin(dLat/2)*Math.sin(dLat/2) + Math.cos(_toRad(lat1))*Math.cos(_toRad(lat2))*Math.sin(dLon/2)*Math.sin(dLon/2)
        return 2*R*Math.atan2(Math.sqrt(a), Math.sqrt(1-a))
    }

    // *** YALNIZCA ZAMANA GÖRE BUDAMA ***
    function _trimTailByTimeOnly() {
        if (!ephemeralTail || trailPoints.length < 2) return
        var now = Date.now()
        var cutoff = now - recentAgeSec*1000

        // Baştan itibaren cutoff'tan eski olanları sil
        var idx = 0
        while (idx < trailPoints.length && trailPoints[idx].ts < cutoff) idx++
        if (idx > 0) trailPoints.splice(0, idx)

        // Emniyet: çok büyürse kes
        while (trailPoints.length > trailMaxPoints) trailPoints.shift()
    }

    function pushTrailPoint(lat, lon) {
        var now = Date.now()
        var L = trailPoints.length
        if (L > 0) {
            var p = trailPoints[L-1]
            var d = _haversine_m(p.lat, p.lon, lat, lon)
            if (d < trailMinStepM) return // çok küçük hareketleri yut
        }
        trailPoints.push({lat: lat, lon: lon, ts: now})

        // Ephemeral tail: yalnızca zamana göre buda
        _trimTailByTimeOnly()

        // Canvas'ı yeniden boya
        if (trailCanvas) {
            trailCanvas.requestPaint()
        }
    }
    
    // ================== DİNAMİK İHA SİSTEMİ ==================
    // Dinamik İHA verileri (takım numarası -> veri objesi)
    property var dynamicIhaData: ({})
    
    // Dinamik İHA marker'ları (takım numarası -> marker objesi)
    property var dynamicIhaMarkers: ({})
    
    // Yarı-dinamik (mevcut) rakip marker'lar için sözlük (takım -> MapQuickItem)
    property var semiRivalMarkers: ({})
    
    // Dinamik İHA Component
    Component { 
        id: dynamicIhaComponent
        MapQuickItem {
            property int teamNumber: -1
            property real lat: 0
            property real lon: 0
            property real yaw: 0
            property real altitude: 0
            property real pitch: 0
            property real roll: 0
            property real speed: 0
            property real timeDiff: 0
            
            coordinate: QtPositioning.coordinate(lat, lon)
            anchorPoint.x: ihaContainer.width/2
            anchorPoint.y: ihaContainer.height/2
            property real markerScale: 1.0
            z: 4
            
            sourceItem: Item {
                id: ihaContainer
                width: ihaImage.width
                height: ihaImage.height
                
                Image {
                    id: ihaImage
                    source: "qrc:/icons/iha.png"
                    width: 18 * markerScale
                    height: 18 * markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: yaw
                }
                
                Rectangle { 
                    width: 14; 
                    height: 14; 
                    color: "#e74c3c"; 
                    radius: 7; 
                    anchors.top: parent.top; 
                    anchors.horizontalCenter: parent.horizontalCenter; 
                    anchors.topMargin: -15
                    Text { 
                        text: "T" + teamNumber; 
                        anchors.centerIn: parent; 
                        color: "white"; 
                        font.bold: true; 
                        font.pixelSize: 8 
                    } 
                }
                
                // Tıklama alanı (dinamik İHA için özel)
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        console.log("Dinamik İHA T" + teamNumber + " tıklandı")
                        showIhaInfo(teamNumber, lat, lon, yaw, altitude, pitch, roll, speed, timeDiff)
                    }
                }
            }
            
            Behavior on coordinate { CoordinateAnimation { duration: 200; easing.type: Easing.Linear } }
            Behavior on rotation { NumberAnimation { duration: 200; easing.type: Easing.Linear } }
        }
    }
    


    // Statik HSS alanlarını güncelleme fonksiyonu (C++'dan çağrılır)
    function updateStaticHssAreas(coordinates) {
        console.log('[Map.qml] updateStaticHssAreas çağrıldı, koordinat sayısı:', coordinates.length)
        
        // İlk 10 koordinatı statik HSS alanlarına uygula
        if (coordinates.length >= 1) {
            // 1. HSS - Gökçeada Havalimanı
            var coord1 = coordinates[0]
            gokceadaHssCircle.center = QtPositioning.coordinate(coord1.lat, coord1.lon)
            gokceadaHssCircle.radius = coord1.radius
            gokceadaHssLabel.coordinate = QtPositioning.coordinate(coord1.lat, coord1.lon)
            gokceadaLabelText.text = "ID: " + coord1.id + "\n" + coord1.radius + "m"
            console.log('[Map.qml] 1. HSS güncellendi:', coord1.lat, coord1.lon, coord1.radius, 'ID:', coord1.id)
        }
        
        if (coordinates.length >= 2) {
            // 2. HSS - Kuzey Liman
            var coord2 = coordinates[1]
            gokceadaKuzeyHssCircle.center = QtPositioning.coordinate(coord2.lat, coord2.lon)
            gokceadaKuzeyHssCircle.radius = coord2.radius
            gokceadaKuzeyHssLabel.coordinate = QtPositioning.coordinate(coord2.lat, coord2.lon)
            gokceadaKuzeyLabelText.text = "ID: " + coord2.id + "\n" + coord2.radius + "m"
            console.log('[Map.qml] 2. HSS güncellendi:', coord2.lat, coord2.lon, coord2.radius, 'ID:', coord2.id)
        }
        
        if (coordinates.length >= 3) {
            // 3. HSS - Güney Sahil
            var coord3 = coordinates[2]
            gokceadaGuneyHssCircle.center = QtPositioning.coordinate(coord3.lat, coord3.lon)
            gokceadaGuneyHssCircle.radius = coord3.radius
            gokceadaGuneyHssLabel.coordinate = QtPositioning.coordinate(coord3.lat, coord3.lon)
            gokceadaGuneyLabelText.text = "ID: " + coord3.id + "\n" + coord3.radius + "m"
            console.log('[Map.qml] 3. HSS güncellendi:', coord3.lat, coord3.lon, coord3.radius, 'ID:', coord3.id)
        }
        
        if (coordinates.length >= 4) {
            // 4. HSS Alanı
            var coord4 = coordinates[3]
            hss4Circle.center = QtPositioning.coordinate(coord4.lat, coord4.lon)
            hss4Circle.radius = coord4.radius
            hss4Label.coordinate = QtPositioning.coordinate(coord4.lat, coord4.lon)
            hss4LabelText.text = "ID: " + coord4.id + "\n" + coord4.radius + "m"
            console.log('[Map.qml] 4. HSS güncellendi:', coord4.lat, coord4.lon, coord4.radius, 'ID:', coord4.id)
        }
        
        if (coordinates.length >= 5) {
            // 5. HSS Alanı
            var coord5 = coordinates[4]
            hss5Circle.center = QtPositioning.coordinate(coord5.lat, coord5.lon)
            hss5Circle.radius = coord5.radius
            hss5Label.coordinate = QtPositioning.coordinate(coord5.lat, coord5.lon)
            hss5LabelText.text = "ID: " + coord5.id + "\n" + coord5.radius + "m"
            console.log('[Map.qml] 5. HSS güncellendi:', coord5.lat, coord5.lon, coord5.radius, 'ID:', coord5.id)
        }
        
        if (coordinates.length >= 6) {
            // 6. HSS Alanı
            var coord6 = coordinates[5]
            hss6Circle.center = QtPositioning.coordinate(coord6.lat, coord6.lon)
            hss6Circle.radius = coord6.radius
            hss6Label.coordinate = QtPositioning.coordinate(coord6.lat, coord6.lon)
            hss6LabelText.text = "ID: " + coord6.id + "\n" + coord6.radius + "m"
            console.log('[Map.qml] 6. HSS güncellendi:', coord6.lat, coord6.lon, coord6.radius, 'ID:', coord6.id)
        }
        
        if (coordinates.length >= 7) {
            // 7. HSS Alanı
            var coord7 = coordinates[6]
            hss7Circle.center = QtPositioning.coordinate(coord7.lat, coord7.lon)
            hss7Circle.radius = coord7.radius
            hss7Label.coordinate = QtPositioning.coordinate(coord7.lat, coord7.lon)
            hss7LabelText.text = "ID: " + coord7.id + "\n" + coord7.radius + "m"
            console.log('[Map.qml] 7. HSS güncellendi:', coord7.lat, coord7.lon, coord7.radius, 'ID:', coord7.id)
        }
        
        if (coordinates.length >= 8) {
            // 8. HSS Alanı
            var coord8 = coordinates[7]
            hss8Circle.center = QtPositioning.coordinate(coord8.lat, coord8.lon)
            hss8Circle.radius = coord8.radius
            hss8Label.coordinate = QtPositioning.coordinate(coord8.lat, coord8.lon)
            hss8LabelText.text = "ID: " + coord8.id + "\n" + coord8.radius + "m"
            console.log('[Map.qml] 8. HSS güncellendi:', coord8.lat, coord8.lon, coord8.radius, 'ID:', coord8.id)
        }
        
        if (coordinates.length >= 9) {
            // 9. HSS Alanı
            var coord9 = coordinates[8]
            hss9Circle.center = QtPositioning.coordinate(coord9.lat, coord9.lon)
            hss9Circle.radius = coord9.radius
            hss9Label.coordinate = QtPositioning.coordinate(coord9.lat, coord9.lon)
            hss9LabelText.text = "ID: " + coord9.id + "\n" + coord9.radius + "m"
            console.log('[Map.qml] 9. HSS güncellendi:', coord9.lat, coord9.lon, coord9.radius, 'ID:', coord9.id)
        }
        
        if (coordinates.length >= 10) {
            // 10. HSS Alanı
            var coord10 = coordinates[9]
            hss10Circle.center = QtPositioning.coordinate(coord10.lat, coord10.lon)
            hss10Circle.radius = coord10.radius
            hss10Label.coordinate = QtPositioning.coordinate(coord10.lat, coord10.lon)
            hss10LabelText.text = "ID: " + coord10.id + "\n" + coord10.radius + "m"
            console.log('[Map.qml] 10. HSS güncellendi:', coord10.lat, coord10.lon, coord10.radius, 'ID:', coord10.id)
        }
        
        if (coordinates.length >= 11) {
            // 11. HSS Alanı
            var coord11 = coordinates[10]
            hss11Circle.center = QtPositioning.coordinate(coord11.lat, coord11.lon)
            hss11Circle.radius = coord11.radius
            hss11Label.coordinate = QtPositioning.coordinate(coord11.lat, coord11.lon)
            hss11LabelText.text = "ID: " + coord11.id + "\n" + coord11.radius + "m"
            console.log('[Map.qml] 11. HSS güncellendi:', coord11.lat, coord11.lon, coord11.radius, 'ID:', coord11.id)
        }
        
        if (coordinates.length >= 12) {
            // 12. HSS Alanı
            var coord12 = coordinates[11]
            hss12Circle.center = QtPositioning.coordinate(coord12.lat, coord12.lon)
            hss12Circle.radius = coord12.radius
            hss12Label.coordinate = QtPositioning.coordinate(coord12.lat, coord12.lon)
            hss12LabelText.text = "ID: " + coord12.id + "\n" + coord12.radius + "m"
            console.log('[Map.qml] 12. HSS güncellendi:', coord12.lat, coord12.lon, coord12.radius, 'ID:', coord12.id)
        }
        
        if (coordinates.length >= 13) {
            // 13. HSS Alanı
            var coord13 = coordinates[12]
            hss13Circle.center = QtPositioning.coordinate(coord13.lat, coord13.lon)
            hss13Circle.radius = coord13.radius
            hss13Label.coordinate = QtPositioning.coordinate(coord13.lat, coord13.lon)
            hss13LabelText.text = "ID: " + coord13.id + "\n" + coord13.radius + "m"
            console.log('[Map.qml] 13. HSS güncellendi:', coord13.lat, coord13.lon, coord13.radius, 'ID:', coord13.id)
        }
        
        if (coordinates.length >= 14) {
            // 14. HSS Alanı
            var coord14 = coordinates[13]
            hss14Circle.center = QtPositioning.coordinate(coord14.lat, coord14.lon)
            hss14Circle.radius = coord14.radius
            hss14Label.coordinate = QtPositioning.coordinate(coord14.lat, coord14.lon)
            hss14LabelText.text = "ID: " + coord14.id + "\n" + coord14.radius + "m"
            console.log('[Map.qml] 14. HSS güncellendi:', coord14.lat, coord14.lon, coord14.radius, 'ID:', coord14.id)
        }
        
        if (coordinates.length >= 15) {
            // 15. HSS Alanı
            var coord15 = coordinates[14]
            hss15Circle.center = QtPositioning.coordinate(coord15.lat, coord15.lon)
            hss15Circle.radius = coord15.radius
            hss15Label.coordinate = QtPositioning.coordinate(coord15.lat, coord15.lon)
            hss15LabelText.text = "ID: " + coord15.id + "\n" + coord15.radius + "m"
            console.log('[Map.qml] 15. HSS güncellendi:', coord15.lat, coord15.lon, coord15.radius, 'ID:', coord15.id)
        }
    }


    // QR koordinatları (sabit)
    property real qrLat: 40.20323
    property real qrLon: 25.88129

    // Dinamik rakip marker listesi (teamNo -> object)
    property var rivalObjects: ({})
    
    // ================== DİNAMİK İHA GÜNCELLEME FONKSİYONLARI ==================
    
    // C++ tarafından çağrılır: tek İHA verisi güncelle
    function updateDynamicIha(teamNo, lat, lon, yaw, altitude, pitch, roll, speed, timeDiff) {
        // Veriyi sakla
        dynamicIhaData[teamNo] = {
            lat: lat,
            lon: lon,
            yaw: yaw,
            altitude: altitude,
            pitch: pitch,
            roll: roll,
            speed: speed,
            timeDiff: timeDiff
        }
        
        // Panel verilerini güncelle (eğer açıksa)
        updatePanelData(teamNo, lat, lon, yaw, altitude, pitch, roll, speed, timeDiff)
        
        // Marker varsa güncelle
        if (dynamicIhaMarkers[teamNo]) {
            var marker = dynamicIhaMarkers[teamNo]
            marker.teamNumber = teamNo  // Takım numarasını güncelle
            marker.lat = lat
            marker.lon = lon
            marker.yaw = yaw
            marker.altitude = altitude
            marker.pitch = pitch
            marker.roll = roll
            marker.speed = speed
            marker.timeDiff = timeDiff
        } else {
            // Yeni marker oluştur
            createDynamicIhaMarker(teamNo)
        }
    }
    
    // Dinamik İHA marker'ı oluştur
    function createDynamicIhaMarker(teamNo) {
        if (!mapView || !dynamicIhaData[teamNo]) return
        
        var data = dynamicIhaData[teamNo]
        var marker = dynamicIhaComponent.createObject(mapView, {
            teamNumber: teamNo,
            lat: data.lat,
            lon: data.lon,
            yaw: data.yaw,
            altitude: data.altitude,
            pitch: data.pitch,
            roll: data.roll,
            speed: data.speed,
            timeDiff: data.timeDiff
        })
        
        if (marker) {
            dynamicIhaMarkers[teamNo] = marker
            console.log('[Map.qml] Dinamik İHA marker oluşturuldu:', teamNo)
        }
    }
    
    // Toplu İHA verisi güncelle (JSON array)
    function updateDynamicIhaBatch(ihaDataArray) {
        console.log('[Map.qml] updateDynamicIhaBatch çağrıldı, İHA sayısı:', ihaDataArray.length)
        
        for (var i = 0; i < ihaDataArray.length; i++) {
            var ihaData = ihaDataArray[i]
            updateDynamicIha(
                ihaData.takim_numarasi,
                ihaData.iha_enlem,
                ihaData.iha_boylam,
                ihaData.iha_yonelme,
                ihaData.iha_irtifa,
                ihaData.iha_dikilme,
                ihaData.iha_yatis,
                ihaData.iha_hizi,
                ihaData.zaman_farki
            )
        }
    }
    
    // Eski updateRival fonksiyonu (geriye uyumluluk için)
    function updateRival(teamNo, lat, lon, yaw){
        console.log('[Map.qml] updateRival çağrıldı, dinamik sisteme yönlendiriliyor:', teamNo)
        updateDynamicIha(teamNo, lat, lon, yaw, 0, 0, 0, 0, 0)
    }

    // Yarı-dinamik mevcut marker'ları güncelle (teamNo 1..14 eşlemesi)
    function updateSemiRival(teamNo, lat, lon, yaw){
        if (!semiRivalMarkers || !semiRivalMarkers[teamNo]) return
        var marker = semiRivalMarkers[teamNo]
        marker.coordinate = QtPositioning.coordinate(lat, lon)
        
        // Yaw değerini görsel olarak uygula - İHA ikonunu döndür
        if (marker.sourceItem) {
            // sourceItem'ın children'larından Image nesnesini bul ve döndür
            for (var i = 0; i < marker.sourceItem.children.length; i++) {
                var child = marker.sourceItem.children[i]
                if (child && child.source && child.source.toString().includes("enemy.png")) {
                    child.rotation = yaw
                    break
                }
            }
        }
        
        // Panel açıksa güncelle (yarı-dinamik akışlarda da panel güncel kalsın)
        updatePanelData(teamNo, lat, lon, yaw, 0, 0, 0, 0, 0)
    }

    // Kuyruklanmış rakip güncellemelerini uygula
    function flushPendingRivals(){
        // Dinamik İHA oluşturma kapalı olduğundan, kuyruğu boşalt
        pendingRivalUpdates = []
    }

    // C++ toplu güncellemeden sonra aktif takım listesine göre marker havuzunu senkronize et
    function syncRivalMarkers(teamNos){
        // Dinamik İHA oluşturma devre dışı; varsa mevcut dinamik marker'ları temizle
        for (var key in rivalObjects) {
            if (rivalObjects[key]) rivalObjects[key].destroy()
            delete rivalObjects[key]
        }
    }

    // İlk rakibe merkezleme bayrağı (başlangıçta kapalı bırak)
    property bool rivalCentered: false

    // Yardımcı: yaw açı normalizasyonu
    function normalizeDeg(deg){
        var n = deg % 360
        if (n < 0) n += 360
        return n
    }

    // --- Sınır kontrolü (kendi İHA için) ---
    function checkBoundaries(lat, lon) {
        if (lat > coords.topLeft.latitude)     lat = coords.topLeft.latitude
        if (lat < coords.bottomRight.latitude) lat = coords.bottomRight.latitude
        if (lon < coords.topLeft.longitude)    lon = coords.topLeft.longitude
        if (lon > coords.bottomRight.longitude)lon = coords.bottomRight.longitude
        return { lat: lat, lon: lon }
    }

    // --- Waypoint basit API (kendi İHA için) ---
    property var waypoints: []
    property int currentWaypointIndex: 0
    property bool waypointMode: false
    function addWaypoint(lat, lon) { waypoints.push(QtPositioning.coordinate(lat, lon)) }
    function clearWaypoints() { waypoints = []; currentWaypointIndex = 0 }

    // --- QR koordinatları API ---
    // QR koordinatları artık C++ tarafından güncelleniyor
    // Bu fonksiyon artık kullanılmıyor

    // --- Trail modeli (kendi İHA izi) ---
    property var trailModel: []

    // Dinamik rakip akışı: updateRival fonksiyonunda doğrudan marker'lar güncellenir

    // ================== HARİTA ==================
    // Harita sağlayıcıları
    Plugin {
        id: osmPlugin
        name: "osm"
        PluginParameter { name: "osm.mapping.providersrepository.disabled"; value: true }
        PluginParameter { name: "osm.mapping.host"; value: "https://tile.openstreetmap.org/{z}/{x}/{y}.png" }
        PluginParameter { name: "osm.mapping.cache.directory"; value: "./osm_cache" }
        PluginParameter { name: "osm.mapping.useragent"; value: "MyQtApp/1.0" }
        PluginParameter { name: "osm.mapping.copyright"; value: "© OpenStreetMap contributors" }
    }

    Map {
        id: mapView
        objectName: "mapView"
        anchors.fill: parent
        plugin: osmPlugin
        zoomLevel: 14  // HSS alanını daha iyi görmek için
        antialiasing: true

        Component.onCompleted: {
            // İHA konumuna merkezle (HSS alanına değil)
            mapView.center = QtPositioning.coordinate(ihaLatLocation, ihaLonLocation)
            console.log('[Map.qml] Map ready. enemy.png test load...')
            // Harita hazır işareti
            window.mapReady = true
            // Bekleyen rakip güncellemelerini uygula
            flushPendingRivals()

            // Mevcut yarı-dinamik marker'ları takım numaralarına eşle
            semiRivalMarkers[1] = extraIhaMarker
            semiRivalMarkers[2] = extraIha2Marker
            semiRivalMarkers[3] = extraIha3Marker
            semiRivalMarkers[4] = extraIha4Marker
            semiRivalMarkers[5] = extraIha5Marker
            semiRivalMarkers[6] = extraIha6Marker
            semiRivalMarkers[7] = extraIha7Marker
            semiRivalMarkers[8] = extraIha8Marker
            semiRivalMarkers[9] = extraIha9Marker
            semiRivalMarkers[10] = extraIha10Marker
            semiRivalMarkers[11] = extraIha11Marker
            semiRivalMarkers[12] = extraIha12Marker
            semiRivalMarkers[13] = extraIha13Marker
            semiRivalMarkers[14] = extraIha14Marker
            semiRivalMarkers[15] = extraIha15Marker
            semiRivalMarkers[16] = extraIha16Marker
            semiRivalMarkers[17] = extraIha17Marker
            semiRivalMarkers[18] = extraIha18Marker
            semiRivalMarkers[19] = extraIha19Marker
            semiRivalMarkers[20] = extraIha20Marker
            semiRivalMarkers[21] = extraIha21Marker
            semiRivalMarkers[22] = extraIha22Marker
            semiRivalMarkers[23] = extraIha23Marker
            semiRivalMarkers[24] = extraIha24Marker
            semiRivalMarkers[25] = extraIha25Marker
            semiRivalMarkers[26] = extraIha26Marker
            semiRivalMarkers[27] = extraIha27Marker
            semiRivalMarkers[28] = extraIha28Marker
            semiRivalMarkers[29] = extraIha29Marker
            semiRivalMarkers[30] = extraIha30Marker
            semiRivalMarkers[31] = extraIha31Marker
            semiRivalMarkers[32] = extraIha32Marker
            semiRivalMarkers[33] = extraIha33Marker
            semiRivalMarkers[34] = extraIha34Marker
            semiRivalMarkers[35] = extraIha35Marker
            semiRivalMarkers[36] = extraIha36Marker
            semiRivalMarkers[37] = extraIha37Marker
            semiRivalMarkers[38] = extraIha38Marker
            semiRivalMarkers[39] = extraIha39Marker
            semiRivalMarkers[40] = extraIha40Marker
            semiRivalMarkers[41] = extraIha41Marker
            semiRivalMarkers[42] = extraIha42Marker
            semiRivalMarkers[43] = extraIha43Marker
        }

        // C++ tarafından çağrılır: Verilen koordinata merkezle ve zoom yap
        function centerOnCoordinate(lat, lon, zoom) {
            mapView.center = QtPositioning.coordinate(lat, lon)
            if (zoom !== undefined && zoom > 0) {
                mapView.zoomLevel = zoom
            }
        }

        // Pan/zoom mouse kontrolleri (senin kodun)
        MouseArea {
            anchors.fill: parent
            property real lastX: 0
            property real lastY: 0
            property var lastCenter: QtPositioning.coordinate(ihaLatLocation, ihaLonLocation)
            onPressed: (mouse)=>{ lastX = mouse.x; lastY = mouse.y; lastCenter = mapView.center }
            onPositionChanged: (mouse)=> {
                if (mouse.buttons & Qt.LeftButton) {
                    var dx = -(mouse.x - lastX);
                    var dy = -(mouse.y - lastY);
                    var metersPerPixel = 156543.03392 * Math.cos(mapView.center.latitude * Math.PI / 180) / Math.pow(2, mapView.zoomLevel);
                    var dLat = -(dy * metersPerPixel) / 111320;
                    var dLon =  (dx * metersPerPixel) / (111320 * Math.cos(mapView.center.latitude * Math.PI / 180));
                    mapView.center = QtPositioning.coordinate(lastCenter.latitude + dLat, lastCenter.longitude + dLon);
                }
            }
            onClicked: (mouse)=> {
                if (waypointMode) {
                    const c = mapView.toCoordinate(Qt.point(mouse.x, mouse.y));
                    window.addWaypoint(c.latitude, c.longitude);
                }
            }
        }
        WheelHandler {
            acceptedDevices: PointerDevice.Mouse
            onWheel: (event) => {
                if (event.angleDelta.y > 0 && mapView.zoomLevel < 20) {
                    mapView.zoomLevel++
                    imageMarker.markerScale *= 1.2
                    // Rakip İHA'ların markerScale değerlerini de güncelle
                    extraIhaMarker.markerScale *= 1.2
                    extraIha2Marker.markerScale *= 1.2
                    extraIha3Marker.markerScale *= 1.2
                    extraIha4Marker.markerScale *= 1.2
                    extraIha5Marker.markerScale *= 1.2
                    extraIha6Marker.markerScale *= 1.2
                    extraIha7Marker.markerScale *= 1.2
                    extraIha8Marker.markerScale *= 1.2
                    extraIha9Marker.markerScale *= 1.2
                    extraIha10Marker.markerScale *= 1.2
                    extraIha11Marker.markerScale *= 1.2
                    extraIha12Marker.markerScale *= 1.2
                    extraIha13Marker.markerScale *= 1.2
                    extraIha14Marker.markerScale *= 1.2
                    extraIha15Marker.markerScale *= 1.2
                    extraIha16Marker.markerScale *= 1.2
                    extraIha17Marker.markerScale *= 1.2
                    extraIha18Marker.markerScale *= 1.2
                    extraIha19Marker.markerScale *= 1.2
                    extraIha20Marker.markerScale *= 1.2
                    extraIha21Marker.markerScale *= 1.2
                    extraIha22Marker.markerScale *= 1.2
                    extraIha23Marker.markerScale *= 1.2
                    extraIha24Marker.markerScale *= 1.2
                    extraIha25Marker.markerScale *= 1.2
                    extraIha26Marker.markerScale *= 1.2
                    extraIha27Marker.markerScale *= 1.2
                    extraIha28Marker.markerScale *= 1.2
                    extraIha29Marker.markerScale *= 1.2
                    extraIha30Marker.markerScale *= 1.2
                    extraIha31Marker.markerScale *= 1.2
                    extraIha32Marker.markerScale *= 1.2
                    extraIha33Marker.markerScale *= 1.2
                    extraIha34Marker.markerScale *= 1.2
                    extraIha35Marker.markerScale *= 1.2
                    extraIha36Marker.markerScale *= 1.2
                    extraIha37Marker.markerScale *= 1.2
                    extraIha38Marker.markerScale *= 1.2
                    extraIha39Marker.markerScale *= 1.2
                    extraIha40Marker.markerScale *= 1.2
                    extraIha41Marker.markerScale *= 1.2
                    extraIha42Marker.markerScale *= 1.2
                    extraIha43Marker.markerScale *= 1.2
                } else if (event.angleDelta.y < 0 && mapView.zoomLevel > 3) {
                    mapView.zoomLevel--
                    imageMarker.markerScale /= 1.2
                    // Rakip İHA'ların markerScale değerlerini de güncelle
                    extraIhaMarker.markerScale /= 1.2
                    extraIha2Marker.markerScale /= 1.2
                    extraIha3Marker.markerScale /= 1.2
                    extraIha4Marker.markerScale /= 1.2
                    extraIha5Marker.markerScale /= 1.2
                    extraIha6Marker.markerScale /= 1.2
                    extraIha7Marker.markerScale /= 1.2
                    extraIha8Marker.markerScale /= 1.2
                    extraIha9Marker.markerScale /= 1.2
                    extraIha10Marker.markerScale /= 1.2
                    extraIha11Marker.markerScale /= 1.2
                    extraIha12Marker.markerScale /= 1.2
                    extraIha13Marker.markerScale /= 1.2
                    extraIha14Marker.markerScale /= 1.2
                    extraIha15Marker.markerScale /= 1.2
                    extraIha16Marker.markerScale /= 1.2
                    extraIha17Marker.markerScale /= 1.2
                    extraIha18Marker.markerScale /= 1.2
                    extraIha19Marker.markerScale /= 1.2
                    extraIha20Marker.markerScale /= 1.2
                    extraIha21Marker.markerScale /= 1.2
                    extraIha22Marker.markerScale /= 1.2
                    extraIha23Marker.markerScale /= 1.2
                    extraIha24Marker.markerScale /= 1.2
                    extraIha25Marker.markerScale /= 1.2
                    extraIha26Marker.markerScale /= 1.2
                    extraIha27Marker.markerScale /= 1.2
                    extraIha28Marker.markerScale /= 1.2
                    extraIha29Marker.markerScale /= 1.2
                    extraIha30Marker.markerScale /= 1.2
                    extraIha31Marker.markerScale /= 1.2
                    extraIha32Marker.markerScale /= 1.2
                    extraIha33Marker.markerScale /= 1.2
                    extraIha34Marker.markerScale /= 1.2
                    extraIha35Marker.markerScale /= 1.2
                    extraIha36Marker.markerScale /= 1.2
                    extraIha37Marker.markerScale /= 1.2
                    extraIha38Marker.markerScale /= 1.2
                    extraIha39Marker.markerScale /= 1.2
                    extraIha40Marker.markerScale /= 1.2
                    extraIha41Marker.markerScale /= 1.2
                    extraIha42Marker.markerScale /= 1.2
                    extraIha43Marker.markerScale /= 1.2
                }
                // Tüm marker'ların ölçeklerini sınırla
                imageMarker.markerScale = Math.min(5.0, Math.max(0.2, imageMarker.markerScale))
                extraIhaMarker.markerScale = Math.min(5.0, Math.max(0.2, extraIhaMarker.markerScale))
                extraIha2Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha2Marker.markerScale))
                extraIha3Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha3Marker.markerScale))
                extraIha4Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha4Marker.markerScale))
                extraIha5Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha5Marker.markerScale))
                extraIha6Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha6Marker.markerScale))
                extraIha7Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha7Marker.markerScale))
                extraIha8Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha8Marker.markerScale))
                extraIha9Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha9Marker.markerScale))
                extraIha10Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha10Marker.markerScale))
                extraIha11Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha11Marker.markerScale))
                extraIha12Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha12Marker.markerScale))
                extraIha13Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha13Marker.markerScale))
                extraIha14Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha14Marker.markerScale))
                extraIha15Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha15Marker.markerScale))
                extraIha16Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha16Marker.markerScale))
                extraIha17Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha17Marker.markerScale))
                extraIha18Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha18Marker.markerScale))
                extraIha19Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha19Marker.markerScale))
                extraIha20Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha20Marker.markerScale))
                extraIha21Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha21Marker.markerScale))
                                    extraIha22Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha22Marker.markerScale))
                extraIha23Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha23Marker.markerScale))
                extraIha24Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha24Marker.markerScale))
                extraIha25Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha25Marker.markerScale))
                extraIha26Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha26Marker.markerScale))
                extraIha27Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha27Marker.markerScale))
                extraIha28Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha28Marker.markerScale))
                extraIha29Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha29Marker.markerScale))
                extraIha30Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha30Marker.markerScale))
                extraIha31Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha31Marker.markerScale))
                extraIha32Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha32Marker.markerScale))
                extraIha33Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha33Marker.markerScale))
                extraIha34Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha34Marker.markerScale))
                extraIha35Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha35Marker.markerScale))
                extraIha36Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha36Marker.markerScale))
                extraIha37Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha37Marker.markerScale))
                extraIha38Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha38Marker.markerScale))
                extraIha39Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha39Marker.markerScale))
                                    extraIha40Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha40Marker.markerScale))
                extraIha41Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha41Marker.markerScale))
                extraIha42Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha42Marker.markerScale))
                extraIha43Marker.markerScale = Math.min(5.0, Math.max(0.2, extraIha43Marker.markerScale))
                event.accepted = true
            }
        }




        
        // ÖRNEK HSS: Gökçeada Havalimanı (kapatıldı)
        // Dinamik HSS Component'leri kök scope'a taşındı (çift tanımı kaldırıldı)

        // ================== TRAIL ÇİZİM SİSTEMİ ==================
        // Kendi İHA izi (eski sistem - geriye uyumluluk için)
        MapPolyline { id: trailLine; line.width: 4; line.color: "magenta"; path: trailModel }
        Timer {
            id: trailSampler
            interval: 300; running: true; repeat: true
            onTriggered: {
                if (trailModel.length > 1) {
                    trailModel = [ trailModel[trailModel.length-1] ]
                    trailLine.path = trailModel.slice()
                }
                const live = QtPositioning.coordinate(imageMarker.coordinate.latitude, imageMarker.coordinate.longitude)
                const eps = 1e-5
                if (trailModel.length === 0 ||
                    Math.abs(trailModel[trailModel.length-1].latitude  - live.latitude ) > eps ||
                    Math.abs(trailModel[trailModel.length-1].longitude - live.longitude) > eps) {
                    trailModel.push(live)
                    if (trailModel.length > 20) trailModel.shift()
                    trailLine.path = trailModel.slice()
                }
            }
        }

        // ================== İZ ÇİZİM KATMANI (Canvas) - 3 KATMANLI ==================
        Canvas {
            id: trailCanvas
            anchors.fill: parent
            antialiasing: true
            z: 3    // İHA ikonunun arkasında kalsın

            // Ayarlar
            property real recentLenMeters: 700      // son 700 m düz (magenta)
            property real headingLenMeters: 150     // burun vektörü uzunluğu
            property real lineWidthPx: 3.0
            property color colRecent: window.colRecent   // takip rengi (üst scope)
            property color colOlder:  "#FFD400"     // sarı (Mission Planner vibe)
            property real dotLenPx: 8.0
            property real gapPx: 6.0
            property int  maxDrawSegments: 2000     // güvenlik

            onPaint: {
                var ctx = getContext("2d")
                // *** EKRANI HER SEFERİNDE TEMİZLE ***
                ctx.clearRect(0, 0, width, height)
                ctx.globalCompositeOperation = "source-over"
                ctx.lineCap = "round"
                ctx.lineJoin = "round"

                // Yeterli nokta yoksa çık
                if (!trailPoints || trailPoints.length < 2) return

                var now = Date.now()
                // İnce çizgi: zoom'dan neredeyse bağımsız
                var zoomFactor = Math.pow(Math.max(1.0, parent.zoomLevel/8.0), 0.15)
                var lw = Math.max(1.2, lineWidthPx * zoomFactor)

                // --- Yardımcılar ---
                function toPx(lat, lon) { return parent.fromCoordinate(QtPositioning.coordinate(lat, lon)) }
                function hav(lat1, lon1, lat2, lon2) {
                    var R=6378137.0, dLat=(lat2-lat1)*Math.PI/180, dLon=(lon2-lon1)*Math.PI/180
                    var a=Math.sin(dLat/2)**2+Math.cos(lat1*Math.PI/180)*Math.cos(lat2*Math.PI/180)*Math.sin(dLon/2)**2
                    return 2*R*Math.atan2(Math.sqrt(a),Math.sqrt(1-a))
                }

                // --- 1) Heading vektörü (magenta) ---
                // ihaLatLocation / ihaLonLocation / ihaYaw mevcut
                if (typeof ihaLatLocation === "number" && typeof ihaLonLocation === "number" && typeof ihaYaw === "number") {
                    var lat0 = ihaLatLocation, lon0 = ihaLonLocation
                    var brng = Math.PI/180.0 * ihaYaw
                    var d = headingLenMeters / 6378137.0
                    var lat1 = Math.asin(Math.sin(lat0*Math.PI/180)*Math.cos(d) + Math.cos(lat0*Math.PI/180)*Math.sin(d)*Math.cos(brng)) * 180/Math.PI
                    var lon1 = lon0 + Math.atan2(Math.sin(brng)*Math.sin(d)*Math.cos(lat0*Math.PI/180),
                                                  Math.cos(d)-Math.sin(lat0*Math.PI/180)*Math.sin(lat1*Math.PI/180)) * 180/Math.PI
                    var p0 = toPx(lat0, lon0)
                    var p1 = toPx(lat1, lon1)
                    ctx.strokeStyle = colRecent
                    ctx.lineWidth = lw
                    ctx.globalAlpha = 0.35
                    ctx.beginPath(); ctx.moveTo(p0.x, p0.y); ctx.lineTo(p1.x, p1.y); ctx.stroke()
                }

                // --- SADECE mevcut kısa kuyruğu çiz (ephemeral) ---
                if (!trailPoints || trailPoints.length < 2) return
                var now2 = Date.now()
                ctx.strokeStyle = colRecent
                ctx.lineWidth = lw
                ctx.globalAlpha = 0.35
                for (var i2 = 0; i2 < trailPoints.length - 1; i2++) {
                    var r1 = trailPoints[i2], r2 = trailPoints[i2+1]
                    // Sabit opaklık (fade-out kapalı)
                    ctx.globalAlpha = 0.35
                    var s1 = toPx(r1.lat, r1.lon), s2 = toPx(r2.lat, r2.lon)
                    ctx.beginPath(); ctx.moveTo(s1.x, s1.y); ctx.lineTo(s2.x, s2.y); ctx.stroke()
                }
                ctx.globalAlpha = 1.0

                // alpha'yı sıfırla
                ctx.globalAlpha = 1.0
            }

            // Harita hareket/zoom'da yeniden boya
            Connections {
                target: parent
                function onCenterChanged() { trailCanvas.requestPaint() }
                function onZoomLevelChanged() { trailCanvas.requestPaint() }
                function onBearingChanged() { trailCanvas.requestPaint() }
                function onTiltChanged() { trailCanvas.requestPaint() }
            }
        }

        // Kendi İHA
        MapQuickItem {
            id: imageMarker
            coordinate: QtPositioning.coordinate(ihaLatLocation, ihaLonLocation)
            anchorPoint.x: markerImage.width/2
            anchorPoint.y: markerImage.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                width: markerImage.width; height: markerImage.height
                Image {
                    id: markerImage
                    source: "qrc:/icons/iha.png"
                    width: 18 * imageMarker.markerScale
                    height: 18 * imageMarker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: window.ihaYaw
                }
                
                // Tıklama alanı (kendi İHA için özel)
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        console.log("Kendi İHA tıklandı")
                        showIhaInfo(0, ihaLatLocation, ihaLonLocation, ihaYaw, ihaAltitude, 0, 0, ihaSpeed, 0)
                    }
                }
            }
            Behavior on coordinate { CoordinateAnimation { duration: 200; easing.type: Easing.Linear } }
        }

        

        

        // QR marker (dinamik konum)
        MapQuickItem {
            id: qrMarker
            coordinate: QtPositioning.coordinate(qrLat, qrLon)
            anchorPoint.x: qrImage.width/2
            anchorPoint.y: qrImage.height/2
            z: 5

            sourceItem: Item {
                width: qrImage.width
                height: qrImage.height

                Image {
                    id: qrImage
                    source: "qrc:/icons/qr.png"
                    width: 32
                    height: 32
                    smooth: true
                    anchors.centerIn: parent
                }
            }
        }

        // (Dinamik marker component kök scope'a taşındı)



        // ====== GÖKÇEADA HAVALİMANI SINIR ALANI ======
        // Gökçeada Havalimanı çevresinde 4 noktalı sınır alanı (daha büyük)
        MapPolygon {
            id: gokceadaBoundary
            color: Qt.rgba(0, 0.5, 0, 0.3) // Yarı saydam yeşil
            border.color: "#00ff00" // Parlak yeşil kenarlık
            border.width: 3
            z: 99
            
            // Gökçeada Havalimanı çevresinde 4 nokta (daha büyük alan)
            path: [
                QtPositioning.coordinate(40.19891, 25.88131),
                QtPositioning.coordinate(40.20009, 25.87654),
                QtPositioning.coordinate(40.20727, 25.87931),
                QtPositioning.coordinate(40.20612, 25.88425)
            ]
        }
        
        // Sınır noktalarını işaretle
        MapQuickItem {
            id: boundaryPoint1
            coordinate: QtPositioning.coordinate(40.19891, 25.88131)
            anchorPoint.x: boundaryMarker1.width / 2
            anchorPoint.y: boundaryMarker1.height / 2
            z: 100
            sourceItem: Rectangle {
                id: boundaryMarker1
                width: 12
                height: 12
                color: "#00ff00"
                border.color: "#ffffff"
                border.width: 2
                radius: 6
            }
        }
        
        MapQuickItem {
            id: boundaryPoint2
            coordinate: QtPositioning.coordinate(40.20009, 25.87654)
            anchorPoint.x: boundaryMarker2.width / 2
            anchorPoint.y: boundaryMarker2.height / 2
            z: 100
            sourceItem: Rectangle {
                id: boundaryMarker2
                width: 12
                height: 12
                color: "#00ff00"
                border.color: "#ffffff"
                border.width: 2
                radius: 6
            }
        }
        
        MapQuickItem {
            id: boundaryPoint3
            coordinate: QtPositioning.coordinate(40.20727, 25.87931)
            anchorPoint.x: boundaryMarker3.width / 2
            anchorPoint.y: boundaryMarker3.height / 2
            z: 100
            sourceItem: Rectangle {
                id: boundaryMarker3
                width: 12
                height: 12
                color: "#00ff00"
                border.color: "#ffffff"
                border.width: 2
                radius: 6
            }
        }
        
        MapQuickItem {
            id: boundaryPoint4
            coordinate: QtPositioning.coordinate(40.20612, 25.88425)
            anchorPoint.x: boundaryMarker4.width / 2
            anchorPoint.y: boundaryMarker4.height / 2
            z: 100
            sourceItem: Rectangle {
                id: boundaryMarker4
                width: 12
                height: 12
                color: "#00ff00"
                border.color: "#ffffff"
                border.width: 2
                radius: 6
            }
        }

        // ====== GÖKÇEADA HSS ALANLARI (Statik) ======
        // Gökçeada Havalimanı HSS Alanı
        MapCircle {
            id: gokceadaHssCircle
            center: QtPositioning.coordinate(29.329423, 2.875411) // Yeni HSS koordinatları
            radius: 500 // 500 metre yarıçap
            color: "#ff0000" // Kırmızı renk
            opacity: 0.3
            border.color: "#cc0000"
            border.width: 3
            z: 100
            visible: true
        }
        
        // Gökçeada HSS Etiketi
        MapQuickItem {
            id: gokceadaHssLabel
            coordinate: QtPositioning.coordinate(29.329423, 2.875411)
            anchorPoint.x: gokceadaLabelRect.width/2
            anchorPoint.y: gokceadaLabelRect.height + 8
            z: 101
            sourceItem: Rectangle {
                id: gokceadaLabelRect
                color: "#ff0000"
                opacity: 0.95
                radius: 3
                border.color: "#cc0000"
                border.width: 2
                width: gokceadaLabelText.implicitWidth + 10
                height: gokceadaLabelText.implicitHeight + 6
                Text {
                    id: gokceadaLabelText
                    text: "500m"
                    color: "white"
                    font.bold: true
                    font.pixelSize: 11
                    anchors.centerIn: parent
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        // Gökçeada Kuzey Liman HSS Alanı (2. HSS)
        MapCircle {
            id: gokceadaKuzeyHssCircle
            center: QtPositioning.coordinate(29.329423, 2.875411) // Yeni HSS koordinatları
            radius: 300 // 300 metre yarıçap (daha küçük)
            color: "#ff0000" // Kırmızı renk
            opacity: 0.3
            border.color: "#cc0000"
            border.width: 3
            z: 100
            visible: true
        }
        
        // Gökçeada Kuzey HSS Etiketi
        MapQuickItem {
            id: gokceadaKuzeyHssLabel
            coordinate: QtPositioning.coordinate(29.329423, 2.875411)
            anchorPoint.x: gokceadaKuzeyLabelRect.width/2
            anchorPoint.y: gokceadaKuzeyLabelRect.height + 8
            z: 101
            sourceItem: Rectangle {
                id: gokceadaKuzeyLabelRect
                color: "#ff0000"
                opacity: 0.95
                radius: 3
                border.color: "#cc0000"
                border.width: 2
                width: gokceadaKuzeyLabelText.implicitWidth + 10
                height: gokceadaKuzeyLabelText.implicitHeight + 6
                Text {
                    id: gokceadaKuzeyLabelText
                    text: "300m"
                    color: "white"
                    font.bold: true
                    font.pixelSize: 11
                    anchors.centerIn: parent
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        // Gökçeada Güney Sahil HSS Alanı (3. HSS)
        MapCircle {
            id: gokceadaGuneyHssCircle
            center: QtPositioning.coordinate(29.329423, 2.875411) // Yeni HSS koordinatları
            radius: 800 // 800 metre yarıçap (daha büyük)
            color: "#ff0000" // Kırmızı renk
            opacity: 0.3
            border.color: "#cc0000"
            border.width: 3
            z: 100
            visible: true
        }
        
        // Gökçeada Güney HSS Etiketi
        MapQuickItem {
            id: gokceadaGuneyHssLabel
            coordinate: QtPositioning.coordinate(29.329423, 2.875411)
            anchorPoint.x: gokceadaGuneyLabelRect.width/2
            anchorPoint.y: gokceadaGuneyLabelRect.height + 8
            z: 101
            sourceItem: Rectangle {
                id: gokceadaGuneyLabelRect
                color: "#ff0000"
                opacity: 0.95
                radius: 3
                border.color: "#cc0000"
                border.width: 2
                width: gokceadaGuneyLabelText.implicitWidth + 10
                height: gokceadaGuneyLabelText.implicitHeight + 6
                Text {
                    id: gokceadaGuneyLabelText
                    text: "800m"
                    color: "white"
                    font.bold: true
                    font.pixelSize: 11
                    anchors.centerIn: parent
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        // ====== EK HSS ALANLARI (4-10) ======
        // 4. HSS Alanı
        MapCircle {
            id: hss4Circle
            center: QtPositioning.coordinate(29.329423, 2.875411)
            radius: 400
            color: "#ff0000"
            opacity: 0.3
            border.color: "#cc0000"
            border.width: 3
            z: 100
            visible: true
        }
        
        MapQuickItem {
            id: hss4Label
            coordinate: QtPositioning.coordinate(29.329423, 2.875411)
            anchorPoint.x: hss4LabelRect.width/2
            anchorPoint.y: hss4LabelRect.height + 8
            z: 101
            sourceItem: Rectangle {
                id: hss4LabelRect
                color: "#ff0000"
                opacity: 0.95
                radius: 3
                border.color: "#cc0000"
                border.width: 2
                width: hss4LabelText.implicitWidth + 10
                height: hss4LabelText.implicitHeight + 6
                Text {
                    id: hss4LabelText
                    text: "400m"
                    color: "white"
                    font.bold: true
                    font.pixelSize: 11
                    anchors.centerIn: parent
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        // 5. HSS Alanı
        MapCircle {
            id: hss5Circle
            center: QtPositioning.coordinate(29.329423, 2.875411)
            radius: 600
            color: "#ff0000"
            opacity: 0.3
            border.color: "#cc0000"
            border.width: 3
            z: 100
            visible: true
        }
        
        MapQuickItem {
            id: hss5Label
            coordinate: QtPositioning.coordinate(29.329423, 2.875411)
            anchorPoint.x: hss5LabelRect.width/2
            anchorPoint.y: hss5LabelRect.height + 8
            z: 101
            sourceItem: Rectangle {
                id: hss5LabelRect
                color: "#ff0000"
                opacity: 0.95
                radius: 3
                border.color: "#cc0000"
                border.width: 2
                width: hss5LabelText.implicitWidth + 10
                height: hss5LabelText.implicitHeight + 6
                Text {
                    id: hss5LabelText
                    text: "600m"
                    color: "white"
                    font.bold: true
                    font.pixelSize: 11
                    anchors.centerIn: parent
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        // 6. HSS Alanı
        MapCircle {
            id: hss6Circle
            center: QtPositioning.coordinate(29.329423, 2.875411)
            radius: 350
            color: "#ff0000"
            opacity: 0.3
            border.color: "#cc0000"
            border.width: 3
            z: 100
            visible: true
        }
        
        MapQuickItem {
            id: hss6Label
            coordinate: QtPositioning.coordinate(29.329423, 2.875411)
            anchorPoint.x: hss6LabelRect.width/2
            anchorPoint.y: hss6LabelRect.height + 8
            z: 101
            sourceItem: Rectangle {
                id: hss6LabelRect
                color: "#ff0000"
                opacity: 0.95
                radius: 3
                border.color: "#cc0000"
                border.width: 2
                width: hss6LabelText.implicitWidth + 10
                height: hss6LabelText.implicitHeight + 6
                Text {
                    id: hss6LabelText
                    text: "350m"
                    color: "white"
                    font.bold: true
                    font.pixelSize: 11
                    anchors.centerIn: parent
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        // 7. HSS Alanı
        MapCircle {
            id: hss7Circle
            center: QtPositioning.coordinate(29.329423, 2.875411)
            radius: 450
            color: "#ff0000"
            opacity: 0.3
            border.color: "#cc0000"
            border.width: 3
            z: 100
            visible: true
        }
        
        MapQuickItem {
            id: hss7Label
            coordinate: QtPositioning.coordinate(29.329423, 2.875411)
            anchorPoint.x: hss7LabelRect.width/2
            anchorPoint.y: hss7LabelRect.height + 8
            z: 101
            sourceItem: Rectangle {
                id: hss7LabelRect
                color: "#ff0000"
                opacity: 0.95
                radius: 3
                border.color: "#cc0000"
                border.width: 2
                width: hss7LabelText.implicitWidth + 10
                height: hss7LabelText.implicitHeight + 6
                Text {
                    id: hss7LabelText
                    text: "450m"
                    color: "white"
                    font.bold: true
                    font.pixelSize: 11
                    anchors.centerIn: parent
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        // 8. HSS Alanı
        MapCircle {
            id: hss8Circle
            center: QtPositioning.coordinate(29.329423, 2.875411)
            radius: 550
            color: "#ff0000"
            opacity: 0.3
            border.color: "#cc0000"
            border.width: 3
            z: 100
            visible: true
        }
        
        MapQuickItem {
            id: hss8Label
            coordinate: QtPositioning.coordinate(29.329423, 2.875411)
            anchorPoint.x: hss8LabelRect.width/2
            anchorPoint.y: hss8LabelRect.height + 8
            z: 101
            sourceItem: Rectangle {
                id: hss8LabelRect
                color: "#ff0000"
                opacity: 0.95
                radius: 3
                border.color: "#cc0000"
                border.width: 2
                width: hss8LabelText.implicitWidth + 10
                height: hss8LabelText.implicitHeight + 6
                Text {
                    id: hss8LabelText
                    text: "550m"
                    color: "white"
                    font.bold: true
                    font.pixelSize: 11
                    anchors.centerIn: parent
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        // 9. HSS Alanı
        MapCircle {
            id: hss9Circle
            center: QtPositioning.coordinate(29.329423, 2.875411)
            radius: 700
            color: "#ff0000"
            opacity: 0.3
            border.color: "#cc0000"
            border.width: 3
            z: 100
            visible: true
        }
        
        MapQuickItem {
            id: hss9Label
            coordinate: QtPositioning.coordinate(29.329423, 2.875411)
            anchorPoint.x: hss9LabelRect.width/2
            anchorPoint.y: hss9LabelRect.height + 8
            z: 101
            sourceItem: Rectangle {
                id: hss9LabelRect
                color: "#ff0000"
                opacity: 0.95
                radius: 3
                border.color: "#cc0000"
                border.width: 2
                width: hss9LabelText.implicitWidth + 10
                height: hss9LabelText.implicitHeight + 6
                Text {
                    id: hss9LabelText
                    text: "700m"
                    color: "white"
                    font.bold: true
                    font.pixelSize: 11
                    anchors.centerIn: parent
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        // 10. HSS Alanı
        MapCircle {
            id: hss10Circle
            center: QtPositioning.coordinate(29.329423, 2.875411)
            radius: 900
            color: "#ff0000"
            opacity: 0.3
            border.color: "#cc0000"
            border.width: 3
            z: 100
            visible: true
        }
        
        MapQuickItem {
            id: hss10Label
            coordinate: QtPositioning.coordinate(29.329423, 2.875411)
            anchorPoint.x: hss10LabelRect.width/2
            anchorPoint.y: hss10LabelRect.height + 8
            z: 101
            sourceItem: Rectangle {
                id: hss10LabelRect
                color: "#ff0000"
                opacity: 0.95
                radius: 3
                border.color: "#cc0000"
                border.width: 2
                width: hss10LabelText.implicitWidth + 10
                height: hss10LabelText.implicitHeight + 6
                Text {
                    id: hss10LabelText
                    text: "900m"
                    color: "white"
                    font.bold: true
                    font.pixelSize: 11
                    anchors.centerIn: parent
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        // 11. HSS Alanı
        MapCircle {
            id: hss11Circle
            center: QtPositioning.coordinate(29.329423, 2.875411)
            radius: 650
            color: "#ff0000"
            opacity: 0.3
            border.color: "#cc0000"
            border.width: 3
            z: 100
            visible: true
        }
        
        MapQuickItem {
            id: hss11Label
            coordinate: QtPositioning.coordinate(29.329423, 2.875411)
            anchorPoint.x: hss11LabelRect.width/2
            anchorPoint.y: hss11LabelRect.height + 8
            z: 101
            sourceItem: Rectangle {
                id: hss11LabelRect
                color: "#ff0000"
                opacity: 0.95
                radius: 3
                border.color: "#cc0000"
                border.width: 2
                width: hss11LabelText.implicitWidth + 10
                height: hss11LabelText.implicitHeight + 6
                Text {
                    id: hss11LabelText
                    text: "650m"
                    color: "white"
                    font.bold: true
                    font.pixelSize: 11
                    anchors.centerIn: parent
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        // 12. HSS Alanı
        MapCircle {
            id: hss12Circle
            center: QtPositioning.coordinate(29.329423, 2.875411)
            radius: 750
            color: "#ff0000"
            opacity: 0.3
            border.color: "#cc0000"
            border.width: 3
            z: 100
            visible: true
        }
        
        MapQuickItem {
            id: hss12Label
            coordinate: QtPositioning.coordinate(29.329423, 2.875411)
            anchorPoint.x: hss12LabelRect.width/2
            anchorPoint.y: hss12LabelRect.height + 8
            z: 101
            sourceItem: Rectangle {
                id: hss12LabelRect
                color: "#ff0000"
                opacity: 0.95
                radius: 3
                border.color: "#cc0000"
                border.width: 2
                width: hss12LabelText.implicitWidth + 10
                height: hss12LabelText.implicitHeight + 6
                Text {
                    id: hss12LabelText
                    text: "750m"
                    color: "white"
                    font.bold: true
                    font.pixelSize: 11
                    anchors.centerIn: parent
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        // 13. HSS Alanı
        MapCircle {
            id: hss13Circle
            center: QtPositioning.coordinate(29.329423, 2.875411)
            radius: 850
            color: "#ff0000"
            opacity: 0.3
            border.color: "#cc0000"
            border.width: 3
            z: 100
            visible: true
        }
        
        MapQuickItem {
            id: hss13Label
            coordinate: QtPositioning.coordinate(29.329423, 2.875411)
            anchorPoint.x: hss13LabelRect.width/2
            anchorPoint.y: hss13LabelRect.height + 8
            z: 101
            sourceItem: Rectangle {
                id: hss13LabelRect
                color: "#ff0000"
                opacity: 0.95
                radius: 3
                border.color: "#cc0000"
                border.width: 2
                width: hss13LabelText.implicitWidth + 10
                height: hss13LabelText.implicitHeight + 6
                Text {
                    id: hss13LabelText
                    text: "850m"
                    color: "white"
                    font.bold: true
                    font.pixelSize: 11
                    anchors.centerIn: parent
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        // 14. HSS Alanı
        MapCircle {
            id: hss14Circle
            center: QtPositioning.coordinate(29.329423, 2.875411)
            radius: 950
            color: "#ff0000"
            opacity: 0.3
            border.color: "#cc0000"
            border.width: 3
            z: 100
            visible: true
        }
        
        MapQuickItem {
            id: hss14Label
            coordinate: QtPositioning.coordinate(29.329423, 2.875411)
            anchorPoint.x: hss14LabelRect.width/2
            anchorPoint.y: hss14LabelRect.height + 8
            z: 101
            sourceItem: Rectangle {
                id: hss14LabelRect
                color: "#ff0000"
                opacity: 0.95
                radius: 3
                border.color: "#cc0000"
                border.width: 2
                width: hss14LabelText.implicitWidth + 10
                height: hss14LabelText.implicitHeight + 6
                Text {
                    id: hss14LabelText
                    text: "950m"
                    color: "white"
                    font.bold: true
                    font.pixelSize: 11
                    anchors.centerIn: parent
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        // 15. HSS Alanı
        MapCircle {
            id: hss15Circle
            center: QtPositioning.coordinate(29.329423, 2.875411)
            radius: 1000
            color: "#ff0000"
            opacity: 0.3
            border.color: "#cc0000"
            border.width: 3
            z: 100
            visible: true
        }
        
        MapQuickItem {
            id: hss15Label
            coordinate: QtPositioning.coordinate(29.329423, 2.875411)
            anchorPoint.x: hss15LabelRect.width/2
            anchorPoint.y: hss15LabelRect.height + 8
            z: 101
            sourceItem: Rectangle {
                id: hss15LabelRect
                color: "#ff0000"
                opacity: 0.95
                radius: 3
                border.color: "#cc0000"
                border.width: 2
                width: hss15LabelText.implicitWidth + 10
                height: hss15LabelText.implicitHeight + 6
                Text {
                    id: hss15LabelText
                    text: "1000m"
                    color: "white"
                    font.bold: true
                    font.pixelSize: 11
                    anchors.centerIn: parent
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        // ====== STATİK İHA MARKER'LARI (Dinamik veri gelince gizlenir) ======
        // ======İHA1======
        MapQuickItem {
            id: extraIhaMarker
            coordinate: QtPositioning.coordinate(27.548676, 6.828966)
            anchorPoint.x: extraIhaContainer.width/2
            anchorPoint.y: extraIhaContainer.height/2
            property real markerScale: 1.0
            z: 4
            visible: !dynamicIhaMarkers[1] // Dinamik veri gelince gizle
            sourceItem: Item {
                id: extraIhaContainer
                width: extraIhaImage.width
                height: extraIhaImage.height
                
                Image {
                    id: extraIhaImage
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIhaMarker.markerScale
                    height: 18 * extraIhaMarker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0  // Sabit yön
                }
                
                Rectangle { 
                    width: 14; 
                    height: 14; 
                    color: "#e74c3c"; 
                    radius: 7; 
                    anchors.top: parent.top; 
                    anchors.horizontalCenter: parent.horizontalCenter; 
                    anchors.topMargin: -15
                    Text { 
                        text: "T1"; 
                        anchors.centerIn: parent; 
                        color: "white"; 
                        font.bold: true; 
                        font.pixelSize: 8 
                    } 
                }
                
                // Tıklama alanı merkezi fonksiyon ile eklenecek
            }
        }

        // ======İHA2======
        MapQuickItem {
            id: extraIha2Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.829966) // T1'in x ekseninde bir yanı
            anchorPoint.x: extraIha2Container.width/2
            anchorPoint.y: extraIha2Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha2Container
                width: extraIha2Image.width
                height: extraIha2Image.height
                
                Image {
                    id: extraIha2Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha2Marker.markerScale
                    height: 18 * extraIha2Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0  // Sabit yön
                }
                
                Rectangle { 
                    width: 14; 
                    height: 14; 
                    color: "#e74c3c"; 
                    radius: 7; 
                    anchors.top: parent.top; 
                    anchors.horizontalCenter: parent.horizontalCenter; 
                    anchors.topMargin: -15
                    Text { 
                        text: "T2"; 
                        anchors.centerIn: parent; 
                        color: "white"; 
                        font.bold: true; 
                        font.pixelSize: 8 
                    } 
                }
                
                // Tıklama alanı merkezi fonksiyon ile eklenecek
            }
        }

        // ======İHA3======
        MapQuickItem {
            id: extraIha3Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.830966)
            anchorPoint.x: extraIha3Container.width/2
            anchorPoint.y: extraIha3Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha3Container
                width: extraIha3Image.width
                height: extraIha3Image.height
                
                Image {
                    id: extraIha3Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha3Marker.markerScale
                    height: 18 * extraIha3Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T3"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
                
                // Tıklama alanı merkezi fonksiyon ile eklenecek
            }
        }

        // ======İHA4======
        MapQuickItem {
            id: extraIha4Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.831966)
            anchorPoint.x: extraIha4Container.width/2
            anchorPoint.y: extraIha4Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha4Container
                width: extraIha4Image.width
                height: extraIha4Image.height
                
                Image {
                    id: extraIha4Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha4Marker.markerScale
                    height: 18 * extraIha4Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T4"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
                
                // Tıklama alanı merkezi fonksiyon ile eklenecek
            }
        }

        // ======İHA5======
        MapQuickItem {
            id: extraIha5Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.832966)
            anchorPoint.x: extraIha5Container.width/2
            anchorPoint.y: extraIha5Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha5Container
                width: extraIha5Image.width
                height: extraIha5Image.height
                
                Image {
                    id: extraIha5Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha5Marker.markerScale
                    height: 18 * extraIha5Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T5"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
                
                // Tıklama alanı merkezi fonksiyon ile eklenecek
            }
        }

        // ======İHA6======
        MapQuickItem {
            id: extraIha6Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.833966)
            anchorPoint.x: extraIha6Container.width/2
            anchorPoint.y: extraIha6Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha6Container
                width: extraIha6Image.width
                height: extraIha6Image.height
                
                Image {
                    id: extraIha6Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha6Marker.markerScale
                    height: 18 * extraIha6Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T6"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
                
                // Tıklama alanı merkezi fonksiyon ile eklenecek
                
                // Tıklama alanı merkezi fonksiyon ile eklenecek
            }
        }

        // ======İHA7======
        MapQuickItem {
            id: extraIha7Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.834966)
            anchorPoint.x: extraIha7Container.width/2
            anchorPoint.y: extraIha7Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha7Container
                width: extraIha7Image.width
                height: extraIha7Image.height
                
                Image {
                    id: extraIha7Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha7Marker.markerScale
                    height: 18 * extraIha7Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T7"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
                
                // Tıklama alanı merkezi fonksiyon ile eklenecek
            }
        }

        // ======İHA8======
        MapQuickItem {
            id: extraIha8Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.835966)
            anchorPoint.x: extraIha8Container.width/2
            anchorPoint.y: extraIha8Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha8Container
                width: extraIha8Image.width
                height: extraIha8Image.height
                
                Image {
                    id: extraIha8Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha8Marker.markerScale
                    height: 18 * extraIha8Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T8"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
                
                // Tıklama alanı merkezi fonksiyon ile eklenecek
            }
        }

        // ======İHA9======
        MapQuickItem {
            id: extraIha9Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.836966)
            anchorPoint.x: extraIha9Container.width/2
            anchorPoint.y: extraIha9Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha9Container
                width: extraIha9Image.width
                height: extraIha9Image.height
                
                Image {
                    id: extraIha9Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha9Marker.markerScale
                    height: 18 * extraIha9Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T9"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
                
                // Tıklama alanı merkezi fonksiyon ile eklenecek
            }
        }

        // ======İHA10======
        MapQuickItem {
            id: extraIha10Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.837966)
            anchorPoint.x: extraIha10Container.width/2
            anchorPoint.y: extraIha10Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha10Container
                width: extraIha10Image.width
                height: extraIha10Image.height
                
                Image {
                    id: extraIha10Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha10Marker.markerScale
                    height: 18 * extraIha10Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T10"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
                
                // Tıklama alanı merkezi fonksiyon ile eklenecek
            }
        }

        // ======İHA11======
        MapQuickItem {
            id: extraIha11Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.838966)
            anchorPoint.x: extraIha11Container.width/2
            anchorPoint.y: extraIha11Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha11Container
                width: extraIha11Image.width
                height: extraIha11Image.height
                
                Image {
                    id: extraIha11Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha11Marker.markerScale
                    height: 18 * extraIha11Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T11"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
                
                // Tıklama alanı merkezi fonksiyon ile eklenecek
            }
        }

        // ======İHA12======
        MapQuickItem {
            id: extraIha12Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.839966)
            anchorPoint.x: extraIha12Container.width/2
            anchorPoint.y: extraIha12Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha12Container
                width: extraIha12Image.width
                height: extraIha12Image.height
                
                Image {
                    id: extraIha12Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha12Marker.markerScale
                    height: 18 * extraIha12Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T12"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
                
                // Tıklama alanı merkezi fonksiyon ile eklenecek
            }
        }

        // ======İHA13======
        MapQuickItem {
            id: extraIha13Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.840966)
            anchorPoint.x: extraIha13Container.width/2
            anchorPoint.y: extraIha13Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha13Container
                width: extraIha13Image.width
                height: extraIha13Image.height
                
                Image {
                    id: extraIha13Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha13Marker.markerScale
                    height: 18 * extraIha13Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T13"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
                
                // Tıklama alanı merkezi fonksiyon ile eklenecek
            }
        }

        // ======İHA14======
        MapQuickItem {
            id: extraIha14Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.841966)
            anchorPoint.x: extraIha14Container.width/2
            anchorPoint.y: extraIha14Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha14Container
                width: extraIha14Image.width
                height: extraIha14Image.height
                
                Image {
                    id: extraIha14Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha14Marker.markerScale
                    height: 18 * extraIha14Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T14"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA15======
        MapQuickItem {
            id: extraIha15Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.842966)
            anchorPoint.x: extraIha15Container.width/2
            anchorPoint.y: extraIha15Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha15Container
                width: extraIha15Image.width
                height: extraIha15Image.height
                
                Image {
                    id: extraIha15Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha15Marker.markerScale
                    height: 18 * extraIha15Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T15"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA16======
        MapQuickItem {
            id: extraIha16Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.843966)
            anchorPoint.x: extraIha16Container.width/2
            anchorPoint.y: extraIha16Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha16Container
                width: extraIha16Image.width
                height: extraIha16Image.height
                
                Image {
                    id: extraIha16Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha16Marker.markerScale
                    height: 18 * extraIha16Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T16"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA17======
        MapQuickItem {
            id: extraIha17Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.844966)
            anchorPoint.x: extraIha17Container.width/2
            anchorPoint.y: extraIha17Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha17Container
                width: extraIha17Image.width
                height: extraIha17Image.height
                
                Image {
                    id: extraIha17Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha17Marker.markerScale
                    height: 18 * extraIha17Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T17"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA18======
        MapQuickItem {
            id: extraIha18Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.845966)
            anchorPoint.x: extraIha18Container.width/2
            anchorPoint.y: extraIha18Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha18Container
                width: extraIha18Image.width
                height: extraIha18Image.height
                
                Image {
                    id: extraIha18Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha18Marker.markerScale
                    height: 18 * extraIha18Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T18"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA19======
        MapQuickItem {
            id: extraIha19Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.846966)
            anchorPoint.x: extraIha19Container.width/2
            anchorPoint.y: extraIha19Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha19Container
                width: extraIha19Image.width
                height: extraIha19Image.height
                
                Image {
                    id: extraIha19Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha19Marker.markerScale
                    height: 18 * extraIha19Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T19"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA20======
        MapQuickItem {
            id: extraIha20Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.847966)
            anchorPoint.x: extraIha20Container.width/2
            anchorPoint.y: extraIha20Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha20Container
                width: extraIha20Image.width
                height: extraIha20Image.height
                
                Image {
                    id: extraIha20Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha20Marker.markerScale
                    height: 18 * extraIha20Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T20"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA21======
        MapQuickItem {
            id: extraIha21Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.848966)
            anchorPoint.x: extraIha21Container.width/2
            anchorPoint.y: extraIha21Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha21Container
                width: extraIha21Image.width
                height: extraIha21Image.height
                
                Image {
                    id: extraIha21Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha21Marker.markerScale
                    height: 18 * extraIha21Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T21"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA22======
        MapQuickItem {
            id: extraIha22Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.849966)
            anchorPoint.x: extraIha22Container.width/2
            anchorPoint.y: extraIha22Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha22Container
                width: extraIha22Image.width
                height: extraIha22Image.height
                
                Image {
                    id: extraIha22Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha22Marker.markerScale
                    height: 18 * extraIha22Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T22"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA23======
        MapQuickItem {
            id: extraIha23Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.850966)
            anchorPoint.x: extraIha23Container.width/2
            anchorPoint.y: extraIha23Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha23Container
                width: extraIha23Image.width
                height: extraIha23Image.height
                
                Image {
                    id: extraIha23Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha23Marker.markerScale
                    height: 18 * extraIha23Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T23"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA24======
        MapQuickItem {
            id: extraIha24Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.851966)
            anchorPoint.x: extraIha24Container.width/2
            anchorPoint.y: extraIha24Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha24Container
                width: extraIha24Image.width
                height: extraIha24Image.height
                
                Image {
                    id: extraIha24Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha24Marker.markerScale
                    height: 18 * extraIha24Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T24"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA25======
        MapQuickItem {
            id: extraIha25Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.852966)
            anchorPoint.x: extraIha25Container.width/2
            anchorPoint.y: extraIha25Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha25Container
                width: extraIha25Image.width
                height: extraIha25Image.height
                
                Image {
                    id: extraIha25Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha25Marker.markerScale
                    height: 18 * extraIha25Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T25"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA26======
        MapQuickItem {
            id: extraIha26Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.853966)
            anchorPoint.x: extraIha26Container.width/2
            anchorPoint.y: extraIha26Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha26Container
                width: extraIha26Image.width
                height: extraIha26Image.height
                
                Image {
                    id: extraIha26Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha26Marker.markerScale
                    height: 18 * extraIha26Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T26"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA27======
        MapQuickItem {
            id: extraIha27Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.854966)
            anchorPoint.x: extraIha27Container.width/2
            anchorPoint.y: extraIha27Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha27Container
                width: extraIha27Image.width
                height: extraIha27Image.height
                
                Image {
                    id: extraIha27Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha27Marker.markerScale
                    height: 18 * extraIha27Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T27"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA28======
        MapQuickItem {
            id: extraIha28Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.855966)
            anchorPoint.x: extraIha28Container.width/2
            anchorPoint.y: extraIha28Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha28Container
                width: extraIha28Image.width
                height: extraIha28Image.height
                
                Image {
                    id: extraIha28Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha28Marker.markerScale
                    height: 18 * extraIha28Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T28"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA29======
        MapQuickItem {
            id: extraIha29Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.856966)
            anchorPoint.x: extraIha29Container.width/2
            anchorPoint.y: extraIha29Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha29Container
                width: extraIha29Image.width
                height: extraIha29Image.height
                
                Image {
                    id: extraIha29Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha29Marker.markerScale
                    height: 18 * extraIha29Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T29"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA30======
        MapQuickItem {
            id: extraIha30Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.857966)
            anchorPoint.x: extraIha30Container.width/2
            anchorPoint.y: extraIha30Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha30Container
                width: extraIha30Image.width
                height: extraIha30Image.height
                
                Image {
                    id: extraIha30Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha30Marker.markerScale
                    height: 18 * extraIha30Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T30"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA31======
        MapQuickItem {
            id: extraIha31Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.858966)
            anchorPoint.x: extraIha31Container.width/2
            anchorPoint.y: extraIha31Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha31Container
                width: extraIha31Image.width
                height: extraIha31Image.height
                
                Image {
                    id: extraIha31Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha31Marker.markerScale
                    height: 18 * extraIha31Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T31"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA32======
        MapQuickItem {
            id: extraIha32Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.859966)
            anchorPoint.x: extraIha32Container.width/2
            anchorPoint.y: extraIha32Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha32Container
                width: extraIha32Image.width
                height: extraIha32Image.height
                
                Image {
                    id: extraIha32Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha32Marker.markerScale
                    height: 18 * extraIha32Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T32"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA33======
        MapQuickItem {
            id: extraIha33Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.860966)
            anchorPoint.x: extraIha33Container.width/2
            anchorPoint.y: extraIha33Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha33Container
                width: extraIha33Image.width
                height: extraIha33Image.height
                
                Image {
                    id: extraIha33Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha33Marker.markerScale
                    height: 18 * extraIha33Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T33"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA34======
        MapQuickItem {
            id: extraIha34Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.861966)
            anchorPoint.x: extraIha34Container.width/2
            anchorPoint.y: extraIha34Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha34Container
                width: extraIha34Image.width
                height: extraIha34Image.height
                
                Image {
                    id: extraIha34Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha34Marker.markerScale
                    height: 18 * extraIha34Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T34"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA35======
        MapQuickItem {
            id: extraIha35Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.862966)
            anchorPoint.x: extraIha35Container.width/2
            anchorPoint.y: extraIha35Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha35Container
                width: extraIha35Image.width
                height: extraIha35Image.height
                
                Image {
                    id: extraIha35Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha35Marker.markerScale
                    height: 18 * extraIha35Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T35"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA36======
        MapQuickItem {
            id: extraIha36Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.863966)
            anchorPoint.x: extraIha36Container.width/2
            anchorPoint.y: extraIha36Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha36Container
                width: extraIha36Image.width
                height: extraIha36Image.height
                
                Image {
                    id: extraIha36Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha36Marker.markerScale
                    height: 18 * extraIha36Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T36"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA37======
        MapQuickItem {
            id: extraIha37Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.864966)
            anchorPoint.x: extraIha37Container.width/2
            anchorPoint.y: extraIha37Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha37Container
                width: extraIha37Image.width
                height: extraIha37Image.height
                
                Image {
                    id: extraIha37Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha37Marker.markerScale
                    height: 18 * extraIha37Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T37"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA38======
        MapQuickItem {
            id: extraIha38Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.865966)
            anchorPoint.x: extraIha38Container.width/2
            anchorPoint.y: extraIha38Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha38Container
                width: extraIha38Image.width
                height: extraIha38Image.height
                
                Image {
                    id: extraIha38Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha38Marker.markerScale
                    height: 18 * extraIha38Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T38"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA39======
        MapQuickItem {
            id: extraIha39Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.866966)
            anchorPoint.x: extraIha39Container.width/2
            anchorPoint.y: extraIha39Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha39Container
                width: extraIha39Image.width
                height: extraIha39Image.height
                
                Image {
                    id: extraIha39Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha39Marker.markerScale
                    height: 18 * extraIha39Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T39"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA40======
        MapQuickItem {
            id: extraIha40Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.867966)
            anchorPoint.x: extraIha40Container.width/2
            anchorPoint.y: extraIha40Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha40Container
                width: extraIha40Image.width
                height: extraIha40Image.height
                
                Image {
                    id: extraIha40Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha40Marker.markerScale
                    height: 18 * extraIha40Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T40"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA41======
        MapQuickItem {
            id: extraIha41Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.868966)
            anchorPoint.x: extraIha41Container.width/2
            anchorPoint.y: extraIha41Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha41Container
                width: extraIha41Image.width
                height: extraIha41Image.height
                
                Image {
                    id: extraIha41Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha41Marker.markerScale
                    height: 18 * extraIha41Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T41"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA42======
        MapQuickItem {
            id: extraIha42Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.869966)
            anchorPoint.x: extraIha42Container.width/2
            anchorPoint.y: extraIha42Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha42Container
                width: extraIha42Image.width
                height: extraIha42Image.height
                
                Image {
                    id: extraIha42Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha42Marker.markerScale
                    height: 18 * extraIha42Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T42"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // ======İHA43======
        MapQuickItem {
            id: extraIha43Marker
            coordinate: QtPositioning.coordinate(27.548676, 6.870966)
            anchorPoint.x: extraIha43Container.width/2
            anchorPoint.y: extraIha43Container.height/2
            property real markerScale: 1.0
            z: 4
            sourceItem: Item {
                id: extraIha43Container
                width: extraIha43Image.width
                height: extraIha43Image.height
                
                Image {
                    id: extraIha43Image
                    source: "qrc:/icons/enemy.png"
                    width: 18 * extraIha43Marker.markerScale
                    height: 18 * extraIha43Marker.markerScale
                    smooth: true
                    anchors.centerIn: parent
                    rotation: 0
                }
                
                Rectangle { 
                    width: 14; height: 14; color: "#e74c3c"; radius: 7; 
                    anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: -15
                    Text { text: "T43"; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 8 } 
                }
            }
        }

        // Waypoint'leri görsel gösterim (Map'in içinde olmalı)
        Repeater {
            model: waypoints
            MapQuickItem {
                coordinate: QtPositioning.coordinate(modelData.latitude, modelData.longitude)
                anchorPoint.x: waypointItem.width/2
                anchorPoint.y: waypointItem.height
                z: 5
                sourceItem: Item {
                    id: waypointItem
                    width: waypointIcon.width
                    height: waypointIcon.height + waypointText.height + 5
                    Image { 
                        id: waypointIcon
                        source: "qrc:/icons/waypoint.png"
                                        width: 18
                height: 18
                        smooth: true
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    Text { 
                        id: waypointText
                        text: "WP" + (index + 1)
                        font.bold: true
                        font.pixelSize: 10
                        color: "white"
                        anchors.top: waypointIcon.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.topMargin: 2
                    }
                }
            }
        }
    }

    // ================== İHA BİLGİ PANELİ ==================
    // İHA bilgi gösterme fonksiyonu
    function showIhaInfo(teamNo, lat, lon, yaw, altitude, pitch, roll, speed, timeDiff) {
        selectedIhaInfo = {
            teamNumber: Number(teamNo),
            latitude: lat,
            longitude: lon,
            yaw: yaw,
            altitude: altitude,
            pitch: pitch,
            roll: roll,
            speed: speed,
            timeDiff: timeDiff
        }
        ihaInfoPanelVisible = true
        console.log("İHA T" + teamNo + " bilgi paneli açıldı")
    }
    
    // Toplu tıklama ekleme sistemi hazır
    
    // Düşman İHA bilgi gösterme fonksiyonu
    function showEnemyIhaInfo(teamNo, lat, lon, yaw, altitude, pitch, roll, speed, timeDiff) {
        // Önce diğer paneli kapat
        ihaInfoPanelVisible = false
        
        selectedIhaInfo = {
            teamNumber: Number(teamNo),
            latitude: lat,
            longitude: lon,
            yaw: yaw,
            altitude: altitude,
            pitch: pitch,
            roll: roll,
            speed: speed,
            timeDiff: timeDiff
        }
        enemyInfoPanelVisible = true
        console.log("Düşman İHA T" + teamNo + " bilgi paneli açıldı")
    }
    
    // Panel verilerini güncelleme fonksiyonu
    function updatePanelData(teamNo, lat, lon, yaw, altitude, pitch, roll, speed, timeDiff) {
        // Eğer herhangi bir panel açıksa ve doğru İHA seçiliyse, manuel güncelleme yap
        if (selectedIhaInfo && Number(selectedIhaInfo.teamNumber) === Number(teamNo) && anyPanelVisible) {
            // Yeni obje oluşturarak QML'in değişikliği algılamasını sağla
            selectedIhaInfo = {
                teamNumber: teamNo,
                latitude: lat,
                longitude: lon,
                yaw: yaw,
                altitude: altitude,
                pitch: pitch,
                roll: roll,
                speed: speed,
                timeDiff: timeDiff
            }
        }
    }
    
    // Property binding ile otomatik güncelleme - Timer'a gerek yok
    
    // Düşman İHA paneli için state
    property bool enemyInfoPanelVisible: false
    
    // Herhangi bir panel açık mı kontrolü - JavaScript kullanmadan
    property bool anyPanelVisible: ihaInfoPanelVisible || enemyInfoPanelVisible
    
    // İHA bilgi paneli - tıklama ile gösterilir
    Rectangle {
        id: ihaInfoPanel
        width: 200
        height: 120
        color: "#000000"
        radius: 8
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 10
        z: 10
        opacity: 0.9
        visible: ihaInfoPanelVisible

        Column {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 4

            Text {
                text: selectedIhaInfo ? ("Takım " + selectedIhaInfo.teamNumber + " İHA Bilgileri") : "İHA Bilgileri"
                color: "white"
                font.bold: true
                font.pixelSize: 12
            }

            Text {
                text: selectedIhaInfo ? ("Enlem: " + selectedIhaInfo.latitude.toFixed(7)) : "Enlem: --"
                color: "#CCCCCC"
                font.pixelSize: 10
            }

            Text {
                text: selectedIhaInfo ? ("Boylam: " + selectedIhaInfo.longitude.toFixed(7)) : "Boylam: --"
                color: "#CCCCCC"
                font.pixelSize: 10
            }

            Text {
                text: selectedIhaInfo ? ("Yönelme: " + selectedIhaInfo.yaw.toFixed(1) + "°") : "Yönelme: --"
                color: "#CCCCCC"
                font.pixelSize: 10
            }

            Text {
                text: selectedIhaInfo ? ("İrtifa: " + selectedIhaInfo.altitude.toFixed(1) + "m") : "İrtifa: --"
                color: "#FFD700"
                font.pixelSize: 10
            }

            Text {
                text: selectedIhaInfo ? ("Hız: " + selectedIhaInfo.speed.toFixed(1) + " m/s") : "Hız: --"
                color: "#FFD700"
                font.pixelSize: 10
            }
        }

        // Kapatma butonu
        Rectangle {
            width: 20
            height: 20
            color: "#e74c3c"
            radius: 10
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 5
            z: 11

            Text {
                text: "×"
                color: "white"
                font.bold: true
                font.pixelSize: 14
                anchors.centerIn: parent
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    ihaInfoPanelVisible = false
                    enemyInfoPanelVisible = false
                    selectedIhaInfo = null
                }
            }
        }
    }

    // Düşman İHA bilgi paneli - kırmızı arka plan
    Rectangle {
        id: enemyInfoPanel
        width: 200
        height: 120
        color: "#8B0000"
        radius: 8
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 10
        z: 10
        opacity: 0.9
        visible: enemyInfoPanelVisible

        Column {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 4

            Text {
                text: selectedIhaInfo ? ("Düşman T" + selectedIhaInfo.teamNumber + " İHA Bilgileri") : "Düşman İHA Bilgileri"
                color: "white"
                font.bold: true
                font.pixelSize: 12
            }

            Text {
                text: selectedIhaInfo ? ("Enlem: " + selectedIhaInfo.latitude.toFixed(7)) : "Enlem: --"
                color: "#FFCCCC"
                font.pixelSize: 10
            }

            Text {
                text: selectedIhaInfo ? ("Boylam: " + selectedIhaInfo.longitude.toFixed(7)) : "Boylam: --"
                color: "#FFCCCC"
                font.pixelSize: 10
            }

            Text {
                text: selectedIhaInfo ? ("Yönelme: " + selectedIhaInfo.yaw.toFixed(1) + "°") : "Yönelme: --"
                color: "#FFCCCC"
                font.pixelSize: 10
            }

            Text {
                text: selectedIhaInfo ? ("İrtifa: " + selectedIhaInfo.altitude.toFixed(1) + "m") : "İrtifa: --"
                color: "#FFD700"
                font.pixelSize: 10
            }

            Text {
                text: selectedIhaInfo ? ("Hız: " + selectedIhaInfo.speed.toFixed(1) + " m/s") : "Hız: --"
                color: "#FFD700"
                font.pixelSize: 10
            }
        }

        // Kapatma butonu
        Rectangle {
            width: 20
            height: 20
            color: "#e74c3c"
            radius: 10
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 5
            z: 11

            Text {
                text: "×"
                color: "white"
                font.bold: true
                font.pixelSize: 14
                anchors.centerIn: parent
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    enemyInfoPanelVisible = false
                    ihaInfoPanelVisible = false
                    selectedIhaInfo = null
                }
            }
        }
    }

    function setupEnemyMarkerClicks() {
        // Tüm statik marker'lara MouseArea ekle
        var markers = [extraIhaMarker, extraIha2Marker, extraIha3Marker, extraIha4Marker, extraIha5Marker, 
                      extraIha6Marker, extraIha7Marker, extraIha8Marker, extraIha9Marker, extraIha10Marker,
                      extraIha11Marker, extraIha12Marker, extraIha13Marker, extraIha14Marker, extraIha15Marker,
                      extraIha16Marker, extraIha17Marker, extraIha18Marker, extraIha19Marker, extraIha20Marker,
                      extraIha21Marker, extraIha22Marker, extraIha23Marker, extraIha24Marker, extraIha25Marker,
                      extraIha26Marker, extraIha27Marker, extraIha28Marker, extraIha29Marker, extraIha30Marker,
                      extraIha31Marker, extraIha32Marker, extraIha33Marker, extraIha34Marker, extraIha35Marker,
                      extraIha36Marker, extraIha37Marker, extraIha38Marker, extraIha39Marker, extraIha40Marker,
                      extraIha41Marker, extraIha42Marker, extraIha43Marker]
        for (var i = 0; i < markers.length; i++) {
            var marker = markers[i]
            if (marker && marker.sourceItem) {
                var mouseArea = Qt.createQmlObject('import QtQuick 2.15; MouseArea { anchors.fill: parent; acceptedButtons: Qt.LeftButton; preventStealing: true; z: 9999 }', marker.sourceItem)
                ;(function(teamNo, refMarker) {
                    mouseArea.clicked.connect(function() {
                        console.log("Statik İHA T" + teamNo + " tıklandı")
                        showEnemyIhaInfo(teamNo, refMarker.coordinate.latitude, refMarker.coordinate.longitude, 0, 0, 0, 0, 0, 0)
                    })
                })(i + 1, marker)
            }
        }
    }

    // Başlangıç trail set
    Component.onCompleted: {
        trailModel = [QtPositioning.coordinate(ihaLatLocation, ihaLonLocation)]
        trailLine.path = trailModel.slice()
        
        // Yeni iz sistemi başlat
        trailPoints = [{lat: ihaLatLocation, lon: ihaLonLocation, ts: Date.now()}]
        _trailPrimed = true
        
        // QR koordinatları artık C++ tarafından yönetiliyor
        
        console.log("İHA marker'larına tıklama özelliği ekleniyor...")
        setupEnemyMarkerClicks()
        
        console.log("Tüm İHA marker'larına tıklama özelliği eklendi!")
    }
    onIhaLatLocationChanged: { 
        // İz sistemi: ilk nokta veya yeni nokta ekle
        if (!_trailPrimed) {
            // İlk nokta ekle (zıplama olmasın)
            trailPoints = [{lat: ihaLatLocation, lon: ihaLonLocation, ts: Date.now()}]
            _trailPrimed = true
        } else {
            pushTrailPoint(ihaLatLocation, ihaLonLocation)
        }
        
        trailModel = [QtPositioning.coordinate(ihaLatLocation, ihaLonLocation)]; 
        trailLine.path = trailModel.slice()
        // Kendi İHA paneli açıksa güncelle (her iki panel için)
        if (selectedIhaInfo && selectedIhaInfo.teamNumber === 0 && anyPanelVisible) {
            selectedIhaInfo = {
                teamNumber: 0,
                latitude: ihaLatLocation,
                longitude: selectedIhaInfo.longitude,
                yaw: selectedIhaInfo.yaw,
                altitude: selectedIhaInfo.altitude,
                pitch: selectedIhaInfo.pitch,
                roll: selectedIhaInfo.roll,
                speed: selectedIhaInfo.speed,
                timeDiff: selectedIhaInfo.timeDiff
            }
        }
    }
    onIhaLonLocationChanged: { 
        // İz sistemi: boylam değişiminde de nokta ekle (eğer lat değişiminde eklenmemişse)
        if (_trailPrimed && trailPoints.length > 0) {
            var lastPoint = trailPoints[trailPoints.length - 1]
            if (Math.abs(lastPoint.lon - ihaLonLocation) > 0.000001) { // Çok küçük değişimleri yut
                pushTrailPoint(ihaLatLocation, ihaLonLocation)
            }
        }
        
        trailModel = [QtPositioning.coordinate(ihaLatLocation, ihaLonLocation)]; 
        trailLine.path = trailModel.slice()
        // Kendi İHA paneli açıksa güncelle (her iki panel için)
        if (selectedIhaInfo && selectedIhaInfo.teamNumber === 0 && anyPanelVisible) {
            selectedIhaInfo = {
                teamNumber: 0,
                latitude: selectedIhaInfo.latitude,
                longitude: ihaLonLocation,
                yaw: selectedIhaInfo.yaw,
                altitude: selectedIhaInfo.altitude,
                pitch: selectedIhaInfo.pitch,
                roll: selectedIhaInfo.roll,
                speed: selectedIhaInfo.speed,
                timeDiff: selectedIhaInfo.timeDiff
            }
        }
    }
    onIhaYawChanged: {
        // Kendi İHA paneli açıksa güncelle (her iki panel için)
        if (selectedIhaInfo && selectedIhaInfo.teamNumber === 0 && anyPanelVisible) {
            selectedIhaInfo = {
                teamNumber: 0,
                latitude: selectedIhaInfo.latitude,
                longitude: selectedIhaInfo.longitude,
                yaw: ihaYaw,
                altitude: selectedIhaInfo.altitude,
                pitch: selectedIhaInfo.pitch,
                roll: selectedIhaInfo.roll,
                speed: selectedIhaInfo.speed,
                timeDiff: selectedIhaInfo.timeDiff
            }
        }
    }
    
    onIhaAltitudeChanged: {
        // Kendi İHA paneli açıksa güncelle (her iki panel için)
        if (selectedIhaInfo && selectedIhaInfo.teamNumber === 0 && anyPanelVisible) {
            selectedIhaInfo = {
                teamNumber: 0,
                latitude: selectedIhaInfo.latitude,
                longitude: selectedIhaInfo.longitude,
                yaw: selectedIhaInfo.yaw,
                altitude: ihaAltitude,
                pitch: selectedIhaInfo.pitch,
                roll: selectedIhaInfo.roll,
                speed: selectedIhaInfo.speed,
                timeDiff: selectedIhaInfo.timeDiff
            }
        }
    }
    
    onIhaSpeedChanged: {
        // Kendi İHA paneli açıksa güncelle (her iki panel için)
        if (selectedIhaInfo && selectedIhaInfo.teamNumber === 0 && anyPanelVisible) {
            selectedIhaInfo = {
                teamNumber: 0,
                latitude: selectedIhaInfo.latitude,
                longitude: selectedIhaInfo.longitude,
                yaw: selectedIhaInfo.yaw,
                altitude: selectedIhaInfo.altitude,
                pitch: selectedIhaInfo.pitch,
                roll: selectedIhaInfo.roll,
                speed: ihaSpeed,
                timeDiff: selectedIhaInfo.timeDiff
            }
        }
    }

}
