
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabTester

Item {
    id: page

    property var controller
    property var groupOptions: []
    property var filteredStudents: []
    property var labOptions: []
    property int selectedStudentId: 0
    property var selectedEntry: ({})
    property var runTestCases: []
    property var selectedCase: ({})

    readonly property int colTests: 120
    readonly property int colStars: 120
    readonly property int colStatus: 230
    readonly property int colSpacing: 12
    readonly property int innerMargin: 12
    readonly property int colWork: Math.max(
        280,
        historyList.width
            - (innerMargin * 2)
            - (colSpacing * 3)
            - colTests
            - colStars
            - colStatus
    )

    function selectedGroupName() {
        if (groupCombo.currentIndex < 0 || groupCombo.currentIndex >= groupOptions.length)
            return ""
        const item = groupOptions[groupCombo.currentIndex]
        return item && item.name ? item.name : ""
    }

    function selectedLabId() {
        if (labCombo.currentIndex < 0 || labCombo.currentIndex >= labOptions.length)
            return 0
        const item = labOptions[labCombo.currentIndex]
        return item && item.id ? Number(item.id) : 0
    }

    function updateFilteredStudents() {
        const groupName = selectedGroupName()
        const source = controller ? (controller.students || []) : []
        const next = []
        for (let i = 0; i < source.length; ++i) {
            const item = source[i]
            if (groupName.length > 0 && item.group !== groupName)
                continue
            next.push(item)
        }
        filteredStudents = next
        if (selectedStudentId === 0)
            return
        for (let j = 0; j < filteredStudents.length; ++j) {
            if (Number(filteredStudents[j].id) === selectedStudentId)
                return
        }
        selectedStudentId = 0
    }

    function rebuildFilters() {
        const previousGroup = selectedGroupName()
        const previousLab = selectedLabId()
        const sourceStudents = controller ? (controller.students || []) : []
        const sourceLabs = controller ? (controller.labWorks || []) : []

        const groups = ["Все группы"]
        for (let i = 0; i < sourceStudents.length; ++i) {
            const groupName = (sourceStudents[i].group || "").toString().trim()
            if (groupName.length > 0 && groups.indexOf(groupName) === -1)
                groups.push(groupName)
        }
        groups.sort(function(a, b) {
            if (a === "Все группы") return -1
            if (b === "Все группы") return 1
            return a.localeCompare(b, "ru-RU")
        })
        groupOptions = groups.map(function(groupName) { return { name: groupName === "Все группы" ? "" : groupName, title: groupName } })

        let groupIndex = 0
        for (let g = 0; g < groupOptions.length; ++g) {
            if ((groupOptions[g].name || "") === previousGroup) {
                groupIndex = g
                break
            }
        }
        groupCombo.currentIndex = groupIndex

        labOptions = [{ id: 0, title: "Все лабораторные" }]
        for (let l = 0; l < sourceLabs.length; ++l)
            labOptions.push({ id: Number(sourceLabs[l].id), title: sourceLabs[l].title })

        let labIndex = 0
        for (let i = 0; i < labOptions.length; ++i) {
            if (Number(labOptions[i].id) === previousLab) {
                labIndex = i
                break
            }
        }
        labCombo.currentIndex = labIndex
        updateFilteredStudents()
    }

    function applyFilters() {
        if (!controller)
            return
        controller.applyHistoryFilters(selectedGroupName(), selectedStudentId, selectedLabId())
    }

    function openRunDetails(entry) {
        selectedEntry = entry
        runTestCases = controller ? controller.testCasesForCheckRun(Number(entry.checkRunId || 0)) : []
        runDialog.open()
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
        if (actual.length === 0 || hasTechnicalTrace(actual)) {
            const visibleFailure = visibleFailureLine(caseData.failureDetails || caseData.message || actual)
            if (visibleFailure.length > 0)
                return visibleFailure
            const failure = (caseData.failureDetails || caseData.message || actual || "").toString().trim()
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

    onControllerChanged: {
        rebuildFilters()
        applyFilters()
    }
    Component.onCompleted: {
        rebuildFilters()
        applyFilters()
    }

    Connections {
        target: controller
        function onStudentsChanged() {
            page.rebuildFilters()
            page.applyFilters()
        }
        function onLabWorksChanged() {
            page.rebuildFilters()
            page.applyFilters()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        AppCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 88
            baseColor: "#131f31"
            borderColor: "#37557d"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                AppComboBox {
                    id: groupCombo
                    Layout.fillWidth: true
                    model: page.groupOptions
                    textRole: "title"
                    valueRole: "name"
                    onActivated: {
                        page.updateFilteredStudents()
                        page.applyFilters()
                    }
                }

                StudentPickerField {
                    Layout.fillWidth: true
                    studentsModel: page.filteredStudents
                    allowAllOption: true
                    allOptionText: "Все студенты"
                    selectedStudentId: page.selectedStudentId
                    placeholderText: "Выберите студента"
                    onStudentChosen: function(studentId) {
                        page.selectedStudentId = Number(studentId)
                        page.applyFilters()
                    }
                }

                AppComboBox {
                    id: labCombo
                    Layout.fillWidth: true
                    model: page.labOptions
                    textRole: "title"
                    valueRole: "id"
                    onActivated: page.applyFilters()
                }

                AppButton {
                    text: "Обновить"
                    Layout.preferredWidth: 120
                    onClicked: page.applyFilters()
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

                Label {
                    text: "История запусков"
                    color: "#edf4ff"
                    font.pixelSize: 16
                    font.bold: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 36
                    radius: 10
                    color: "#15273d"
                    border.color: "#36557b"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: page.innerMargin
                        spacing: page.colSpacing

                        Label {
                            text: "Работа"
                            color: "#9eb8da"
                            font.pixelSize: 12
                            font.bold: true
                            Layout.preferredWidth: page.colWork
                        }
                        Label {
                            text: "Тесты"
                            color: "#9eb8da"
                            font.pixelSize: 12
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            Layout.preferredWidth: page.colTests
                        }
                        Label {
                            text: "Звезды"
                            color: "#9eb8da"
                            font.pixelSize: 12
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            Layout.preferredWidth: page.colStars
                        }
                        Label {
                            text: "Статус"
                            color: "#9eb8da"
                            font.pixelSize: 12
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            Layout.preferredWidth: page.colStatus
                        }
                    }
                }

                ListView {
                    id: historyList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 8
                    clip: true
                    model: controller ? controller.historyModel : null
                    reuseItems: false

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 94
                        radius: 12
                        color: hover.containsMouse ? "#1d2f47" : "#1a2a40"
                        border.color: hover.containsMouse ? "#4f79ab" : "#355070"
                        border.width: 1

                        MouseArea {
                            id: hover
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: page.openRunDetails({
                                checkRunId: checkRunId,
                                submissionId: submissionId,
                                studentName: studentName,
                                groupName: groupName,
                                labTitle: labTitle,
                                passedTests: passedTests,
                                totalTests: totalTests,
                                status: status,
                                statusKey: statusKey,
                                stars: stars,
                                executedAt: executedAt,
                                message: message
                            })
                        }

                        AppButton {
                            id: deleteHistoryEntryButton
                            anchors.top: parent.top
                            anchors.right: parent.right
                            anchors.topMargin: -8
                            anchors.rightMargin: -8
                            text: "×"
                            quiet: true
                            Layout.preferredWidth: 26
                            Layout.preferredHeight: 26
                            width: 26
                            height: 26
                            font.pixelSize: 16
                            z: 4
                            onClicked: {
                                if (controller)
                                    controller.removeHistoryEntry(Number(checkRunId))
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: page.innerMargin
                            spacing: page.colSpacing

                            ColumnLayout {
                                Layout.preferredWidth: page.colWork
                                spacing: 4
                                Label { text: studentName + " (" + groupName + ")"; color: "#f2f7ff"; font.bold: true; elide: Text.ElideRight }
                                Label { text: labTitle; color: "#aecdff"; elide: Text.ElideRight }
                                Label { text: executedAt; color: "#96acce"; font.pixelSize: 12; elide: Text.ElideRight }
                            }

                            Label { text: passedTests + "/" + totalTests; color: "#d5e4fb"; font.pixelSize: 14; horizontalAlignment: Text.AlignHCenter; Layout.preferredWidth: page.colTests }
                            Label {
                                text: "★".repeat(Math.max(0, Math.min(3, Number(stars)))) + "☆".repeat(Math.max(0, 3 - Math.max(0, Math.min(3, Number(stars)))))
                                color: stars > 0 ? "#f4cf6a" : "#7086a8"
                                font.pixelSize: 17
                                horizontalAlignment: Text.AlignHCenter
                                Layout.preferredWidth: page.colStars
                            }
                            Item {
                                Layout.preferredWidth: page.colStatus
                                Layout.fillHeight: true
                                StatusBadge { anchors.centerIn: parent; status: model.status; statusKey: model.statusKey }
                            }
                        }
                    }
                }

                Label {
                    visible: historyList.count === 0
                    text: "История пока пуста."
                    color: "#8fa4c3"
                    font.pixelSize: 13
                }
            }
        }
    }

    Popup {
        id: runDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        dim: false
        width: Math.min(page.width - 40, 940)
        height: Math.min(page.height - 40, 650)
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
                    text: selectedEntry.labTitle ? (selectedEntry.studentName + " — " + selectedEntry.labTitle) : "Детали запуска"
                    color: "#edf4ff"
                    font.pixelSize: 17
                    font.bold: true
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                ToolButton {
                    id: closeRunDialogButton
                    implicitWidth: 36
                    implicitHeight: 36
                    hoverEnabled: true
                    focusPolicy: Qt.NoFocus
                    background: Rectangle {
                        radius: 10
                        color: closeRunDialogButton.hovered ? "#2a415f" : "#1a2a40"
                        border.color: closeRunDialogButton.hovered ? "#5889c3" : "#36557a"
                        border.width: 1
                    }
                    contentItem: Text { text: "✕"; color: "#d8e8ff"; font.pixelSize: 16; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    onClicked: runDialog.close()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 44
                radius: 10
                color: "#152336"
                border.color: "#2f4868"
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 10

                    Label { text: (selectedEntry.passedTests || 0) + "/" + (selectedEntry.totalTests || 0); color: "#d8e8ff"; font.bold: true }
                    Label { text: "•"; color: "#6d88ab" }
                    Label { text: selectedEntry.executedAt || ""; color: "#9eb8da"; Layout.fillWidth: true; elide: Text.ElideRight }
                    Label {
                        text: "★".repeat(Math.max(0, Math.min(3, Number(selectedEntry.stars || 0))))
                              + "☆".repeat(Math.max(0, 3 - Math.max(0, Math.min(3, Number(selectedEntry.stars || 0)))))
                        color: Number(selectedEntry.stars || 0) > 0 ? "#f4cf6a" : "#7086a8"
                        font.pixelSize: 16
                    }
                    StatusBadge { status: selectedEntry.status || ""; statusKey: selectedEntry.statusKey || "" }
                }
            }

            ScrollView {
                id: runTestsScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                AlgorithmTestGroups {
                    width: runTestsScroll.availableWidth
                    cases: page.runTestCases
                    defaultExpanded: false
                    emptyText: "Для этого запуска детали тестов не найдены."
                    onCaseClicked: function(testCase) {
                        page.selectedCase = testCase
                        caseDialog.open()
                    }
                }
            }
        }
    }

    Popup {
        id: caseDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        dim: false
        width: Math.min(page.width - 40, 940)
        height: Math.min(page.height - 40, 620)
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
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: selectedCase.testName ? selectedCase.testName : "Детали теста"
                    color: "#edf4ff"
                    font.pixelSize: 17
                    font.bold: true
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                ToolButton {
                    id: closeCaseDialogButton
                    implicitWidth: 36
                    implicitHeight: 36
                    hoverEnabled: true
                    focusPolicy: Qt.NoFocus
                    background: Rectangle {
                        radius: 10
                        color: closeCaseDialogButton.hovered ? "#2a415f" : "#1a2a40"
                        border.color: closeCaseDialogButton.hovered ? "#5889c3" : "#36557a"
                        border.width: 1
                    }
                    contentItem: Text { text: "✕"; color: "#d8e8ff"; font.pixelSize: 16; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    onClicked: caseDialog.close()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 8
                color: selectedCase.passed ? "#1f3a33" : "#452c34"
                border.color: selectedCase.passed ? "#3b7d66" : "#9f5160"
                border.width: 1
                implicitHeight: 44

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8
                    Label { text: selectedCase.status || ""; color: selectedCase.passed ? "#97e6c4" : "#ffb8c3"; font.bold: true }
                    Label { text: page.caseSummary(selectedCase); color: "#c4d4ee"; Layout.fillWidth: true; elide: Text.ElideRight }
                }
            }

            Label { text: "Вход"; color: "#9eb8da"; font.bold: true }
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: 100
                clip: true
                TextArea {
                    readOnly: true
                    wrapMode: TextArea.WrapAnywhere
                    text: selectedCase.inputData || "Нет данных."
                    color: "#e8f0ff"
                    font.family: "Consolas"
                    background: Rectangle { color: "#152336"; radius: 10; border.color: "#2f4868" }
                }
            }

            Label { text: selectedCase.isBuildErrorCase ? "Ошибка" : "Выход"; color: "#9eb8da"; font.bold: true }
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: 120
                clip: true
                TextArea {
                    readOnly: true
                    wrapMode: TextArea.WrapAnywhere
                    text: selectedCase.isBuildErrorCase
                        ? page.normalizedActualOutput(selectedCase)
                        : (("Ожидаемый:\n" + (selectedCase.expectedOutput || "")) + "\n\nФактический:\n" + page.normalizedActualOutput(selectedCase))
                    color: selectedCase.isBuildErrorCase ? "#ffd5dd" : "#e8f0ff"
                    font.family: "Consolas"
                    background: Rectangle {
                        color: selectedCase.isBuildErrorCase ? "#2a1d29" : "#152336"
                        radius: 10
                        border.color: selectedCase.isBuildErrorCase ? "#9e5160" : "#2f4868"
                    }
                }
            }

            Label { text: "Код"; color: "#9eb8da"; font.bold: true }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                TextArea {
                    readOnly: true
                    wrapMode: TextArea.NoWrap
                    text: selectedCase.functionCode || "Нет данных."
                    color: "#e8f0ff"
                    font.family: "Consolas"
                    background: Rectangle { color: "#152336"; radius: 10; border.color: "#2f4868" }
                }
            }
        }
    }
}
