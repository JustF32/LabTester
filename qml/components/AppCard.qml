import QtQuick

Rectangle {
    id: card

    property color baseColor: "#151d2a"
    property color borderColor: "#324a6a"

    radius: 16
    color: baseColor
    border.color: borderColor
    border.width: 1

    Behavior on color {
        ColorAnimation { duration: 150 }
    }

    Behavior on border.color {
        ColorAnimation { duration: 150 }
    }
}
