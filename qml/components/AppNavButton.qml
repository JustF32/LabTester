import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: control

    property bool selected: false
    property string subtitle: ""
    property url iconSource: ""

    implicitHeight: 50
    implicitWidth: 210
    padding: 0
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0
    font.pixelSize: 15
    font.weight: Font.DemiBold
    hoverEnabled: true
    flat: true
    focusPolicy: Qt.NoFocus
    clip: true
    scale: control.down ? 0.985 : (control.hovered ? 1.01 : 1.0)

    Behavior on scale {
        NumberAnimation {
            duration: 100
            easing.type: Easing.OutCubic
        }
    }

    background: Rectangle {
        radius: 14
        antialiasing: true
        color: control.selected
            ? "#203b5b"
            : control.hovered
                ? "#18293d"
                : "transparent"
        border.width: 1
        border.color: control.selected
            ? "#5e8fca"
            : control.hovered
                ? "#3a5578"
                : "#2d435f"

        Behavior on color {
            ColorAnimation { duration: 150 }
        }

        Behavior on border.color {
            ColorAnimation { duration: 150 }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: 13
            color: control.selected
                ? "#1a314d"
                : control.hovered
                    ? "#172538"
                    : "#151f2f"
            opacity: control.selected ? 0.92 : 0.72

            Behavior on opacity {
                NumberAnimation { duration: 150 }
            }
        }

    }

    contentItem: RowLayout {
        spacing: 10

        Item {
            Layout.leftMargin: 14
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20

            Image {
                anchors.centerIn: parent
                width: 18
                height: 18
                source: control.iconSource
                fillMode: Image.PreserveAspectFit
                smooth: true
                antialiasing: true
                opacity: control.selected ? 1.0 : 0.82

                Behavior on opacity {
                    NumberAnimation { duration: 130 }
                }
            }
        }

        Text {
            text: control.text
            color: "#eef4ff"
            font.pixelSize: control.font.pixelSize
            font.weight: control.font.weight
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            Layout.fillWidth: true
            Layout.rightMargin: 10
        }
    }
}
