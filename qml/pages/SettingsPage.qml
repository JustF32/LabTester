import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabTester

Item {
    id: page

    property var controller
    property var requestUploadSync
    property var requestDownloadSync
    property bool executionSettingsSaveInProgress: false
    property string runtimeLogText: ""
    property var executionModeOptions: [
        { title: "Серверный", value: "server" },
        { title: "Локальный", value: "local" }
    ]

    function executionModeIndex(modeValue) {
        const normalized = (modeValue || "").toString().toLowerCase()
        for (let i = 0; i < executionModeOptions.length; ++i) {
            if (executionModeOptions[i].value === normalized)
                return i
        }
        return 0
    }

    function syncExecutionSettings() {
        if (!controller)
            return
        runModeCombo.currentIndex = executionModeIndex(controller.executionMode || "server")
        remoteUrlField.text = controller.remoteExecutionUrl || ""
        remoteTokenField.text = controller.remoteExecutionToken || ""
    }

    function saveExecutionSettings() {
        if (!controller)
            return

        executionSettingsSaveInProgress = true
        const selectedMode = executionModeOptions[Math.max(0, runModeCombo.currentIndex)].value
        controller.setRemoteExecutionUrl(remoteUrlField.text.trim())
        controller.setRemoteExecutionToken(remoteTokenField.text.trim())
        controller.setRemoteExecutionNgrokMode(false)
        controller.setExecutionMode(selectedMode)
        executionSettingsSaveInProgress = false
        syncExecutionSettings()
    }

    function openRuntimeLog() {
        if (!controller)
            return
        runtimeLogText = controller.readRuntimeLog(250000)
        runtimeLogDialog.open()
    }

    Component.onCompleted: syncExecutionSettings()

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        AppCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 236
            baseColor: "#121d2e"
            borderColor: "#35537d"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Label {
                        text: "Сервер проверки"
                        color: "#edf4ff"
                        font.pixelSize: 18
                        font.bold: true
                        Layout.fillWidth: true
                    }

                    AppButton {
                        text: "Лог"
                        quiet: true
                        Layout.preferredWidth: 90
                        onClicked: page.openRuntimeLog()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Label {
                        text: "Сервер"
                        color: "#c2d6f4"
                        Layout.preferredWidth: 120
                    }

                    AppTextField {
                        id: remoteUrlField
                        Layout.fillWidth: true
                        placeholderText: "https://server:20000"
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Label {
                        text: "Режим"
                        color: "#c2d6f4"
                        Layout.preferredWidth: 120
                    }

                    AppComboBox {
                        id: runModeCombo
                        Layout.fillWidth: true
                        textRole: "title"
                        model: page.executionModeOptions
                        onActivated: page.saveExecutionSettings()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Label {
                        text: "Bearer токен"
                        color: "#c2d6f4"
                        Layout.preferredWidth: 120
                    }

                    AppTextField {
                        id: remoteTokenField
                        Layout.fillWidth: true
                        placeholderText: "Вставьте токен сервера"
                        echoMode: TextInput.Password
                        passwordCharacter: "*"
                    }

                    AppButton {
                        text: "Сохранить"
                        Layout.preferredWidth: 130
                        onClicked: page.saveExecutionSettings()
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: "Для удаленной проверки укажите адрес сервера, выберите режим и сохраните токен."
                    color: "#8fa8cb"
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }
        }
        Item { Layout.fillHeight: true }
    }

    Connections {
        target: controller

        function onExecutionSettingsChanged() {
            if (page.executionSettingsSaveInProgress)
                return
            page.syncExecutionSettings()
        }
    }

    Popup {
        id: runtimeLogDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        dim: false
        width: Math.min(page.width - 40, 1020)
        height: Math.min(page.height - 40, 700)
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        Overlay.modal: Rectangle { color: "transparent" }

        background: Rectangle {
            radius: 14
            color: "#101a29"
            border.color: "#39567f"
            border.width: 1
        }

        onOpened: {
            Qt.callLater(function() {
                if (!runtimeLogTextArea)
                    return
                runtimeLogTextArea.cursorPosition = runtimeLogTextArea.length
            })
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            RowLayout {
                Layout.fillWidth: true

                Label {
                    text: "Журнал выполнения"
                    color: "#edf4ff"
                    font.pixelSize: 17
                    font.bold: true
                    Layout.fillWidth: true
                }

                AppButton {
                    text: "Обновить"
                    quiet: true
                    Layout.preferredWidth: 120
                    onClicked: page.runtimeLogText = controller ? controller.readRuntimeLog(250000) : ""
                }

                ToolButton {
                    id: closeRuntimeLogButton
                    implicitWidth: 36
                    implicitHeight: 36
                    hoverEnabled: true
                    focusPolicy: Qt.NoFocus
                    background: Rectangle {
                        radius: 10
                        color: closeRuntimeLogButton.hovered ? "#2a415f" : "#1a2a40"
                        border.color: closeRuntimeLogButton.hovered ? "#5889c3" : "#36557a"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: "x"
                        color: "#d8e8ff"
                        font.pixelSize: 16
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: runtimeLogDialog.close()
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                TextArea {
                    id: runtimeLogTextArea
                    text: runtimeLogText.length > 0 ? runtimeLogText : "Лог пуст."
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.NoWrap
                    color: "#d9e7fb"
                    font.family: "Consolas"
                    font.pixelSize: 12
                    background: Rectangle {
                        radius: 10
                        color: "#0b1220"
                        border.color: "#263d5e"
                        border.width: 1
                    }
                }
            }
        }
    }
}
