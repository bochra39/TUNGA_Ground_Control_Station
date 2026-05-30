import QtQuick 6.10
import QtQuick.Controls 6.10

GroupBox {
    title: "Kontrol Paneli"
    anchors.top: parent.top
    anchors.left: parent.left
    anchors.margins: 10
    z: 100
    Rectangle {
        color: "#2d2d2d"
        border.color: "#ffd600"
        border.width: 2
        radius: 8
        anchors.fill: parent
        anchors.margins: 4

        Row {
            spacing: 10
            anchors.centerIn: parent

            Button { text: "Yayına Bağlan" }
            Button { text: "Yenile" }
            Button { text: "Kamikaze" }
            Button { text: "HSS" }
            Button { text: "Kitlenme" }
            Button { text: "DURDUR"; enabled: false }
        }
    }
} 