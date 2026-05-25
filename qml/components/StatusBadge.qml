import QtQuick
import QtQuick.Controls

Rectangle {
    id: badge

    property string status: "Ожидает"
    property string statusKey: "Pending"
    property int maxWidth: -1

    readonly property int naturalWidth: Math.ceil(textMetrics.advanceWidth) + 18
    readonly property bool clamped: maxWidth > 0 && maxWidth < naturalWidth
    implicitWidth: naturalWidth
    width: maxWidth > 0 ? Math.min(naturalWidth, maxWidth) : naturalWidth
    implicitHeight: 28
    radius: 10

    color: {
        const key = statusKey && statusKey.length > 0 ? statusKey : status
        if (key === "FullySolved")
            return "#1f7f57"
        if (key === "NeedsOptimization")
            return "#2b6e9d"
        if (key === "BasicOnly")
            return "#6c5aa8"
        if (key === "Passed" || key === "Успешно")
            return "#1f7f57"
        if (key === "Failed" || key === "Провалено")
            return "#a1444f"
        if (key === "BuildError" || key === "Build Error" || key === "Ошибка сборки")
            return "#9a6a28"
        return "#496183"
    }

    border.color: Qt.darker(color, 1.25)
    border.width: 1

    Behavior on color {
        ColorAnimation { duration: 140 }
    }

    TextMetrics {
        id: textMetrics
        text: badge.status
        font.pixelSize: label.font.pixelSize
        font.weight: label.font.weight
        font.family: label.font.family
    }

    Label {
        id: label
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 9
        anchors.rightMargin: 9
        anchors.verticalCenter: parent.verticalCenter
        text: badge.status
        color: "#f5f8ff"
        font.pixelSize: 12
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        elide: badge.clamped ? Text.ElideRight : Text.ElideNone
    }
}
