import QtQuick
import QtQuick.Controls

Item {
    width: 400
    height: 300

    ListView {
        anchors.fill: parent
        model: ihaModel
        delegate: Rectangle {
            width: parent.width
            height: 120
            color: "lightblue"
            border.color: "gray"
            border.width: 1

            Column {
                anchors.centerIn: parent
                spacing: 4

                Text { text: "Takım: " + takim_numarasi }
                Text { text: "Enlem: " + iha_enlem }
                Text { text: "Boylam: " + iha_boylam }
                Text { text: "Hız: " + iha_hiz + " km/h" }
                Text { text: "Batarya: " + iha_batarya + "%" }
            }
        }
    }
} 