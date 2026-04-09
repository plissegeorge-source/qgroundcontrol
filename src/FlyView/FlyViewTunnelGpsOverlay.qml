import QtQuick
import QtLocation
import QtPositioning

import QGroundControl
import QGroundControl.Controls

Item {
    id: root

    property var mapControl
    property var vehicle

    readonly property var _tunnelGps: vehicle ? vehicle.tunnelGps : null

    QGCPalette {
        id: qgcPal
        colorGroupEnabled: enabled
    }

    Rectangle {
        id: panel
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: ScreenTools.defaultFontPixelWidth
        anchors.topMargin: ScreenTools.defaultFontPixelHeight
        visible: !!_tunnelGps
        color: qgcPal.windowShadeDark
        border.color: qgcPal.groupBorder
        radius: ScreenTools.defaultFontPixelWidth * 0.5

        width: ScreenTools.defaultFontPixelWidth * 34
        height: content.implicitHeight + (ScreenTools.defaultFontPixelHeight * 1.2)

        Column {
            id: content
            anchors.fill: parent
            anchors.margins: ScreenTools.defaultFontPixelHeight * 0.6
            spacing: ScreenTools.defaultFontPixelHeight * 0.25

            QGCLabel {
                text: qsTr("Tunnel GPS")
                font.bold: true
            }

            QGCLabel {
                text: _tunnelGps ? _tunnelGps.statusText : ""
                color: (_tunnelGps && _tunnelGps.available) ? qgcPal.colorGreen : qgcPal.warningText
            }

            QGCLabel { text: _tunnelGps ? _tunnelGps.gps1Summary : "" }
            QGCLabel { text: _tunnelGps ? _tunnelGps.gps2Summary : "" }

            QGCLabel {
                text: _tunnelGps && !isNaN(_tunnelGps.distanceMeters)
                      ? qsTr("Distance: %1 m").arg(_tunnelGps.distanceMeters.toFixed(1))
                      : qsTr("Distance: —")
            }

            QGCLabel {
                text: _tunnelGps ? qsTr("Selected: GPS %1").arg(_tunnelGps.selectedSource) : ""
            }
        }
    }

    MapQuickItem {
        visible: !!_tunnelGps && _tunnelGps.gps1Coordinate.isValid
        coordinate: _tunnelGps ? _tunnelGps.gps1Coordinate : QtPositioning.coordinate()
        anchorPoint.x: sourceItem.width * 0.5
        anchorPoint.y: sourceItem.height * 0.5
        map: mapControl
        z: QGroundControl.zOrderVehicles + 1

        sourceItem: Rectangle {
            width: ScreenTools.defaultFontPixelHeight * 2.2
            height: width
            radius: width / 2
            color: qgcPal.colorBlue
            border.color: qgcPal.text

            Rectangle {
                width: parent.width * 0.18
                height: parent.width * 0.56
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                color: qgcPal.text
                rotation: _tunnelGps ? _tunnelGps.gps1Heading : 0
                transformOrigin: Item.Bottom
            }

            QGCLabel {
                anchors.centerIn: parent
                text: "1"
                font.bold: true
            }
        }
    }

    MapQuickItem {
        visible: !!_tunnelGps && _tunnelGps.gps2Coordinate.isValid
        coordinate: _tunnelGps ? _tunnelGps.gps2Coordinate : QtPositioning.coordinate()
        anchorPoint.x: sourceItem.width * 0.5
        anchorPoint.y: sourceItem.height * 0.5
        map: mapControl
        z: QGroundControl.zOrderVehicles + 1

        sourceItem: Rectangle {
            width: ScreenTools.defaultFontPixelHeight * 2.2
            height: width
            radius: width / 2
            color: qgcPal.colorOrange
            border.color: qgcPal.text

            Rectangle {
                width: parent.width * 0.18
                height: parent.width * 0.56
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                color: qgcPal.text
                rotation: _tunnelGps ? _tunnelGps.gps2Heading : 0
                transformOrigin: Item.Bottom
            }

            QGCLabel {
                anchors.centerIn: parent
                text: "2"
                font.bold: true
            }
        }
    }
}
