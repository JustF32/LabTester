import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import LabTester

Item {
    id: page

    property var controller
    property var selectedTestCase: ({})
    property var outputRows: []
    property int selectedStudentId: 0
    property bool globalDropActive: false
    property bool testCaseTabSwitching: false
    property bool navigationDebugEnabled: true
    readonly property bool modalNavigationActive: testCaseDialog.visible || batchImportDialog.visible

    function importLog(message) {
        console.log("[SubmissionsPage] " + message)
    }

    function navLog(message) {
        if (!navigationDebugEnabled)
            return
        console.log("[NAV][SubmissionsPage] " + message)
    }

    function localPathFromUrlLike(value) {
        if (value === undefined || value === null)
            return ""

        if (typeof value === "object" && typeof value.toLocalFile === "function") {
            const local = value.toLocalFile()
            return local ? local.toString().trim() : ""
        }

        let text = value.toString().trim()
        if (text.length === 0)
            return ""

        if (text.startsWith("file://")) {
            text = text.replace(/^file:\/+/, "/")
            if (/^\/[A-Za-z]:/.test(text))
                text = text.substring(1)
            try {
                text = decodeURIComponent(text)
            } catch (error) {
                importLog("decodeURIComponent failed for: " + text)
            }
        }

        return text
    }

    function splitLines(value) {
        const normalized = (value || "").toString().replace(/\r\n/g, "\n")
        if (normalized.length === 0)
            return []
        return normalized.split("\n")
    }

    function buildOutputRows(expectedText, actualText, testPassed) {
        if (isDiagnosticOutput(actualText)) {
            return [{
                line: 1,
                expectedLine: (expectedText || "").toString().trim(),
                actualLine: actualText.toString().trim(),
                match: testPassed,
                diagnostic: true
            }]
        }

        const expectedLines = splitLines(expectedText)
        const actualLines = splitLines(actualText)
        const total = Math.max(expectedLines.length, actualLines.length, 1)
        const rows = []

        for (let i = 0; i < total; ++i) {
            const expectedLine = i < expectedLines.length ? expectedLines[i] : ""
            const actualLine = i < actualLines.length ? actualLines[i] : ""
            rows.push({
                line: i + 1,
                expectedLine: expectedLine,
                actualLine: actualLine,
                match: testPassed || expectedLine === actualLine,
                diagnostic: false
            })
        }

        return rows
    }

    function isDiagnosticOutput(text) {
        const value = (text || "").toString()
        return hasTechnicalTrace(value)
            || value.indexOf("Место:") !== -1
            || value.indexOf("Ожидалось равенство") !== -1
            || value.indexOf("Фактически:") !== -1
    }

    function hasTechnicalTrace(text) {
        const value = (text || "").toString()
        return value.indexOf(".cpp:") !== -1
            || value.indexOf("Value of:") !== -1
            || value.indexOf("Expected:") !== -1
            || value.indexOf("Actual:") !== -1
            || value.indexOf("C:\\\\") !== -1
    }

    function visibleFailureLine(text) {
        const lines = (text || "").toString().split(/\r?\n/)
        for (let i = 0; i < lines.length; ++i) {
            const line = lines[i].trim()
            const lower = line.toLowerCase()
            if (lower.indexOf("runtime error") !== -1
                    || lower.indexOf("превышен лимит времени") !== -1
                    || lower.indexOf("timeout") !== -1
                    || lower.indexOf("time limit") !== -1)
                return line
        }
        return ""
    }

    function formatFailureDetails(text) {
        const rawLines = (text || "").toString().split(/\r?\n/)
        const lines = []
        let afterEqualityHeader = false
        let nextPlainLineIsExpected = false

        for (let i = 0; i < rawLines.length; ++i) {
            let line = rawLines[i].trim()
            if (line.length === 0)
                continue

            const pathMatch = line.match(/([^\/\\]+\.cpp:\d+)/)
            if (pathMatch) {
                lines.push("Место: " + pathMatch[1])
                continue
            }

            if (line === "Expected equality of these values:") {
                lines.push("Ожидалось равенство значений:")
                afterEqualityHeader = true
                continue
            }

            if (line.indexOf("Which is:") === 0) {
                lines.push("Фактически: " + line.substring("Which is:".length).trim())
                nextPlainLineIsExpected = true
                afterEqualityHeader = false
                continue
            }

            if (nextPlainLineIsExpected) {
                lines.push("Ожидалось: " + line)
                nextPlainLineIsExpected = false
                continue
            }

            if (afterEqualityHeader) {
                lines.push("Проверяемое значение: " + line)
                afterEqualityHeader = false
                continue
            }

            if (line.indexOf("Expected:") === 0) {
                lines.push("Ожидалось: " + line.substring("Expected:".length).trim())
                continue
            }

            if (line.indexOf("Actual:") === 0) {
                lines.push("Фактически: " + line.substring("Actual:".length).trim())
                continue
            }

            lines.push(line)
        }

        return lines.join("\n")
    }

    function normalizedActualOutput(caseData) {
        if (!caseData)
            return "Нет данных."
        const actual = (caseData.actualOutput || "").toString().trim()
        if (caseData.isBuildErrorCase) {
            if (actual.length > 0)
                return actual
            const failure = (caseData.failureDetails || "").toString().trim()
            return failure.length > 0 ? failure : "Нет данных."
        }
        const formattedFailure = formatFailureDetails(caseData.failureDetails || caseData.message || "")
        if (!caseData.passed && formattedFailure.length > 0 && actual.length > 0 && !hasTechnicalTrace(actual))
            return actual + "\n\n" + formattedFailure
        if (actual.length === 0 || hasTechnicalTrace(actual)) {
            const visibleFailure = visibleFailureLine(caseData.failureDetails || caseData.message || actual)
            if (visibleFailure.length > 0)
                return visibleFailure
            const failure = formatFailureDetails(caseData.failureDetails || caseData.message || actual || "")
            if (failure.length > 0)
                return failure
            return "Нет фактического вывода."
        }
        return actual
    }

    function caseSummary(caseData) {
        if (!caseData)
            return ""
        if (caseData.isBuildErrorCase)
            return "Ошибка сборки"
        return caseData.passed ? "Тест пройден" : "Тест не пройден"
    }

    function openCaseDialog(caseData) {
        selectedTestCase = caseData
        outputRows = buildOutputRows(caseData.expectedOutput, normalizedActualOutput(caseData), !!caseData.passed)
        testCaseTabs.currentIndex = 0
        testCaseDialog.open()
    }

    function markTestCaseTabSwitching() {
        testCaseTabSwitching = true
        testCaseTabSwitchTimer.restart()
    }

    function switchTestCaseTab(step) {
        const total = 3
        const next = (testCaseTabs.currentIndex + step + total) % total
        if (next === testCaseTabs.currentIndex)
            return
        navLog("switchTestCaseTab step=" + step + ", from=" + testCaseTabs.currentIndex + ", to=" + next)
        testCaseTabs.currentIndex = next
    }

    function handleModalTabStep(step) {
        navLog("handleModalTabStep step=" + step + ", visible=" + testCaseDialog.visible)
        if (!testCaseDialog.visible)
            return
        switchTestCaseTab(step)
    }

    function ensureSelectedStudent() {
        if (!controller) {
            selectedStudentId = 0
            return
        }

        const sourceStudents = controller.students || []
        if (sourceStudents.length === 0) {
            selectedStudentId = 0
            return
        }

        let candidateId = Number(selectedStudentId)
        if (candidateId <= 0)
            candidateId = Number(controller.selectedStudentId)

        if (candidateId > 0) {
            let found = false
            for (let i = 0; i < sourceStudents.length; ++i) {
                if (Number(sourceStudents[i].id) === candidateId) {
                    found = true
                    break
                }
            }
            selectedStudentId = found ? candidateId : 0
            return
        }

        selectedStudentId = 0
    }

    function activeStudentId() {
        const studentId = Number(page.selectedStudentId)
        return studentId > 0 ? studentId : 0
    }

    function activeLabId() {
        let labId = 0
        const labs = controller ? (controller.labWorks || []) : []
        if (labs.length > 0)
            labId = Number(labs[0].id)
        return labId
    }

    function openImportDialog(initialPaths) {
        if (!controller)
            return
        const paths = initialPaths || []
        const studentId = activeStudentId()
        const labId = activeLabId()
        importLog("Open import dialog: count=" + paths.length + ", studentId=" + studentId + ", labId=" + labId)
        batchImportDialog.openDialog(studentId, labId, paths)
    }

    function importDroppedUrls(urls) {
        if (!urls || urls.length === 0)
            return false

        const paths = []
        for (let i = 0; i < urls.length; ++i) {
            const localPath = localPathFromUrlLike(urls[i])
            if (localPath && localPath.length > 0)
                paths.push(localPath)
        }
        importLog("Dropped URLs: " + urls.length + ", normalized paths: " + paths.length)
        if (paths.length === 0)
            return false
        openImportDialog(paths)
        return true
    }

    onControllerChanged: ensureSelectedStudent()
    Component.onCompleted: ensureSelectedStudent()

    Timer {
        id: testCaseTabSwitchTimer
        interval: 180
        repeat: false
        onTriggered: page.testCaseTabSwitching = false
    }

    Connections {
        target: controller
        function onStudentsChanged() {
            page.ensureSelectedStudent()
        }
        function onSelectedStudentChanged() {
            if (!controller)
                return
            const externalId = Number(controller.selectedStudentId)
            page.selectedStudentId = externalId > 0 ? externalId : 0
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        AppCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 96
            baseColor: "#131f31"
            borderColor: "#37557d"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            text: "Добавление работ"
                            color: "#edf4ff"
                            font.pixelSize: 16
                            font.bold: true
                        }

                        Label {
                            text: "Нажмите \"Добавить\" или перетащите файлы/папку в окно."
                            color: "#9eb8da"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Label {
                            text: {
                                if (!controller)
                                    return ""
                                if ((controller.executionMode || "local") === "hybrid") {
                                    return "Режим: гибридный (локально + сервер)"
                                }
                                if ((controller.executionMode || "local") === "server") {
                                    return "Режим: серверный запуск"
                                }
                                return "Режим: локальный запуск"
                            }
                            color: "#83b1ea"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }

                    AppButton {
                        id: addButton
                        text: "Добавить"
                        enabled: !(controller && controller.testsRunning)
                        Layout.preferredWidth: 120
                        onClicked: {
                            page.importLog("Add button clicked")
                            const point = addButton.mapToItem(page, 0, addButton.height + 6)
                            addOptionsPopup.x = point.x
                            addOptionsPopup.y = point.y
                            addOptionsPopup.openedY = point.y
                            addOptionsPopup.open()
                        }
                    }

                    AppButton {
                        text: controller && controller.testsRunning ? "Отменить" : "Запустить тесты"
                        Layout.preferredWidth: 160
                        onClicked: {
                            if (!controller)
                                return
                            if (controller.testsRunning) {
                                controller.cancelTests()
                            } else {
                                controller.runTests()
                            }
                        }
                    }
                }

            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            AppCard {
                Layout.preferredWidth: 470
                Layout.fillHeight: true
                baseColor: "#141f30"
                borderColor: "#345074"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            text: "Работы"
                            color: "#edf4ff"
                            font.pixelSize: 16
                            font.bold: true
                            Layout.fillWidth: true
                        }

                        AppButton {
                            text: "Очистить"
                            quiet: true
                            danger: true
                            Layout.preferredWidth: 110
                            enabled: controller && !controller.testsRunning
                            onClicked: if (controller) controller.clearSubmissions()
                        }
                    }

                    ListView {
                        id: submissionsList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 8
                        clip: true
                        model: controller ? controller.submissionsModel : null
                        reuseItems: false

                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 82
                            radius: 12
                            color: "#1a2a40"
                            border.color: "#355070"
                            border.width: 1

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                anchors.rightMargin: 76
                                spacing: 4

                                Item { Layout.fillHeight: true }

                                Label {
                                    text: studentName + " (" + groupName + ")"
                                    color: "#f2f7ff"
                                    font.bold: true
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: labTitle + " [" + language + "]"
                                    color: "#aecdff"
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                Item { Layout.fillHeight: true }
                            }

                            AppButton {
                                anchors.right: parent.right
                                anchors.rightMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                width: 42
                                height: 42
                                text: "×"
                                quiet: true
                                danger: true
                                onClicked: {
                                    if (controller)
                                        controller.removeSubmission(submissionId)
                                }
                            }
                        }
                    }
                }
            }

            AppCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                baseColor: "#141f30"
                borderColor: "#345074"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            text: "Проверки"
                            color: "#edf4ff"
                            font.pixelSize: 16
                            font.bold: true
                            Layout.fillWidth: true
                        }

                        AppButton {
                            text: "Очистить"
                            quiet: true
                            danger: true
                            Layout.preferredWidth: 110
                            enabled: controller && !controller.testsRunning
                            onClicked: if (controller) controller.clearResults()
                        }
                    }

                    ListView {
                        id: resultList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 8
                        clip: true
                        model: controller ? controller.resultsModel : null
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
                                NumberAnimation { property: "x"; from: 24; to: 0; duration: 190; easing.type: Easing.OutCubic }
                            }
                        }

                        remove: Transition {
                            ParallelAnimation {
                                NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 130 }
                                NumberAnimation { property: "x"; from: 0; to: 24; duration: 130; easing.type: Easing.InCubic }
                            }
                        }

                        displaced: Transition {
                            NumberAnimation {
                                properties: "x,y"
                                duration: 160
                                easing.type: Easing.OutCubic
                            }
                        }

                        delegate: Rectangle {
                            id: resultBlock

                            property bool expanded: false
                            property var details: []
                            property bool detailsLoaded: false
                            readonly property int collapsedHeight: 98
                            readonly property int expandedHeight: collapsedHeight + 8 + detailsColumn.implicitHeight

                            function ensureLoaded() {
                                if (detailsLoaded || !controller)
                                    return
                                details = controller.testCasesForSubmission(submissionId)
                                detailsLoaded = true
                            }

                            width: ListView.view.width
                            height: expanded ? expandedHeight : collapsedHeight
                            radius: 12
                            color: "#1a2a40"
                            border.color: expanded ? "#5b84bc" : "#355070"
                            border.width: 1

                            Behavior on height {
                                NumberAnimation {
                                    duration: 180
                                    easing.type: Easing.OutCubic
                                }
                            }

                            Behavior on border.color {
                                ColorAnimation { duration: 140 }
                            }

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 8

                                Rectangle {
                                    id: headerRow
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: resultBlock.collapsedHeight - 20
                                    radius: 10
                                    color: resultBlock.expanded ? "#223652" : "transparent"
                                    border.color: resultBlock.expanded ? "#3f648f" : "transparent"
                                    border.width: 1

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (!resultBlock.expanded)
                                                resultBlock.ensureLoaded()
                                            resultBlock.expanded = !resultBlock.expanded
                                        }
                                    }

                                    Item {
                                        id: headerContent
                                        anchors.fill: parent
                                        anchors.margins: 6

                                        Column {
                                            id: leftColumn
                                            anchors.left: parent.left
                                            anchors.top: parent.top
                                            anchors.bottom: parent.bottom
                                            anchors.right: rightControls.left
                                            anchors.rightMargin: 10
                                            spacing: 3

                                            Label {
                                                text: studentName + " — " + labTitle
                                                width: parent.width
                                                color: "#f2f7ff"
                                                font.bold: true
                                                elide: Text.ElideRight
                                            }
                                            Label {
                                                text: passedTests + "/" + totalTests + " • " + failedTests + " ошибок"
                                                width: parent.width
                                                color: "#aecdff"
                                                font.pixelSize: 12
                                                elide: Text.ElideRight
                                            }
                                            Label {
                                                text: message
                                                width: parent.width
                                                color: "#9fb4d2"
                                                font.pixelSize: 12
                                                elide: Text.ElideRight
                                            }
                                        }

                                        Item {
                                            id: rightControls
                                            readonly property int indicatorWidth: 18
                                            readonly property int gapWidth: 10
                                            readonly property int leftMinWidth: 260
                                            readonly property int badgeMaxWidth: Math.max(
                                                90,
                                                headerContent.width - leftMinWidth - indicatorWidth - gapWidth
                                            )

                                            anchors.right: parent.right
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: statusBadge.width + gapWidth + indicatorWidth
                                            implicitHeight: 30

                                            Label {
                                                id: expandIndicator
                                                anchors.right: rightControls.right
                                                anchors.verticalCenter: parent.verticalCenter
                                                width: rightControls.indicatorWidth
                                                text: resultBlock.expanded ? "▾" : "▸"
                                                color: "#d4e4ff"
                                                font.pixelSize: 20
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }

                                            StatusBadge {
                                                id: statusBadge
                                                anchors.right: expandIndicator.left
                                                anchors.rightMargin: rightControls.gapWidth
                                                anchors.verticalCenter: parent.verticalCenter
                                                maxWidth: rightControls.badgeMaxWidth
                                                status: model.status
                                                statusKey: model.statusKey
                                            }
                                        }
                                    }
                                }

                                Item {
                                    id: detailsClip
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: resultBlock.expanded ? detailsColumn.implicitHeight : 0
                                    clip: true

                                    ColumnLayout {
                                        id: detailsColumn
                                        width: parent.width
                                        spacing: 6

                                        AlgorithmTestGroups {
                                            Layout.fillWidth: true
                                            cases: resultBlock.details
                                            defaultExpanded: false
                                            emptyText: "Детальные тесты недоступны."
                                            onCaseClicked: function(testCase) { page.openCaseDialog(testCase) }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Label {
                        visible: resultList.count === 0
                        text: "Результаты появятся после запуска тестов."
                        color: "#8fa4c3"
                        font.pixelSize: 13
                    }
                }
            }
        }
    }

    Popup {
        id: testCaseDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        onOpened: {
            navLog("testCaseDialog opened")
            testCaseDialog.forceActiveFocus()
        }
        onClosed: navLog("testCaseDialog closed")
        dim: false
        width: Math.min(page.width - 40, 900)
        height: Math.min(page.height - 40, 640)
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        Shortcut {
            sequence: "Tab"
            context: Qt.ApplicationShortcut
            enabled: testCaseDialog.visible
            onActivated: page.switchTestCaseTab(1)
        }

        Shortcut {
            sequence: "A"
            context: Qt.ApplicationShortcut
            enabled: testCaseDialog.visible
            onActivated: page.switchTestCaseTab(-1)
        }

        Shortcut {
            sequence: "D"
            context: Qt.ApplicationShortcut
            enabled: testCaseDialog.visible
            onActivated: page.switchTestCaseTab(1)
        }

        Shortcut {
            sequence: "Ф"
            context: Qt.ApplicationShortcut
            enabled: testCaseDialog.visible
            onActivated: page.switchTestCaseTab(-1)
        }

        Shortcut {
            sequence: "В"
            context: Qt.ApplicationShortcut
            enabled: testCaseDialog.visible
            onActivated: page.switchTestCaseTab(1)
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
                    text: selectedTestCase.testName ? selectedTestCase.testName : "Детали теста"
                    color: "#edf4ff"
                    font.pixelSize: 16
                    font.bold: true
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                ToolButton {
                    id: closeCaseButton
                    implicitWidth: 36
                    implicitHeight: 36
                    hoverEnabled: true
                    focusPolicy: Qt.NoFocus
                    clip: true
                    background: Rectangle {
                        radius: 10
                        color: closeCaseButton.hovered ? "#2a415f" : "#1a2a40"
                        border.color: closeCaseButton.hovered ? "#5889c3" : "#36557a"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: "✕"
                        color: "#d8e8ff"
                        font.pixelSize: 16
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: testCaseDialog.close()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 8
                color: selectedTestCase.passed ? "#1f3a33" : "#452c34"
                border.color: selectedTestCase.passed ? "#3b7d66" : "#9f5160"
                border.width: 1
                implicitHeight: statusRow.implicitHeight + 12

                RowLayout {
                    id: statusRow
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 8

                    Label {
                        text: selectedTestCase.status || ""
                        color: selectedTestCase.passed ? "#97e6c4" : "#ffb8c3"
                        font.bold: true
                    }

                    Label {
                        text: page.caseSummary(selectedTestCase)
                        color: "#c4d4ee"
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }

            TabBar {
                id: testCaseTabs
                Layout.fillWidth: true
                currentIndex: 0
                spacing: 6
                onCurrentIndexChanged: page.markTestCaseTabSwitching()
                background: Rectangle {
                    id: testCaseTabsBg
                    color: "#122034"
                    radius: 10
                    border.color: "#2f4868"

                    Rectangle {
                        id: testTabsIndicator
                        y: 3
                        height: parent.height - 6
                        radius: 8
                        color: "#2f5f95"
                        border.color: "#4f8bd0"
                        border.width: 1

                        x: {
                            switch (testCaseTabs.currentIndex) {
                            case 0: return inputTabButton.x
                            case 1: return outputTabButton.x
                            case 2: return codeTabButton.x
                            default: return inputTabButton.x
                            }
                        }

                        width: {
                            switch (testCaseTabs.currentIndex) {
                            case 0: return inputTabButton.width
                            case 1: return outputTabButton.width
                            case 2: return codeTabButton.width
                            default: return inputTabButton.width
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
                    id: inputTabButton
                    text: "Вход"
                    hoverEnabled: true
                    contentItem: Text {
                        text: parent.text
                        color: parent.checked ? "#ffffff" : (parent.hovered ? "#d9e9ff" : "#86a4cc")
                        font: parent.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                    background: Rectangle {
                        anchors.fill: parent
                        anchors.margins: 3
                        radius: 8
                        color: inputTabButton.hovered && !inputTabButton.checked && !page.testCaseTabSwitching
                            ? "#1e3551"
                            : "transparent"

                        Behavior on color {
                            ColorAnimation { duration: 120 }
                        }
                    }
                }
                TabButton {
                    id: outputTabButton
                    text: "Выход"
                    hoverEnabled: true
                    contentItem: Text {
                        text: parent.text
                        color: parent.checked ? "#ffffff" : (parent.hovered ? "#d9e9ff" : "#86a4cc")
                        font: parent.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                    background: Rectangle {
                        anchors.fill: parent
                        anchors.margins: 3
                        radius: 8
                        color: outputTabButton.hovered && !outputTabButton.checked && !page.testCaseTabSwitching
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
                    contentItem: Text {
                        text: parent.text
                        color: parent.checked ? "#ffffff" : (parent.hovered ? "#d9e9ff" : "#86a4cc")
                        font: parent.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                    background: Rectangle {
                        anchors.fill: parent
                        anchors.margins: 3
                        radius: 8
                        color: codeTabButton.hovered && !codeTabButton.checked && !page.testCaseTabSwitching
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
                currentIndex: testCaseTabs.currentIndex

                ScrollView {
                    clip: true
                    TextArea {
                        readOnly: true
                        wrapMode: TextArea.WrapAnywhere
                        text: selectedTestCase.inputData || "Нет данных."
                        color: "#e8f0ff"
                        font.family: "Consolas"
                        background: Rectangle {
                            color: "#152336"
                            radius: 10
                            border.color: "#2f4868"
                        }
                    }
                }

                ColumnLayout {
                    spacing: 8

                    Loader {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        active: true
                        sourceComponent: selectedTestCase.isBuildErrorCase
                            ? buildErrorOutputComponent
                            : compareOutputComponent
                    }
                }

                ColumnLayout {
                    spacing: 8

                    Rectangle {
                        Layout.fillWidth: true
                        height: 34
                        radius: 8
                        color: "#152336"
                        border.color: "#2f4868"

                        Label {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            text: selectedTestCase.functionName && selectedTestCase.functionName.length > 0
                                ? ("Функция: " + selectedTestCase.functionName)
                                : "Функция не определена"
                            color: "#9eb8da"
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        TextArea {
                            readOnly: true
                            wrapMode: TextArea.NoWrap
                            text: selectedTestCase.functionCode || "Нет данных."
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
        }
    }

    Component {
        id: buildErrorOutputComponent

        ColumnLayout {
            spacing: 8

            Rectangle {
                Layout.fillWidth: true
                height: 34
                radius: 8
                color: "#152336"
                border.color: "#2f4868"

                Label {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    text: "Ошибка сборки / запуска"
                    color: "#9eb8da"
                    verticalAlignment: Text.AlignVCenter
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                TextArea {
                    readOnly: true
                    wrapMode: TextArea.WrapAnywhere
                    text: page.normalizedActualOutput(selectedTestCase)
                    color: "#ffd5dd"
                    font.family: "Consolas"
                    background: Rectangle {
                        color: "#2a1d29"
                        radius: 10
                        border.color: "#9e5160"
                    }
                }
            }
        }
    }

    Component {
        id: compareOutputComponent

        ColumnLayout {
            spacing: 8

            Rectangle {
                Layout.fillWidth: true
                height: 36
                radius: 8
                color: "#152336"
                border.color: "#2f4868"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 10

                    Label {
                        text: "#"
                        color: "#9eb8da"
                        Layout.preferredWidth: 36
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Label {
                        text: "Ожидаемый выход"
                        color: "#9eb8da"
                        Layout.fillWidth: true
                    }

                    Label {
                        text: "Фактический выход"
                        color: "#9eb8da"
                        Layout.fillWidth: true
                    }
                }
            }

            ListView {
                id: outputCompareList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 6
                model: page.outputRows

                delegate: Rectangle {
                    required property var modelData

                    width: outputCompareList.width
                    implicitHeight: Math.max(expectedLabel.implicitHeight, actualLabel.implicitHeight) + 14
                    radius: 8
                    color: modelData.match ? "#17342f" : "#422830"
                    border.width: 1
                    border.color: modelData.match ? "#2f7c67" : "#9e5160"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        anchors.topMargin: 6
                        anchors.bottomMargin: 6
                        spacing: 10

                        Label {
                            text: modelData.line
                            color: "#a7bfdc"
                            font.family: "Segoe UI"
                            renderType: Text.NativeRendering
                            Layout.preferredWidth: 36
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Text {
                            id: expectedLabel
                            text: modelData.expectedLine.length > 0 ? modelData.expectedLine : " "
                            color: "#d8e8ff"
                            wrapMode: Text.WrapAnywhere
                            font.family: modelData.diagnostic ? "Consolas" : "Segoe UI"
                            renderType: Text.NativeRendering
                            Layout.fillWidth: true
                        }

                        Text {
                            id: actualLabel
                            text: modelData.actualLine.length > 0 ? modelData.actualLine : " "
                            color: modelData.match ? "#d8e8ff" : "#ffccd5"
                            wrapMode: Text.WrapAnywhere
                            font.family: modelData.diagnostic ? "Consolas" : "Segoe UI"
                            renderType: Text.NativeRendering
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }
    }

    BatchImportDialog {
        id: batchImportDialog
        controller: page.controller
    }

    FileDialog {
        id: addFilesDialog
        title: "Выбор файлов работ"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Файлы C++ (*.cpp *.cc *.cxx)", "Все файлы (*.*)"]
        onAccepted: {
            const paths = []
            for (let i = 0; i < selectedFiles.length; ++i) {
                const filePath = page.localPathFromUrlLike(selectedFiles[i])
                if (filePath && filePath.length > 0)
                    paths.push(filePath)
            }
            if (paths.length > 0)
                page.openImportDialog(paths)
        }
    }

    FolderDialog {
        id: addFolderDialog
        title: "Выбор папки с работами"
        onAccepted: {
            const folderPath = page.localPathFromUrlLike(selectedFolder)
            if (folderPath && folderPath.length > 0)
                page.openImportDialog([folderPath])
        }
    }

    Popup {
        id: addOptionsPopup
        parent: page
        modal: false
        focus: true
        property real openedY: 0
        padding: 6
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onOpened: page.importLog("Add popup opened")
        onClosed: page.importLog("Add popup closed")

        enter: Transition {
            ParallelAnimation {
                NumberAnimation {
                    property: "opacity"
                    from: 0
                    to: 1
                    duration: 130
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    property: "y"
                    from: addOptionsPopup.openedY - 6
                    to: addOptionsPopup.openedY
                    duration: 130
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
                    duration: 100
                    easing.type: Easing.InCubic
                }
                NumberAnimation {
                    property: "y"
                    from: addOptionsPopup.openedY
                    to: addOptionsPopup.openedY - 6
                    duration: 100
                    easing.type: Easing.InCubic
                }
            }
        }

        background: Rectangle {
            radius: 12
            color: "#122034"
            border.color: "#38567d"
            border.width: 1
        }

        contentItem: Column {
            spacing: 4

            ItemDelegate {
                id: addFilesOption
                width: 220
                height: 38
                hoverEnabled: true
                background: Rectangle {
                    radius: 9
                    color: addFilesOption.hovered ? "#1f3551" : "transparent"

                    Behavior on color {
                        ColorAnimation { duration: 110 }
                    }
                }
                contentItem: Text {
                    text: "Добавить файлы"
                    color: "#e7f0ff"
                    font.pixelSize: 13
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    addOptionsPopup.close()
                    addFilesDialog.open()
                }
            }

            ItemDelegate {
                id: addFolderOption
                width: 220
                height: 38
                hoverEnabled: true
                background: Rectangle {
                    radius: 9
                    color: addFolderOption.hovered ? "#1f3551" : "transparent"

                    Behavior on color {
                        ColorAnimation { duration: 110 }
                    }
                }
                contentItem: Text {
                    text: "Добавить папку"
                    color: "#e7f0ff"
                    font.pixelSize: 13
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    addOptionsPopup.close()
                    addFolderDialog.open()
                }
            }
        }
    }

    DropArea {
        id: windowDropArea
        anchors.fill: parent
        z: 200

        onEntered: function(drag) {
            page.globalDropActive = true
            page.importLog("Drop entered")
            if (drag)
                drag.accepted = true
        }
        onPositionChanged: function(drag) {
            if (drag)
                drag.accepted = true
        }
        onExited: {
            page.globalDropActive = false
            page.importLog("Drop exited")
        }
        onDropped: function(drop) {
            page.globalDropActive = false
            page.importLog("Drop accepted, urls=" + (drop && drop.urls ? drop.urls.length : 0))
            if (!drop || !drop.urls || drop.urls.length === 0)
                return
            page.importDroppedUrls(drop.urls)
            drop.accepted = true
        }
    }

    Rectangle {
        anchors.fill: parent
        z: 199
        visible: opacity > 0.01
        color: "#1c2f49"
        opacity: page.globalDropActive ? 0.28 : 0.0

        Behavior on opacity {
            NumberAnimation { duration: 120 }
        }
    }

    Rectangle {
        anchors.centerIn: parent
        z: 201
        width: Math.min(parent.width - 120, 640)
        height: 176
        radius: 16
        visible: opacity > 0.01
        color: "#12243a"
        border.color: "#6ea8ef"
        border.width: 2
        opacity: page.globalDropActive ? 1.0 : 0.0
        scale: page.globalDropActive ? 1.0 : 0.97

        Behavior on opacity {
            NumberAnimation { duration: 120 }
        }

        Behavior on scale {
            NumberAnimation {
                duration: 150
                easing.type: Easing.OutCubic
            }
        }

        Column {
            anchors.centerIn: parent
            spacing: 10

            Label {
                text: "Отпустите файлы или папки"
                color: "#edf4ff"
                font.pixelSize: 23
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
            }
            Label {
                text: "Будут импортированы все C++ файлы (.cpp, .cc, .cxx)"
                color: "#b9ccea"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}



