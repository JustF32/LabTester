import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabTester

Item {
    id: page

    property var controller
    property var accounts: []
    property string pendingAction: ""
    property var pendingInfo: ({})
    property string statusText: ""
    property bool statusIsError: false
    property bool passwordVisible: false

    function selectedUsername() {
        return accountField.text.trim().toLowerCase()
    }

    function selectedPassword() {
        return passwordField.text.trim()
    }

    function localLabWorks() {
        return Number(pendingInfo.localLabWorkCount || 0)
    }

    function remoteLabWorks() {
        return Number(pendingInfo.remoteLabWorkCount || 0)
    }

    function localHistory() {
        return Number(pendingInfo.localHistoryCount || 0)
    }

    function remoteHistory() {
        return Number(pendingInfo.remoteHistoryCount || 0)
    }

    function statusMessage() {
        if (statusText.length > 0)
            return statusText
        if (controller && controller.lastRunMessage.length > 0)
            return controller.lastRunMessage
        return "Готово к синхронизации. Выберите аккаунт, проверьте пароль и направление обмена."
    }

    function setStatus(message, isError) {
        statusText = message
        statusIsError = isError === true
    }

    function saveCredentials() {
        if (!controller)
            return false
        controller.setCloudUsername(selectedUsername())
        controller.setCloudPassword(selectedPassword())
        setStatus("Аккаунт сохранен локально. При следующем запуске поля будут заполнены автоматически.", false)
        return true
    }

    function refreshAccounts() {
        setStatus("Обновляем список аккаунтов на сервере...", false)
        if (controller)
            controller.requestCloudAccounts()
    }

    function prepareSync(action) {
        statusText = ""
        statusIsError = false
        if (selectedUsername().length === 0 || selectedPassword().length === 0) {
            setStatus("Введите аккаунт и пароль перед синхронизацией.", true)
            return
        }
        saveCredentials()
        statusText = ""
        pendingAction = action
        pendingInfo = ({})
        if (!controller.requestCloudVersionInfo(selectedUsername(), true)) {
            pendingAction = ""
            setStatus(controller.lastRunMessage || "Не удалось получить данные сервера.", true)
        }
    }

    function createAccount() {
        if (selectedUsername().length === 0 || selectedPassword().length === 0) {
            setStatus("Введите имя и пароль нового аккаунта.", true)
            return
        }
        if (controller)
            controller.createCloudAccount(selectedUsername(), selectedPassword())
    }

    function chooseAccount(username) {
        accountField.text = username || ""
        setStatus("Выбран аккаунт " + selectedUsername() + ". Введите пароль и выберите действие.", false)
    }

    function sourceLabel() {
        return pendingAction === "download" ? "Сервер" : "Этот компьютер"
    }

    function targetLabel() {
        return pendingAction === "download" ? "Этот компьютер" : "Сервер"
    }

    function replacementText() {
        const account = selectedUsername()
        if (pendingAction === "download")
            return "Локальные данные аккаунта " + account + " будут заменены серверной копией."
        return "Серверная копия аккаунта " + account + " будет заменена локальными данными."
    }

    Component.onCompleted: {
        accountField.text = controller ? controller.cloudUsername : ""
        passwordField.text = controller ? controller.cloudPassword : ""
        refreshAccounts()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 430
            Layout.maximumHeight: 430
            spacing: 12

            AppCard {
                Layout.preferredWidth: 520
                Layout.fillHeight: true
                baseColor: "#121d2e"
                borderColor: "#35537d"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Label {
                                text: "Аккаунт"
                                color: "#edf4ff"
                                font.pixelSize: 18
                                font.bold: true
                            }

                            Label {
                                Layout.fillWidth: true
                                text: "Учетные данные для синхронизации с сервером"
                                color: "#8fa8cb"
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }
                        }

                        AppButton {
                            text: "Обновить"
                            quiet: true
                            Layout.preferredWidth: 116
                            enabled: controller && !controller.cloudSyncInProgress
                            onClicked: page.refreshAccounts()
                        }
                    }

                    Label {
                        text: "Аккаунт на сервере"
                        color: "#c2d6f4"
                        font.pixelSize: 12
                    }

                    AppComboBox {
                        id: accountsCombo
                        Layout.fillWidth: true
                        textRole: "username"
                        model: page.accounts
                        onActivated: {
                            if (currentIndex >= 0 && page.accounts[currentIndex])
                                page.chooseAccount(page.accounts[currentIndex].username || "")
                        }
                    }

                    Label {
                        text: "Имя аккаунта"
                        color: "#c2d6f4"
                        font.pixelSize: 12
                    }

                    AppTextField {
                        id: accountField
                        Layout.fillWidth: true
                        placeholderText: "Например, group_1"
                        inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhPreferLowercase
                    }

                    Label {
                        text: "Пароль"
                        color: "#c2d6f4"
                        font.pixelSize: 12
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42

                        AppTextField {
                            id: passwordField
                            anchors.fill: parent
                            rightPadding: 52
                            placeholderText: "Пароль аккаунта"
                            echoMode: page.passwordVisible ? TextInput.Normal : TextInput.Password
                            passwordCharacter: "*"
                        }

                        ToolButton {
                            id: togglePasswordButton
                            anchors.right: parent.right
                            anchors.rightMargin: 4
                            anchors.verticalCenter: parent.verticalCenter
                            width: 36
                            height: 34
                            hoverEnabled: true
                            focusPolicy: Qt.NoFocus
                            onClicked: page.passwordVisible = !page.passwordVisible
                            background: Rectangle {
                                radius: 10
                                color: togglePasswordButton.hovered ? "#263d5d" : "transparent"
                                border.width: togglePasswordButton.hovered ? 1 : 0
                                border.color: "#42628d"
                            }
                            contentItem: Text {
                                text: ""
                            }

                            Image {
                                anchors.centerIn: parent
                                width: 20
                                height: 20
                                source: page.passwordVisible
                                    ? "qrc:/resources/icons/eye.svg"
                                    : "qrc:/resources/icons/eye_off.svg"
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                antialiasing: true
                            }
                        }
                    }

                    AppButton {
                        text: "Сохранить"
                        quiet: true
                        Layout.fillWidth: true
                        Layout.preferredHeight: 46
                        enabled: controller && !controller.cloudSyncInProgress
                        onClicked: page.saveCredentials()
                    }

                    AppButton {
                        text: "Создать аккаунт"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 46
                        enabled: controller && !controller.cloudSyncInProgress
                        onClicked: page.createAccount()
                    }

                    Item { Layout.fillHeight: true }
                }
            }

            AppCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                baseColor: "#121d2e"
                borderColor: "#35537d"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            text: "Обмен данными"
                            color: "#edf4ff"
                            font.pixelSize: 18
                            font.bold: true
                        }

                        Label {
                            Layout.fillWidth: true
                            text: "Выберите направление. Перед заменой появится подробное подтверждение."
                            color: "#8fa8cb"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 86
                        radius: 12
                        color: "#17263a"
                        border.color: "#314b70"
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 12

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3

                                Label {
                                    text: "Загрузить с сервера"
                                    color: "#edf4ff"
                                    font.bold: true
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: "Серверная копия заменит локальную базу приложения."
                                    color: "#9fb7da"
                                    font.pixelSize: 13
                                    elide: Text.ElideRight
                                }
                            }

                            AppButton {
                                text: controller && controller.cloudSyncInProgress ? "Проверяем..." : "Загрузить"
                                Layout.preferredWidth: 150
                                Layout.preferredHeight: 46
                                enabled: controller && !controller.cloudSyncInProgress
                                onClicked: page.prepareSync("download")
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 86
                        radius: 12
                        color: "#17263a"
                        border.color: "#314b70"
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 12

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3

                                Label {
                                    text: "Выгрузить на сервер"
                                    color: "#edf4ff"
                                    font.bold: true
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: "Текущая локальная база заменит серверную копию."
                                    color: "#9fb7da"
                                    font.pixelSize: 13
                                    elide: Text.ElideRight
                                }
                            }

                            AppButton {
                                text: controller && controller.cloudSyncInProgress ? "Проверяем..." : "Выгрузить"
                                Layout.preferredWidth: 150
                                Layout.preferredHeight: 46
                                enabled: controller && !controller.cloudSyncInProgress
                                onClicked: page.prepareSync("upload")
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 12
                        color: page.statusIsError ? "#2a1d29" : "#142235"
                        border.color: page.statusIsError ? "#884a5a" : "#314b70"
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 12

                            Rectangle {
                                Layout.preferredWidth: 48
                                Layout.preferredHeight: 48
                                radius: 14
                                color: page.statusIsError ? "#623141" : "#203b5b"
                                border.color: page.statusIsError ? "#c46878" : "#5e8fca"

                                Text {
                                    anchors.centerIn: parent
                                    text: page.statusIsError ? "!" : "i"
                                    color: "#f5f8ff"
                                    font.pixelSize: 23
                                    font.bold: true
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4

                                Label {
                                    text: page.statusIsError ? "Требуется действие" : "Состояние"
                                    color: "#edf4ff"
                                    font.pixelSize: 16
                                    font.bold: true
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: page.statusMessage()
                                    color: page.statusIsError ? "#ffccd5" : "#b9ccea"
                                    font.pixelSize: 14
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }
            }
        }

        AppCard {
            Layout.fillWidth: true
            Layout.fillHeight: true
            baseColor: "#121d2e"
            borderColor: "#35537d"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Label {
                        text: "Аккаунты на сервере"
                        color: "#edf4ff"
                        font.pixelSize: 18
                        font.bold: true
                        Layout.fillWidth: true
                    }

                    Label {
                        text: page.accounts.length + " шт."
                        color: "#9fb7da"
                        font.pixelSize: 13
                    }
                }

                ListView {
                    id: accountsList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 8
                    model: page.accounts

                    delegate: Rectangle {
                        required property var modelData

                        width: accountsList.width
                        height: 58
                        radius: 10
                        color: (modelData.username || "") === page.selectedUsername() ? "#25456a" : "#17263a"
                        border.width: 1
                        border.color: (modelData.username || "") === page.selectedUsername() ? "#6da6eb" : "#314b70"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 8
                            spacing: 10

                            Label {
                                Layout.fillWidth: true
                                text: modelData.username || ""
                                color: "#eef4ff"
                                font.bold: true
                                elide: Text.ElideRight
                            }

                            Label {
                                text: "Лаб.: " + Number(modelData.labWorkCount || 0)
                                color: "#9fb7da"
                                font.pixelSize: 12
                                Layout.preferredWidth: 90
                            }

                            Label {
                                text: modelData.updatedAt && modelData.updatedAt.length > 0 ? modelData.updatedAt : ""
                                color: "#8fa8cb"
                                font.pixelSize: 12
                                visible: text.length > 0
                                elide: Text.ElideRight
                                Layout.preferredWidth: 220
                            }

                            AppButton {
                                text: "Выбрать"
                                quiet: true
                                Layout.preferredWidth: 110
                                onClicked: page.chooseAccount(modelData.username || "")
                            }
                        }
                    }
                }

                Label {
                    visible: page.accounts.length === 0
                    Layout.fillWidth: true
                    text: "Список пуст или сервер еще не ответил."
                    color: "#8fa4c3"
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }

    Popup {
        id: confirmPopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        dim: false
        width: Math.min(page.width - 60, 860)
        height: confirmContent.implicitHeight + 28
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        Overlay.modal: Rectangle { color: "transparent" }

        background: Rectangle {
            radius: 14
            color: "#101a29"
            border.color: "#39567f"
            border.width: 1
        }

        contentItem: ColumnLayout {
            id: confirmContent
            anchors.fill: parent
            anchors.margins: 14
            spacing: 14

            Label {
                Layout.fillWidth: true
                text: "Подтвердите синхронизацию"
                color: "#edf4ff"
                font.pixelSize: 20
                font.bold: true
            }

            Label {
                Layout.fillWidth: true
                text: page.replacementText()
                color: "#b9ccea"
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 190
                spacing: 18

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 12
                    color: "#15263a"
                    border.color: pendingAction === "upload" ? "#6da6eb" : "#314b70"
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10

                        Item {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredWidth: 74
                            Layout.preferredHeight: 52

                            Rectangle {
                                x: 8
                                y: 0
                                width: 58
                                height: 36
                                radius: 5
                                color: "#203b5b"
                                border.color: "#6da6eb"
                                border.width: 2
                            }

                            Rectangle {
                                x: 31
                                y: 36
                                width: 12
                                height: 8
                                color: "#6da6eb"
                            }

                            Rectangle {
                                x: 18
                                y: 44
                                width: 38
                                height: 6
                                radius: 3
                                color: "#6da6eb"
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: "Этот компьютер"
                            color: "#edf4ff"
                            font.pixelSize: 16
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Label {
                            Layout.fillWidth: true
                            text: "Лабораторные: " + page.localLabWorks()
                            color: "#b9ccea"
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Label {
                            Layout.fillWidth: true
                            text: "Записей истории: " + page.localHistory()
                            color: "#b9ccea"
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }

                Label {
                    Layout.preferredWidth: 74
                    Layout.fillHeight: true
                    text: pendingAction === "download" ? "←" : "→"
                    color: "#78b5ff"
                    font.pixelSize: 54
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 12
                    color: "#15263a"
                    border.color: pendingAction === "download" ? "#6da6eb" : "#314b70"
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10

                        Item {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredWidth: 72
                            Layout.preferredHeight: 56

                            Rectangle {
                                x: 10
                                y: 4
                                width: 52
                                height: 14
                                radius: 4
                                color: "#203b5b"
                                border.color: "#6da6eb"
                                border.width: 2
                            }

                            Rectangle {
                                x: 10
                                y: 22
                                width: 52
                                height: 14
                                radius: 4
                                color: "#203b5b"
                                border.color: "#6da6eb"
                                border.width: 2
                            }

                            Rectangle {
                                x: 10
                                y: 40
                                width: 52
                                height: 14
                                radius: 4
                                color: "#203b5b"
                                border.color: "#6da6eb"
                                border.width: 2
                            }

                            Rectangle { x: 18; y: 10; width: 6; height: 4; radius: 2; color: "#95c8ff" }
                            Rectangle { x: 18; y: 28; width: 6; height: 4; radius: 2; color: "#95c8ff" }
                            Rectangle { x: 18; y: 46; width: 6; height: 4; radius: 2; color: "#95c8ff" }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: "Сервер"
                            color: "#edf4ff"
                            font.pixelSize: 16
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Label {
                            Layout.fillWidth: true
                            text: "Лабораторные: " + page.remoteLabWorks()
                            color: "#b9ccea"
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Label {
                            Layout.fillWidth: true
                            text: "Записей истории: " + page.remoteHistory()
                            color: "#b9ccea"
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Label {
                    Layout.fillWidth: true
                    text: "Источник: " + page.sourceLabel() + ". Получатель: " + page.targetLabel() + "."
                    color: "#8fa8cb"
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }

                AppButton {
                    text: "Отмена"
                    quiet: true
                    Layout.preferredWidth: 120
                    onClicked: confirmPopup.close()
                }

                AppButton {
                    text: "Заменить"
                    danger: true
                    Layout.preferredWidth: 140
                    onClicked: {
                        confirmPopup.close()
                        if (pendingAction === "download")
                            controller.syncFromCloud(selectedUsername(), selectedPassword())
                        else
                            controller.syncToCloud(selectedUsername(), selectedPassword())
                        pendingAction = ""
                    }
                }
            }
        }
    }

    Connections {
        target: controller

        function onCloudAccountsReady(items, errorMessage) {
            page.accounts = items || []
            if ((errorMessage || "").length > 0) {
                page.setStatus(errorMessage, true)
            } else {
                page.setStatus("Список аккаунтов обновлен. Доступно аккаунтов: " + page.accounts.length + ".", false)
            }
        }

        function onCloudVersionInfoReady(info) {
            if (page.pendingAction.length === 0)
                return
            const remoteError = (info.error || "").toString()
            if (remoteError.length > 0) {
                page.setStatus(remoteError, true)
                page.pendingAction = ""
                return
            }
            page.pendingInfo = info
            confirmPopup.open()
        }

        function onCloudSettingsChanged() {
            if (!accountField.activeFocus)
                accountField.text = controller ? controller.cloudUsername : ""
            if (!passwordField.activeFocus)
                passwordField.text = controller ? controller.cloudPassword : ""
        }
    }
}
