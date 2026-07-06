import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabTester

RowLayout {
    id: root

    property string text: ""
    property string placeholderText: ""
    signal browseRequested()

    spacing: 10

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 42
        radius: 12
        border.width: 1
        border.color: "#334a68"
        gradient: Gradient {
            GradientStop { position: 0; color: "#1b2636" }
            GradientStop { position: 1; color: "#141d2b" }
        }

        Text {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            verticalAlignment: Text.AlignVCenter
            text: root.text.length > 0 ? root.text : root.placeholderText
            color: root.text.length > 0 ? "#f0f5ff" : "#7c8faa"
            elide: Text.ElideMiddle
        }
    }

    AppButton {
        text: "Обзор"
        quiet: false
        Layout.preferredWidth: 110
        onClicked: root.browseRequested()
    }
}
