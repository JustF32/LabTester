import QtQuick
import QtQuick.Controls

Button {
    id: control

    property bool active: false
    property bool danger: false
    property bool quiet: false

    implicitHeight: 42
    implicitWidth: 124
    font.pixelSize: 14
    font.weight: Font.DemiBold
    hoverEnabled: true
    flat: true
    focusPolicy: Qt.NoFocus
    clip: true
    scale: !control.enabled ? 1.0 : control.down ? 0.98 : (control.hovered ? 1.015 : 1.0)

    Behavior on scale {
        NumberAnimation {
            duration: 110
            easing.type: Easing.OutCubic
        }
    }

    background: Rectangle {
        radius: 12
        border.width: 1
        border.color: control.enabled
            ? control.hovered
                ? "#4f6f9d"
                : "#385078"
            : "#2c3340"
        gradient: Gradient {
            GradientStop {
                position: 0
                color: !control.enabled
                    ? "#2d3340"
                    : control.quiet
                        ? "#202632"
                        : control.danger
                            ? (control.down ? "#9f3f4d" : "#be4d60")
                            : (control.down ? "#2f78cb" : "#3b8ef0")
            }
            GradientStop {
                position: 1
                color: !control.enabled
                    ? "#252b35"
                    : control.quiet
                        ? "#171d27"
                        : control.danger
                            ? (control.down ? "#7c3140" : "#8f3949")
                            : (control.down ? "#1f5ea7" : "#2a6fc7")
            }
        }

        Behavior on border.color {
            ColorAnimation { duration: 130 }
        }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "#ffffff"
            opacity: control.enabled && control.hovered ? 0.035 : 0.0

            Behavior on opacity {
                NumberAnimation {
                    duration: 140
                    easing.type: Easing.OutQuad
                }
            }
        }
    }

    contentItem: Text {
        text: control.text
        color: control.enabled ? "#f5f8ff" : "#8893a5"
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
