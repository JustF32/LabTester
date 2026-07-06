import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabTester

Rectangle {
    id: card

    property string studentName: ""
    property string labTitle: ""
    property int totalTests: 0
    property int passedTests: 0
    property int failedTests: 0
    property string status: "Ожидает"
    property string statusKey: "Pending"
    property string message: ""
    property string executedAt: ""

    radius: 14
    color: "#151c28"
    border.color: "#314766"
    border.width: 1
    implicitHeight: 132

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: card.studentName
                color: "#edf2ff"
                font.pixelSize: 16
                font.bold: true
            }

            Item {
                Layout.fillWidth: true
            }

            StatusBadge {
                status: card.status
                statusKey: card.statusKey
            }
        }

        Label {
            Layout.fillWidth: true
            text: card.labTitle
            color: "#8ec8ff"
            elide: Text.ElideRight
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            Label {
                text: "Тесты: " + card.passedTests + "/" + card.totalTests
                color: "#d4deef"
            }

            Label {
                text: "Провалено: " + card.failedTests
                color: "#f09b9b"
            }

            Item {
                Layout.fillWidth: true
            }

            Label {
                text: card.executedAt
                color: "#8ea0ba"
                font.pixelSize: 12
            }
        }

        Label {
            Layout.fillWidth: true
            text: card.message
            color: "#a9b8d0"
            elide: Text.ElideRight
        }
    }
}
