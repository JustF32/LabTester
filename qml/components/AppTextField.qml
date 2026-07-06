import QtQuick
import QtQuick.Controls

TextField {
    id: control

    implicitHeight: 42
    leftPadding: 12
    rightPadding: 12
    topPadding: 8
    bottomPadding: 8
    color: "#eef4ff"
    selectedTextColor: "#f8fbff"
    selectionColor: "#3e6ea8"
    placeholderTextColor: "#7087a9"
    font.pixelSize: 14
    selectByMouse: true

    background: Rectangle {
        radius: 12
        border.width: 1
        border.color: control.activeFocus
            ? "#5b85bd"
            : control.hovered
                ? "#4c6f9f"
                : "#334a68"
        gradient: Gradient {
            GradientStop { position: 0; color: "#1b2636" }
            GradientStop { position: 1; color: "#141d2b" }
        }

        Behavior on border.color {
            ColorAnimation { duration: 130 }
        }
    }
}
