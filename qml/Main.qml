import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import LabTester

ApplicationWindow {
    id: root

    width: 1460
    height: 900
    minimumWidth: 1180
    minimumHeight: 720
    visible: true
    title: "ЛабТестер - Система проверки лабораторных"

    Material.theme: Material.Dark
    Material.accent: Material.Cyan
    color: "#0b1018"

    property int currentPageIndex: 0
    property string transientTopMessage: ""
    property bool navigationDebugEnabled: true
    property string pendingSyncAction: ""
    property var pendingSyncVersionInfo: ({})
    property string syncConfirmErrorText: ""
    property bool syncRemotePrecheckRequested: false
    property bool syncVersionMismatchConfirmed: false
    readonly property var pageTitles: [
        "Студенты",
        "Проверка работ",
        "Лабораторные",
        "История",
        "Синхронизация",
        "Настройки"
    ]
    onCurrentPageIndexChanged: {
        navLog("currentPageIndex -> " + currentPageIndex + " (" + pageTitles[currentPageIndex] + ")")
        root.contentItem.forceActiveFocus()
        pageTransition.restart()
    }

    function selectPage(index) {
        currentPageIndex = index
        root.contentItem.forceActiveFocus()
    }

    function navLog(message) {
        if (!navigationDebugEnabled)
            return
        console.log("[NAV][Main] " + message)
    }

    function focusItemInfo(item) {
        if (!item)
            return "null"
        if (item.objectName && item.objectName.length > 0)
            return item.objectName
        return item.toString()
    }

    function isTextInputFocused() {
        const item = activeFocusItem
        if (!item) {
            navLog("isTextInputFocused=false (no activeFocusItem)")
            return false
        }
        const result = item.hasOwnProperty("cursorPosition")
            && (item.hasOwnProperty("selectedText")
                || item.hasOwnProperty("selectionStart")
                || item.hasOwnProperty("selectionEnd"))
        navLog("isTextInputFocused=" + result + ", focus=" + focusItemInfo(item))
        return result
    }

    function movePage(step) {
        navLog("movePage(step=" + step + ")")
        if (isTextInputFocused()) {
            navLog("movePage ignored due to text input focus")
            return
        }
        const total = pageTitles.length
        if (total <= 0) {
            navLog("movePage ignored: no pages")
            return
        }
        currentPageIndex = (currentPageIndex + step + total) % total
    }

    function isModalNavigationActive() {
        const active = (submissionsPage && submissionsPage.modalNavigationActive === true)
            || (resultsPage && resultsPage.modalNavigationActive === true)
        navLog("isModalNavigationActive=" + active)
        return active
    }

    function routeHorizontal(step) {
        navLog("routeHorizontal(step=" + step + ")")
        if (isModalNavigationActive()) {
            if (submissionsPage && submissionsPage.modalNavigationActive) {
                navLog("routing to submissions modal")
                submissionsPage.handleModalTabStep(step)
            } else if (resultsPage && resultsPage.modalNavigationActive) {
                navLog("routing to results modal")
                resultsPage.handleModalTabStep(step)
            }
            return
        }
        navLog("routing to left navigation")
        movePage(step)
    }

    function routeVertical(step) {
        navLog("routeVertical(step=" + step + ")")
        if (isModalNavigationActive()) {
            navLog("routeVertical ignored because modal navigation is active")
            return
        }
        movePage(step)
    }

    function openSyncConfirmation(action) {
        pendingSyncAction = action
        syncConfirmErrorText = ""
        syncRemotePrecheckRequested = false
        syncVersionMismatchConfirmed = false
        syncUserField.text = appController.cloudUsername || ""
        pendingSyncVersionInfo = appController.cloudVersionInfo(syncUserField.text, false)
        syncConfirmPopup.open()
    }

    function executeSyncAction() {
        const username = syncUserField.text.trim()
        if (username.length === 0) {
            syncConfirmErrorText = "Укажите имя пользователя."
            return
        }

        appController.setCloudUsername(username)

        if (pendingSyncAction === "download") {
            if (!syncRemotePrecheckRequested) {
                syncRemotePrecheckRequested = true
                if (!appController.requestCloudVersionInfo(username, true)) {
                    syncRemotePrecheckRequested = false
                    syncConfirmErrorText = appController.lastRunMessage || "Не удалось проверить состояние облака."
                }
                return
            }

            const remoteError = (pendingSyncVersionInfo.error || "").toString().trim()
            if (remoteError.length > 0) {
                syncConfirmErrorText = remoteError
                return
            }

            if (pendingSyncVersionInfo.remoteExists !== true) {
                syncConfirmErrorText = "Облачных сохранений для пользователя \"" + username + "\" нет."
                return
            }

            const localAppVersion = (pendingSyncVersionInfo.localAppVersion || "0.2v").toString().trim()
            const remoteAppVersion = (pendingSyncVersionInfo.remoteAppVersion || "").toString().trim()
            if (remoteAppVersion.length > 0
                    && localAppVersion.length > 0
                    && remoteAppVersion !== localAppVersion
                    && !syncVersionMismatchConfirmed) {
                syncVersionMismatchConfirmed = true
                syncConfirmErrorText = "У вас версия " + localAppVersion
                                      + ", в облаке сохранение из версии " + remoteAppVersion
                                      + ". Нажмите \"Загрузить из облака\" еще раз для подтверждения."
                return
            }
        }

        let ok = false
        if (pendingSyncAction === "upload")
            ok = appController.syncToCloud(username)
        else if (pendingSyncAction === "download")
            ok = appController.syncFromCloud(username)

        if (ok) {
            syncConfirmPopup.close()
        }
    }

    onActiveFocusItemChanged: navLog("activeFocusItem -> " + focusItemInfo(activeFocusItem))

    Shortcut {
        sequence: "Tab"
        context: Qt.ApplicationShortcut
        onActivated: {
            root.navLog("Shortcut Tab activated")
            root.routeHorizontal(1)
        }
    }

    Shortcut {
        sequence: "W"
        context: Qt.ApplicationShortcut
        enabled: !root.isModalNavigationActive()
        onActivated: {
            root.navLog("Shortcut W activated")
            root.movePage(-1)
        }
    }

    Shortcut {
        sequence: "S"
        context: Qt.ApplicationShortcut
        enabled: !root.isModalNavigationActive()
        onActivated: {
            root.navLog("Shortcut S activated")
            root.movePage(1)
        }
    }

    Shortcut {
        sequence: "Ц"
        context: Qt.ApplicationShortcut
        enabled: !root.isModalNavigationActive()
        onActivated: {
            root.navLog("Shortcut Ц activated")
            root.movePage(-1)
        }
    }

    Shortcut {
        sequence: "Ы"
        context: Qt.ApplicationShortcut
        enabled: !root.isModalNavigationActive()
        onActivated: {
            root.navLog("Shortcut Ы activated")
            root.movePage(1)
        }
    }

    Shortcut {
        sequence: "A"
        context: Qt.ApplicationShortcut
        onActivated: {
            root.navLog("Shortcut A activated")
            root.routeHorizontal(-1)
        }
    }

    Shortcut {
        sequence: "D"
        context: Qt.ApplicationShortcut
        onActivated: {
            root.navLog("Shortcut D activated")
            root.routeHorizontal(1)
        }
    }

    Shortcut {
        sequence: "Ф"
        context: Qt.ApplicationShortcut
        onActivated: {
            root.navLog("Shortcut Ф activated")
            root.routeHorizontal(-1)
        }
    }

    Shortcut {
        sequence: "В"
        context: Qt.ApplicationShortcut
        onActivated: {
            root.navLog("Shortcut В activated")
            root.routeHorizontal(1)
        }
    }

    Connections {
        target: appController

        function onLastRunMessageChanged() {
            root.transientTopMessage = appController.lastRunMessage
            if (root.transientTopMessage.length > 0) {
                topMessageTimer.restart()
            } else {
                topMessageTimer.stop()
            }
        }

        function onCloudVersionInfoReady(info) {
            if (syncConfirmPopup.visible) {
                pendingSyncVersionInfo = info
                if (syncRemotePrecheckRequested && pendingSyncAction === "download") {
                    root.executeSyncAction()
                }
            }
        }
    }

    Timer {
        id: topMessageTimer
        interval: 10000
        repeat: false
        onTriggered: root.transientTopMessage = ""
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#0b1018" }
            GradientStop { position: 0.45; color: "#111b2a" }
            GradientStop { position: 1.0; color: "#0f1723" }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 14

        AppCard {
            Layout.preferredWidth: 240
            Layout.fillHeight: true
            baseColor: "#111b2a"
            borderColor: "#2f4667"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 14

                Label {
                    Layout.fillWidth: true
                    text: ""
                    visible: false
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Image {
                        source: "qrc:/resources/icons/labtester_icon.svg"
                        Layout.preferredWidth: 34
                        Layout.preferredHeight: 34
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        antialiasing: true
                    }

                    Label {
                        text: "ЛабТестер"
                        font.pixelSize: 28
                        font.bold: true
                        color: "#edf4ff"
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignLeft
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#273b57"
                }

                Item {
                    id: navButtonsHost
                    Layout.fillWidth: true
                    implicitHeight: navButtonsColumn.implicitHeight

                    Rectangle {
                        id: navActiveHighlight
                        z: 0
                        x: 0
                        width: navButtonsHost.width
                        height: 50
                        radius: 14
                        color: "#203b5b"
                        border.width: 1
                        border.color: "#5e8fca"
                        opacity: 0.78
                        visible: root.currentPageIndex >= 0 && root.currentPageIndex < root.pageTitles.length

                        y: {
                            switch (root.currentPageIndex) {
                            case 0: return navBtnStudents.y
                            case 1: return navBtnChecks.y
                            case 2: return navBtnLabs.y
                            case 3: return navBtnHistory.y
                            case 4: return navBtnSync.y
                            case 5: return navBtnSettings.y
                            default: return 0
                            }
                        }

                        Behavior on y {
                            NumberAnimation {
                                duration: 220
                                easing.type: Easing.OutCubic
                            }
                        }
                    }

                    ColumnLayout {
                        id: navButtonsColumn
                        anchors.fill: parent
                        spacing: 8

                        AppNavButton {
                            id: navBtnStudents
                            z: 1
                            Layout.fillWidth: true
                            text: root.pageTitles[0]
                            iconSource: "qrc:/resources/icons/nav_students.svg"
                            selected: root.currentPageIndex === 0
                            onClicked: root.selectPage(0)
                        }

                        AppNavButton {
                            id: navBtnChecks
                            z: 1
                            Layout.fillWidth: true
                            text: root.pageTitles[1]
                            iconSource: "qrc:/resources/icons/nav_checks.svg"
                            selected: root.currentPageIndex === 1
                            onClicked: root.selectPage(1)
                        }

                        AppNavButton {
                            id: navBtnLabs
                            z: 1
                            Layout.fillWidth: true
                            text: root.pageTitles[2]
                            iconSource: "qrc:/resources/icons/nav_labs.svg"
                            selected: root.currentPageIndex === 2
                            onClicked: root.selectPage(2)
                        }

                        AppNavButton {
                            id: navBtnHistory
                            z: 1
                            Layout.fillWidth: true
                            text: root.pageTitles[3]
                            iconSource: "qrc:/resources/icons/nav_history.svg"
                            selected: root.currentPageIndex === 3
                            onClicked: root.selectPage(3)
                        }

                        AppNavButton {
                            id: navBtnSync
                            z: 1
                            Layout.fillWidth: true
                            text: root.pageTitles[4]
                            iconSource: "qrc:/resources/icons/cloud_sync.svg"
                            selected: root.currentPageIndex === 4
                            onClicked: root.selectPage(4)
                        }

                        AppNavButton {
                            id: navBtnSettings
                            z: 1
                            Layout.fillWidth: true
                            text: root.pageTitles[5]
                            iconSource: "qrc:/resources/icons/nav_settings.svg"
                            selected: root.currentPageIndex === 5
                            onClicked: root.selectPage(5)
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            AppCard {
                Layout.fillWidth: true
                Layout.preferredHeight: appController.testsRunning ? 110 : 78
                baseColor: "#101827"
                borderColor: "#314c72"

                Rectangle {
                    anchors.fill: parent
                    radius: 12
                    color: "transparent"
                    border.width: 1
                    border.color: "#1b2f49"
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#13233a" }
                        GradientStop { position: 1.0; color: "#0f1b2d" }
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Label {
                            text: root.pageTitles[root.currentPageIndex]
                            font.pixelSize: 24
                            font.bold: true
                            color: "#edf4ff"
                        }

                        Item { Layout.fillWidth: true }

                        Label {
                            text: root.transientTopMessage
                            color: "#a8bad7"
                            font.pixelSize: 12
                            visible: text.length > 0 && !appController.testsRunning
                            elide: Text.ElideRight
                            horizontalAlignment: Text.AlignRight
                            Layout.preferredWidth: 360
                        }

                    }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: appController.testsRunning
                        spacing: 10

                        ProgressBar {
                            Layout.fillWidth: true
                            from: 0
                            to: 1
                            value: appController.testProgressValue

                            Behavior on value {
                                NumberAnimation {
                                    duration: 240
                                    easing.type: Easing.InOutCubic
                                }
                            }
                        }

                        Label {
                            text: appController.testProgressText
                            color: "#b8c7df"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                            Layout.preferredWidth: 340
                        }
                    }
                }
            }

            AppCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                baseColor: "#0f1723"
                borderColor: "#2f4667"

                StackLayout {
                    id: pagesStack
                    anchors.fill: parent
                    anchors.margins: 12
                    currentIndex: root.currentPageIndex

                    DashboardPage { id: dashboardPage; controller: appController }
                    SubmissionsPage { id: submissionsPage; controller: appController }
                    ResultsPage { id: resultsPage; controller: appController }
                    HistoryPage { id: historyPage; controller: appController }
                    SyncPage {
                        id: syncPage
                        controller: appController
                    }
                    SettingsPage {
                        id: settingsPage
                        controller: appController
                    }
                }

                SequentialAnimation {
                    id: pageTransition
                    running: false

                    ParallelAnimation {
                        NumberAnimation {
                            target: pagesStack
                            property: "opacity"
                            from: 0.78
                            to: 1.0
                            duration: 220
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: pagesStack
                            property: "scale"
                            from: 0.992
                            to: 1.0
                            duration: 220
                            easing.type: Easing.OutCubic
                        }
                    }
                }
            }
        }
    }

    Popup {
        id: syncConfirmPopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        dim: false
        width: Math.min(root.width - 40, 640)
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
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            RowLayout {
                Layout.fillWidth: true

                Label {
                    text: pendingSyncAction === "upload"
                        ? "Подтвердите выгрузку в облако"
                        : "Подтвердите загрузку из облака"
                    color: "#edf4ff"
                    font.pixelSize: 17
                    font.bold: true
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                ToolButton {
                    id: closeSyncPopupButton
                    implicitWidth: 36
                    implicitHeight: 36
                    hoverEnabled: true
                    focusPolicy: Qt.NoFocus
                    background: Rectangle {
                        radius: 10
                        color: closeSyncPopupButton.hovered ? "#2a415f" : "#1a2a40"
                        border.color: closeSyncPopupButton.hovered ? "#5889c3" : "#36557a"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: "✕"
                        color: "#d8e8ff"
                        font.pixelSize: 16
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: syncConfirmPopup.close()
                }
            }

            Label {
                text: pendingSyncAction === "upload"
                    ? "Локальные данные будут сохранены в облачную БД для выбранного пользователя."
                    : "Локальные данные будут заменены данными из облака для выбранного пользователя."
                color: "#b4c7e3"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            TextField {
                id: syncUserField
                Layout.fillWidth: true
                placeholderText: "Имя пользователя"
                color: "#eef4ff"
                placeholderTextColor: "#7c94b8"
                selectByMouse: true
                onTextChanged: {
                    syncConfirmErrorText = ""
                    syncRemotePrecheckRequested = false
                    syncVersionMismatchConfirmed = false
                    pendingSyncVersionInfo = appController.cloudVersionInfo(syncUserField.text.trim(), false)
                }
                background: Rectangle {
                    radius: 10
                    border.width: 1
                    border.color: syncUserField.activeFocus ? "#5b85bd" : "#344c6e"
                    gradient: Gradient {
                        GradientStop { position: 0; color: "#1b2636" }
                        GradientStop { position: 1; color: "#141d2b" }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 10
                color: "#152336"
                border.color: "#2f4868"
                border.width: 1
                implicitHeight: versionInfoColumn.implicitHeight + 14

                ColumnLayout {
                    id: versionInfoColumn
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    Label {
                        text: "Версия программы: " + (pendingSyncVersionInfo.localAppVersion || "0.2v")
                        color: "#9fb7da"
                    }
                    Label {
                        text: {
                            const hasError = (pendingSyncVersionInfo.error || "").length > 0
                            const checked = syncRemotePrecheckRequested || hasError || pendingSyncVersionInfo.remoteExists
                            if (!checked)
                                return "Облачная версия: не проверена"
                            return pendingSyncVersionInfo.remoteExists
                                ? ("Облачная версия: " + Number(pendingSyncVersionInfo.remoteVersion || 0))
                                : "Облачная версия: отсутствует"
                        }
                        color: "#c4d6ef"
                    }
                    Label {
                        visible: (pendingSyncVersionInfo.remoteAppVersion || "").length > 0
                        text: "Версия программы в облаке: " + pendingSyncVersionInfo.remoteAppVersion
                        color: "#9fb7da"
                    }
                    Label {
                        visible: (pendingSyncVersionInfo.remoteUpdatedAt || "").length > 0
                        text: "Последнее обновление (" + (pendingSyncVersionInfo.remoteUsername || syncUserField.text.trim()) + "): " + pendingSyncVersionInfo.remoteUpdatedAt
                        color: "#9fb7da"
                    }
                    Label {
                        visible: pendingSyncVersionInfo.remoteNewer === true
                        text: "В облаке версия новее локальной."
                        color: "#f0ca71"
                        font.bold: true
                    }
                    Label {
                        visible: pendingSyncVersionInfo.remoteSchemaNewer === true
                        text: "В облаке более новая схема данных. Требуется обновление приложения."
                        color: "#ffb9c6"
                        wrapMode: Text.WordWrap
                    }
                    Label {
                        visible: (pendingSyncVersionInfo.error || "").length > 0
                        text: pendingSyncVersionInfo.error || ""
                        color: "#ffb9c6"
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Label {
                visible: syncConfirmErrorText.length > 0
                text: syncConfirmErrorText
                color: "#ffb9c6"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Item { Layout.fillWidth: true }

                AppButton {
                    text: "Отмена"
                    quiet: true
                    Layout.preferredWidth: 120
                    onClicked: syncConfirmPopup.close()
                }

                AppButton {
                    text: pendingSyncAction === "upload" ? "Сохранить в облако" : "Загрузить из облака"
                    Layout.preferredWidth: 190
                    enabled: !appController.cloudSyncInProgress
                    onClicked: root.executeSyncAction()
                }
            }
        }
    }
}
