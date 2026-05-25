import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabTester

Item {
    id: page

    property var controller
    property var selectedLab: ({})
    property var pendingDeleteLab: ({})
    property string templatePreview: ""
    property string testsPreview: ""
    property bool templatePreviewLoaded: false
    property bool testsPreviewLoaded: false
    property bool templatePreviewLoading: false
    property bool testsPreviewLoading: false
    property bool labTabSwitching: false
    property bool navigationDebugEnabled: true
    readonly property bool modalNavigationActive:
        labDialog.visible || labEditorDialog.visible || deleteLabPopup.visible

    readonly property string defaultLabTemplate:
        "#include \"cc.h\"\n\n"
        + "void bubblesort(int arr[], int n)\n{\n    // TODO\n}\n\n"
        + "void selectionSort(int arr[], int n)\n{\n    // TODO\n}\n\n"
        + "void insertionSort(int arr[], int n)\n{\n    // TODO\n}\n\n"
        + "void quickSort(int arr[], int low, int high)\n{\n    // TODO\n}\n\n"
        + "void mergeSort(int arr[], int left, int right)\n{\n    // TODO\n}\n\n"
        + "void sorting_heap(int arr[], int n)\n{\n    // TODO\n}\n\n"
        + "bool lexSort(const std::string& a, const std::string& b)\n{\n    // TODO\n    return a < b;\n}\n\n"
        + "void bubblesortForLex(std::vector<std::string>& arr)\n{\n    // TODO\n}\n\n"
        + "bool isSorted(const int arr[], int n)\n{\n    // TODO\n    return true;\n}\n\n"
        + "bool isSortedLex(const std::vector<std::string>& arr)\n{\n    // TODO\n    return true;\n}\n"

    function fileName(path) {
        if (!path || path.toString().trim().length === 0)
            return "—"
        const normalized = path.toString().replace(/\\/g, "/")
        const parts = normalized.split("/")
        const last = parts.length > 0 ? parts[parts.length - 1] : normalized
        return last && last.length > 0 ? last : normalized
    }

    function previewOrPlaceholder(path, placeholder) {
        if (!controller || !path || path.toString().trim().length === 0)
            return placeholder

        const text = controller.readTextFilePreviewLimited(path, 6000)
        if (text.startsWith("Файл не найден:") || text.startsWith("Путь к файлу не указан."))
            return placeholder
        return text
    }

    function loadTemplatePreview() {
        if (templatePreviewLoaded || templatePreviewLoading)
            return

        templatePreviewLoading = true
        Qt.callLater(function() {
            const templatePath = selectedLab && selectedLab.templateFile
                ? selectedLab.templateFile
                : ""
            templatePreview = previewOrPlaceholder(templatePath, defaultLabTemplate)
            templatePreviewLoaded = true
            templatePreviewLoading = false
        })
    }

    function loadTestsPreview() {
        if (testsPreviewLoaded || testsPreviewLoading)
            return

        testsPreviewLoading = true
        Qt.callLater(function() {
            const suitePath = selectedLab && selectedLab.testSuitePath
                ? selectedLab.testSuitePath
                : ""
            testsPreview = previewOrPlaceholder(suitePath, "Тестовый файл для этой лабораторной пока не добавлен.")
            testsPreviewLoaded = true
            testsPreviewLoading = false
        })
    }

    function openLabDetails(labData) {
        navLog("openLabDetails id=" + (labData && labData.id ? labData.id : 0))
        selectedLab = labData
        templatePreviewLoaded = false
        testsPreviewLoaded = false
        templatePreviewLoading = false
        testsPreviewLoading = false
        templatePreview = "Загрузка..."
        testsPreview = "Откройте вкладку \"Тесты\" для загрузки превью."
        labTabs.currentIndex = 0
        labDialog.open()
        loadTemplatePreview()
    }

    function openCreateLabDialog() {
        if (!labEditorDialog)
            return
        labEditorDialog.openForCreate()
    }

    function openEditLabDialog(labData) {
        if (!labEditorDialog || !labData)
            return
        labEditorDialog.openForEdit(labData)
    }

    function requestDeleteLab(labData) {
        if (!labData || !labData.id)
            return
        pendingDeleteLab = labData
        deleteLabPopup.open()
    }

    function performDeleteLab() {
        if (!controller || !pendingDeleteLab || !pendingDeleteLab.id)
            return
        controller.removeLabWork(Number(pendingDeleteLab.id))
        if (selectedLab && Number(selectedLab.id) === Number(pendingDeleteLab.id))
            labDialog.close()
        pendingDeleteLab = ({})
        deleteLabPopup.close()
    }

    function markLabTabSwitching() {
        labTabSwitching = true
        labTabSwitchTimer.restart()
    }

    function navLog(message) {
        if (!navigationDebugEnabled)
            return
        console.log("[NAV][ResultsPage] " + message)
    }

    function switchLabTab(step) {
        const total = 4
        const next = (labTabs.currentIndex + step + total) % total
        if (next === labTabs.currentIndex)
            return
        navLog("switchLabTab step=" + step + ", from=" + labTabs.currentIndex + ", to=" + next)
        labTabs.currentIndex = next
    }

    function handleModalTabStep(step) {
        navLog("handleModalTabStep step=" + step + ", visible=" + labDialog.visible)
        if (!labDialog.visible)
            return
        switchLabTab(step)
    }

    Timer {
        id: labTabSwitchTimer
        interval: 180
        repeat: false
        onTriggered: page.labTabSwitching = false
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                text: "Каталог лабораторных"
                color: "#d8e8ff"
                font.pixelSize: 16
                font.bold: true
                Layout.fillWidth: true
            }

            AppButton {
                text: "Добавить"
                Layout.preferredWidth: 128
                onClicked: page.openCreateLabDialog()
            }
        }

        ListView {
            id: labsList
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10
            clip: true
            model: controller ? controller.labWorks : []
            reuseItems: false

            populate: Transition {
                NumberAnimation {
                    property: "opacity"
                    from: 0
                    to: 1
                    duration: 180
                }
            }

            add: Transition {
                ParallelAnimation {
                    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 170 }
                    NumberAnimation { property: "x"; from: 20; to: 0; duration: 180; easing.type: Easing.OutCubic }
                }
            }

            remove: Transition {
                ParallelAnimation {
                    NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 }
                    NumberAnimation { property: "x"; from: 0; to: 20; duration: 120; easing.type: Easing.InCubic }
                }
            }

            delegate: AppCard {
                required property var modelData

                width: ListView.view.width
                implicitHeight: 138
                baseColor: labCardHover.containsMouse ? "#1c304a" : "#141f30"
                borderColor: "#345074"

                MouseArea {
                    id: labCardHover
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    z: 0
                    onClicked: page.openLabDetails(modelData)
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10
                    z: 1

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Label {
                            text: "id " + Number(modelData.id)
                            color: "#86a4cc"
                            font.pixelSize: 11
                        }

                        Label {
                            text: modelData.title
                            color: "#edf4ff"
                            font.pixelSize: 17
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Label {
                            text: modelData.description
                            color: "#b7c8df"
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }

                    }

                    ColumnLayout {
                        spacing: 6
                        Layout.alignment: Qt.AlignTop | Qt.AlignRight
                        Layout.topMargin: 2

                        RowLayout {
                            spacing: 8

                            ToolButton {
                                id: editLabButton
                                implicitWidth: 40
                                implicitHeight: 40
                                hoverEnabled: true
                                focusPolicy: Qt.NoFocus
                                background: Rectangle {
                                    radius: 10
                                    color: editLabButton.hovered ? "#274261" : "#1a2b41"
                                    border.color: editLabButton.hovered ? "#6aa4ea" : "#35567f"
                                    border.width: 1
                                }
                                contentItem: Text {
                                    text: "✎"
                                    color: "#d9e9ff"
                                    font.pixelSize: 15
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: page.openEditLabDialog(modelData)
                            }

                            ToolButton {
                                id: deleteLabButton
                                implicitWidth: 40
                                implicitHeight: 40
                                hoverEnabled: true
                                focusPolicy: Qt.NoFocus
                                background: Rectangle {
                                    radius: 10
                                    color: deleteLabButton.hovered ? "#5c2a37" : "#3b1f29"
                                    border.color: deleteLabButton.hovered ? "#f08ea6" : "#8f4b5d"
                                    border.width: 1
                                }
                                contentItem: Text {
                                    text: "×"
                                    color: "#ffe5ec"
                                    font.pixelSize: 18
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: page.requestDeleteLab(modelData)
                            }

                            Label {
                                text: "›"
                                color: "#b8d5ff"
                                font.pixelSize: 28
                            }
                        }
                    }
                }
            }
        }
    }

    Popup {
        id: labDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        onOpened: {
            navLog("labDialog opened")
            labDialog.forceActiveFocus()
        }
        onClosed: navLog("labDialog closed")
        dim: false
        width: Math.min(page.width - 40, 960)
        height: Math.min(page.height - 40, 670)
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        Shortcut {
            sequence: "Tab"
            context: Qt.ApplicationShortcut
            enabled: labDialog.visible
            onActivated: page.switchLabTab(1)
        }

        Shortcut {
            sequence: "A"
            context: Qt.ApplicationShortcut
            enabled: labDialog.visible
            onActivated: page.switchLabTab(-1)
        }

        Shortcut {
            sequence: "D"
            context: Qt.ApplicationShortcut
            enabled: labDialog.visible
            onActivated: page.switchLabTab(1)
        }

        Shortcut {
            sequence: "Ф"
            context: Qt.ApplicationShortcut
            enabled: labDialog.visible
            onActivated: page.switchLabTab(-1)
        }

        Shortcut {
            sequence: "В"
            context: Qt.ApplicationShortcut
            enabled: labDialog.visible
            onActivated: page.switchLabTab(1)
        }

        enter: Transition {
            ParallelAnimation {
                NumberAnimation {
                    property: "opacity"
                    from: 0
                    to: 1
                    duration: 180
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    property: "scale"
                    from: 0.95
                    to: 1.0
                    duration: 180
                    easing.type: Easing.OutCubic
                }
            }
        }

        exit: Transition {
            ParallelAnimation {
                NumberAnimation {
                    property: "opacity"
                    from: 1
                    to: 0
                    duration: 120
                    easing.type: Easing.InCubic
                }
                NumberAnimation {
                    property: "scale"
                    from: 1.0
                    to: 0.96
                    duration: 120
                    easing.type: Easing.InCubic
                }
            }
        }

        Overlay.modal: Rectangle {
            color: "transparent"
        }

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
                    text: selectedLab.title ? selectedLab.title : "Детали лабораторной"
                    color: "#edf4ff"
                    font.pixelSize: 17
                    font.bold: true
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                ToolButton {
                    id: closeLabButton
                    implicitWidth: 36
                    implicitHeight: 36
                    hoverEnabled: true
                    focusPolicy: Qt.NoFocus
                    clip: true
                    background: Rectangle {
                        radius: 10
                        color: closeLabButton.hovered ? "#2a415f" : "#1a2a40"
                        border.color: closeLabButton.hovered ? "#5889c3" : "#36557a"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: "✕"
                        color: "#d8e8ff"
                        font.pixelSize: 16
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: labDialog.close()
                }
            }

            TabBar {
                id: labTabs
                Layout.fillWidth: true
                currentIndex: 0
                spacing: 6
                onCurrentIndexChanged: {
                    page.markLabTabSwitching()
                    if (currentIndex === 1) {
                        page.loadTemplatePreview()
                    } else if (currentIndex === 2) {
                        page.loadTestsPreview()
                    }
                }
                background: Rectangle {
                    id: labTabsBg
                    color: "#122034"
                    radius: 10
                    border.color: "#2f4868"

                    Rectangle {
                        id: labTabsIndicator
                        y: 3
                        height: parent.height - 6
                        radius: 8
                        color: "#2f5f95"
                        border.color: "#4f8bd0"
                        border.width: 1

                        x: {
                            switch (labTabs.currentIndex) {
                            case 0: return descTabButton.x
                            case 1: return codeTabButton.x
                            case 2: return testsTabButton.x
                            case 3: return pathsTabButton.x
                            default: return descTabButton.x
                            }
                        }

                        width: {
                            switch (labTabs.currentIndex) {
                            case 0: return descTabButton.width
                            case 1: return codeTabButton.width
                            case 2: return testsTabButton.width
                            case 3: return pathsTabButton.width
                            default: return descTabButton.width
                            }
                        }

                        Behavior on x {
                            NumberAnimation {
                                duration: 180
                                easing.type: Easing.OutCubic
                            }
                        }

                        Behavior on width {
                            NumberAnimation {
                                duration: 180
                                easing.type: Easing.OutCubic
                            }
                        }
                    }
                }

                TabButton {
                    id: descTabButton
                    text: "Описание"
                    hoverEnabled: true
                    background: Rectangle {
                        anchors.fill: parent
                        anchors.margins: 3
                        radius: 8
                        color: descTabButton.hovered && !descTabButton.checked && !page.labTabSwitching
                            ? "#1e3551"
                            : "transparent"

                        Behavior on color {
                            ColorAnimation { duration: 120 }
                        }
                    }
                }
                TabButton {
                    id: codeTabButton
                    text: "Код"
                    hoverEnabled: true
                    background: Rectangle {
                        anchors.fill: parent
                        anchors.margins: 3
                        radius: 8
                        color: codeTabButton.hovered && !codeTabButton.checked && !page.labTabSwitching
                            ? "#1e3551"
                            : "transparent"

                        Behavior on color {
                            ColorAnimation { duration: 120 }
                        }
                    }
                }
                TabButton {
                    id: testsTabButton
                    text: "Тесты"
                    hoverEnabled: true
                    background: Rectangle {
                        anchors.fill: parent
                        anchors.margins: 3
                        radius: 8
                        color: testsTabButton.hovered && !testsTabButton.checked && !page.labTabSwitching
                            ? "#1e3551"
                            : "transparent"

                        Behavior on color {
                            ColorAnimation { duration: 120 }
                        }
                    }
                }
                TabButton {
                    id: pathsTabButton
                    text: "Пути"
                    hoverEnabled: true
                    background: Rectangle {
                        anchors.fill: parent
                        anchors.margins: 3
                        radius: 8
                        color: pathsTabButton.hovered && !pathsTabButton.checked && !page.labTabSwitching
                            ? "#1e3551"
                            : "transparent"

                        Behavior on color {
                            ColorAnimation { duration: 120 }
                        }
                    }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: labTabs.currentIndex

                ScrollView {
                    clip: true
                    Column {
                        width: parent.width
                        spacing: 10

                        Label {
                            width: parent.width
                            text: selectedLab.description || ""
                            color: "#d8e6ff"
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                AppCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    baseColor: "#152336"
                    borderColor: "#2f4868"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        Label {
                            text: "Шаблон для студента"
                            color: "#8fc4ff"
                            font.bold: true
                        }

                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            TextArea {
                                readOnly: true
                                text: templatePreview
                                wrapMode: TextArea.NoWrap
                                color: "#e8f0ff"
                                font.family: "Consolas"
                                background: Rectangle {
                                    color: "#152336"
                                    radius: 10
                                    border.color: "#2f4868"
                                }
                            }
                        }
                    }
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    TextArea {
                        readOnly: true
                        text: testsPreview
                        wrapMode: TextArea.WrapAnywhere
                        color: "#e8f0ff"
                        font.family: "Consolas"
                        placeholderText: "Превью тестов недоступно."
                        background: Rectangle {
                            color: "#152336"
                            radius: 10
                            border.color: "#2f4868"
                        }
                    }
                }

                AppCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    baseColor: "#152336"
                    borderColor: "#2f4868"

                    Column {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 10

                        Label {
                            text: "Файлы лабораторной"
                            color: "#edf4ff"
                            font.bold: true
                        }
                        Label {
                            text: "Тесты: " + fileName(selectedLab.testSuitePath)
                            color: "#c9d8ef"
                            wrapMode: Text.WrapAnywhere
                        }
                        Label {
                            text: "Интерфейс: " + fileName(selectedLab.referenceHeaderPath)
                            color: "#c9d8ef"
                            wrapMode: Text.WrapAnywhere
                        }
                        Label {
                            text: "Шаблон: " + fileName(selectedLab.templateFile)
                            color: "#c9d8ef"
                            wrapMode: Text.WrapAnywhere
                        }
                    }
                }
            }
        }
    }

    LabEditorDialog {
        id: labEditorDialog
        controller: page.controller
    }

    Popup {
        id: deleteLabPopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        dim: false
        width: Math.min(page.width - 40, 460)
        height: 224
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        Overlay.modal: Rectangle {
            color: "transparent"
        }

        background: Rectangle {
            radius: 14
            color: "#101a29"
            border.color: "#39567f"
            border.width: 1
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 12

            Label {
                text: "Удалить лабораторную?"
                color: "#edf4ff"
                font.pixelSize: 17
                font.bold: true
            }

            Label {
                Layout.fillWidth: true
                text: pendingDeleteLab && pendingDeleteLab.title
                    ? pendingDeleteLab.title
                    : ""
                color: "#b8cbe5"
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                text: "Удаление скроет лабораторную из каталога и уберет связанные активные работы из списка проверки."
                color: "#91a7c8"
                wrapMode: Text.WordWrap
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Item { Layout.fillWidth: true }

                AppButton {
                    text: "Отмена"
                    quiet: true
                    Layout.preferredWidth: 120
                    onClicked: deleteLabPopup.close()
                }

                AppButton {
                    text: "Удалить"
                    danger: true
                    Layout.preferredWidth: 130
                    onClicked: page.performDeleteLab()
                }
            }
        }
    }
}



