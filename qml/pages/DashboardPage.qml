import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabTester

Item {
    id: page

    property var controller
    property int editingStudentId: 0
    property int pendingDeleteStudentId: 0
    property string pendingDeleteStudentName: ""

    property var selectedSubmissionData: ({})
    property var selectedSubmissionCases: []
    property var selectedSubmissionCase: ({})
    property int selectedSubmissionPassedCount: 0
    property int selectedSubmissionTotalCount: 0

    function openCreateStudentDialog() {
        editingStudentId = 0
        addStudentDialog.titleText = "Новый студент"
        addStudentDialog.submitButtonText = "Добавить"
        addStudentDialog.openDialog("", "")
    }

    function openEditStudentDialog(studentData) {
        if (!studentData)
            return
        editingStudentId = Number(studentData.id)
        addStudentDialog.titleText = "Редактирование студента"
        addStudentDialog.submitButtonText = "Сохранить"
        addStudentDialog.openDialog(studentData.name || "", studentData.group || "")
    }

    function submitStudentDialog(studentName, groupName) {
        if (!controller)
            return

        let success = false
        if (editingStudentId > 0)
            success = controller.updateStudent(editingStudentId, studentName, groupName)
        else
            success = controller.addStudent(studentName, groupName)

        if (success) {
            editingStudentId = 0
            addStudentDialog.close()
        }
    }

    function requestDeleteStudent(studentData) {
        if (!studentData)
            return
        pendingDeleteStudentId = Number(studentData.id)
        pendingDeleteStudentName = studentData.name || ""
        deleteStudentPopup.open()
    }

    function confirmDeleteStudent() {
        if (!controller || pendingDeleteStudentId <= 0)
            return

        if (controller.removeStudent(pendingDeleteStudentId)) {
            pendingDeleteStudentId = 0
            pendingDeleteStudentName = ""
            deleteStudentPopup.close()
        }
    }

    function openSubmissionDetails(submissionData) {
        if (!controller || !submissionData)
            return

        selectedSubmissionData = submissionData
        selectedSubmissionCases = controller.testCasesForSubmission(Number(submissionData.submissionId))

        let passed = 0
        for (let i = 0; i < selectedSubmissionCases.length; ++i) {
            if (selectedSubmissionCases[i].passed)
                passed += 1
        }
        selectedSubmissionPassedCount = passed
        selectedSubmissionTotalCount = selectedSubmissionCases.length
        submissionDetailsPopup.open()
    }

    function openSubmissionCaseDetails(caseData) {
        selectedSubmissionCase = caseData || ({})
        submissionCasePopup.open()
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

    RowLayout {
        anchors.fill: parent
        spacing: 12

        AppCard {
            Layout.preferredWidth: 420
            Layout.fillHeight: true
            baseColor: "#141f30"
            borderColor: "#345074"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Label {
                        text: "Студенты"
                        color: "#edf4ff"
                        font.pixelSize: 18
                        font.bold: true
                        Layout.fillWidth: true
                    }

                    AppButton {
                        text: "Добавить"
                        quiet: true
                        Layout.preferredWidth: 120
                        onClicked: page.openCreateStudentDialog()
                    }
                }

                ListView {
                    id: studentList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 8
                    clip: true
                    model: controller ? controller.students : []
                    reuseItems: false

                    populate: Transition {
                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 170 }
                    }

                    add: Transition {
                        ParallelAnimation {
                            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 160 }
                            NumberAnimation { property: "x"; from: 18; to: 0; duration: 170; easing.type: Easing.OutCubic }
                        }
                    }

                    remove: Transition {
                        ParallelAnimation {
                            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 }
                            NumberAnimation { property: "x"; from: 0; to: 18; duration: 120; easing.type: Easing.InCubic }
                        }
                    }

                    delegate: Rectangle {
                        required property var modelData

                        width: ListView.view.width
                        height: 94
                        radius: 12
                        color: controller && controller.selectedStudentId === modelData.id
                            ? "#284468"
                            : "#1a2a40"
                        border.color: controller && controller.selectedStudentId === modelData.id
                            ? "#68a5f7"
                            : "#355070"
                        border.width: 1

                        Behavior on color {
                            ColorAnimation { duration: 140 }
                        }

                        Behavior on border.color {
                            ColorAnimation { duration: 140 }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: if (controller) controller.selectStudent(modelData.id)
                        }

                        Column {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.margins: 12
                            anchors.rightMargin: 86
                            spacing: 4

                            Label {
                                text: modelData.name
                                color: "#f2f7ff"
                                font.pixelSize: 15
                                font.bold: true
                                elide: Text.ElideRight
                            }

                            Label {
                                text: modelData.group + " • работ: " + modelData.worksCount
                                color: "#a6bbda"
                                elide: Text.ElideRight
                            }
                        }

                        Row {
                            anchors.top: parent.top
                            anchors.right: parent.right
                            anchors.topMargin: 8
                            anchors.rightMargin: 8
                            spacing: 6

                            ToolButton {
                                id: editStudentButton
                                width: 28
                                height: 28
                                text: "✎"
                                hoverEnabled: true
                                focusPolicy: Qt.NoFocus
                                font.pixelSize: 14
                                contentItem: Text {
                                    text: editStudentButton.text
                                    color: "#d8e8ff"
                                    font.pixelSize: 14
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 8
                                    color: editStudentButton.hovered ? "#2a415f" : "#1a2a40"
                                    border.color: editStudentButton.hovered ? "#5889c3" : "#36557a"
                                    border.width: 1
                                }
                                onClicked: page.openEditStudentDialog(modelData)
                            }

                            ToolButton {
                                id: removeStudentButton
                                width: 28
                                height: 28
                                text: "×"
                                hoverEnabled: true
                                focusPolicy: Qt.NoFocus
                                font.pixelSize: 16
                                contentItem: Text {
                                    text: removeStudentButton.text
                                    color: "#ffd8df"
                                    font.pixelSize: 16
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 8
                                    color: removeStudentButton.hovered ? "#5d2a36" : "#3e2430"
                                    border.color: removeStudentButton.hovered ? "#d57183" : "#9a5060"
                                    border.width: 1
                                }
                                onClicked: page.requestDeleteStudent(modelData)
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
                spacing: 10

                Label {
                    text: controller && controller.selectedStudentName.length > 0
                        ? controller.selectedStudentName
                        : "Работы студента"
                    color: "#edf4ff"
                    font.pixelSize: 18
                    font.bold: true
                }

                Label {
                    text: "Нажмите на работу, чтобы открыть подробности."
                    color: "#8fa9ca"
                    font.pixelSize: 12
                }

                ListView {
                    id: worksList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 8
                    clip: true
                    model: controller ? controller.selectedStudentSubmissions : []
                    reuseItems: false

                    populate: Transition {
                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 170 }
                    }

                    add: Transition {
                        ParallelAnimation {
                            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 160 }
                            NumberAnimation { property: "x"; from: 18; to: 0; duration: 170; easing.type: Easing.OutCubic }
                        }
                    }

                    remove: Transition {
                        ParallelAnimation {
                            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 }
                            NumberAnimation { property: "x"; from: 0; to: 18; duration: 120; easing.type: Easing.InCubic }
                        }
                    }

                    delegate: Rectangle {
                        required property var modelData

                        width: ListView.view.width
                        height: 88
                        radius: 12
                        color: workHover.containsMouse ? "#1d2f47" : "#1a2a40"
                        border.color: workHover.containsMouse ? "#4f79ab" : "#355070"
                        border.width: 1

                        MouseArea {
                            id: workHover
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: page.openSubmissionDetails(modelData)
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            anchors.rightMargin: 62
                            spacing: 4

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.alignment: Qt.AlignVCenter
                                spacing: 4

                                Label {
                                    text: modelData.labTitle
                                    color: "#f3f7ff"
                                    font.pixelSize: 15
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                Label {
                                    text: modelData.createdAt
                                    color: "#93a9ca"
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }

                        }

                        AppButton {
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            width: 34
                            height: 34
                            text: "×"
                            quiet: true
                            danger: true
                            enabled: controller && !controller.testsRunning
                            onClicked: {
                                if (controller)
                                    controller.removeSubmission(Number(modelData.submissionId))
                            }
                        }
                    }
                }

                Label {
                    visible: worksList.count === 0
                    text: "Пока пусто"
                    color: "#8fa4c3"
                    font.pixelSize: 13
                }
            }
        }
    }

    AddStudentDialog {
        id: addStudentDialog
        titleText: "Новый студент"
        submitButtonText: "Добавить"
        onSubmitRequested: page.submitStudentDialog(studentName, groupName)
    }

    Popup {
        id: deleteStudentPopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        dim: false
        width: Math.min(page.width - 40, 440)
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

            Label {
                Layout.fillWidth: true
                text: "Удалить студента?"
                color: "#edf4ff"
                font.pixelSize: 16
                font.bold: true
            }

            Label {
                Layout.fillWidth: true
                text: "Будут удалены студент и все его работы из активного списка."
                color: "#a9bedc"
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                text: pendingDeleteStudentName
                color: "#f2f7ff"
                font.bold: true
                elide: Text.ElideRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Item { Layout.fillWidth: true }

                AppButton {
                    text: "Отмена"
                    quiet: true
                    Layout.preferredWidth: 120
                    onClicked: deleteStudentPopup.close()
                }

                AppButton {
                    text: "Удалить"
                    danger: true
                    Layout.preferredWidth: 120
                    onClicked: page.confirmDeleteStudent()
                }
            }
        }
    }

    Popup {
        id: submissionDetailsPopup
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
                    text: selectedSubmissionData.labTitle
                        ? (selectedSubmissionData.studentName + " — " + selectedSubmissionData.labTitle)
                        : "Детали работы"
                    color: "#edf4ff"
                    font.pixelSize: 17
                    font.bold: true
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                ToolButton {
                    id: closeSubmissionDialogButton
                    implicitWidth: 36
                    implicitHeight: 36
                    hoverEnabled: true
                    focusPolicy: Qt.NoFocus
                    background: Rectangle {
                        radius: 10
                        color: closeSubmissionDialogButton.hovered ? "#2a415f" : "#1a2a40"
                        border.color: closeSubmissionDialogButton.hovered ? "#5889c3" : "#36557a"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: "✕"
                        color: "#d8e8ff"
                        font.pixelSize: 16
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: submissionDetailsPopup.close()
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

                    Label {
                        text: selectedSubmissionPassedCount + "/" + selectedSubmissionTotalCount
                        color: "#d8e8ff"
                        font.bold: true
                    }
                    Label { text: "•"; color: "#6d88ab" }
                    Label {
                        text: selectedSubmissionData.createdAt || ""
                        color: "#9eb8da"
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    StatusBadge {
                        status: selectedSubmissionData.status || "Ожидает"
                        statusKey: selectedSubmissionData.statusKey || "Pending"
                    }
                }
            }

            ScrollView {
                id: submissionTestsScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                AlgorithmTestGroups {
                    width: submissionTestsScroll.availableWidth
                    cases: page.selectedSubmissionCases
                    defaultExpanded: false
                    emptyText: "Детали тестов для этой работы не найдены."
                    onCaseClicked: function(testCase) { page.openSubmissionCaseDetails(testCase) }
                }
            }

            ListView {
                id: submissionTestsList
                Layout.fillWidth: true
                Layout.fillHeight: false
                Layout.preferredHeight: 0
                visible: false
                clip: true
                spacing: 8
                model: page.selectedSubmissionCases

                delegate: Rectangle {
                    required property var modelData

                    width: submissionTestsList.width
                    implicitHeight: 64
                    radius: 10
                    color: hoverCase.containsMouse ? (modelData.passed ? "#1d3b37" : "#4a2a33") : (modelData.passed ? "#17342f" : "#422830")
                    border.color: modelData.passed ? "#2f7c67" : "#9e5160"
                    border.width: 1

                    MouseArea {
                        id: hoverCase
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: page.openSubmissionCaseDetails(modelData)
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 10

                        Label {
                            text: modelData.testName
                            color: "#e9f2ff"
                            font.bold: true
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        Label {
                            text: Number(modelData.durationMs || 0) + " мс"
                            color: "#9eb8da"
                        }
                        StatusBadge {
                            status: modelData.status
                            statusKey: modelData.passed ? "Passed" : (modelData.isBuildErrorCase ? "BuildError" : "Failed")
                        }
                    }
                }
            }

            Label {
                visible: false
                text: "Для этой работы детали тестов не найдены."
                color: "#8fa4c3"
                font.pixelSize: 13
            }
        }
    }

    Popup {
        id: submissionCasePopup
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
                    text: selectedSubmissionCase.testName ? selectedSubmissionCase.testName : "Детали теста"
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
                    contentItem: Text {
                        text: "✕"
                        color: "#d8e8ff"
                        font.pixelSize: 16
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: submissionCasePopup.close()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 8
                color: selectedSubmissionCase.passed ? "#1f3a33" : "#452c34"
                border.color: selectedSubmissionCase.passed ? "#3b7d66" : "#9f5160"
                border.width: 1
                implicitHeight: 44

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8
                    Label {
                        text: selectedSubmissionCase.status || ""
                        color: selectedSubmissionCase.passed ? "#97e6c4" : "#ffb8c3"
                        font.bold: true
                    }
                    Label {
                        text: page.caseSummary(selectedSubmissionCase)
                        color: "#c4d4ee"
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
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
                    text: selectedSubmissionCase.inputData || "Нет данных."
                    color: "#e8f0ff"
                    font.family: "Consolas"
                    background: Rectangle { color: "#152336"; radius: 10; border.color: "#2f4868" }
                }
            }

            Label { text: selectedSubmissionCase.isBuildErrorCase ? "Ошибка" : "Выход"; color: "#9eb8da"; font.bold: true }
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: 120
                clip: true
                TextArea {
                    readOnly: true
                    wrapMode: TextArea.WrapAnywhere
                    text: selectedSubmissionCase.isBuildErrorCase
                        ? page.normalizedActualOutput(selectedSubmissionCase)
                        : (("Ожидаемый:\n" + (selectedSubmissionCase.expectedOutput || "")) + "\n\nФактический:\n" + page.normalizedActualOutput(selectedSubmissionCase))
                    color: selectedSubmissionCase.isBuildErrorCase ? "#ffd5dd" : "#e8f0ff"
                    font.family: "Consolas"
                    background: Rectangle {
                        color: selectedSubmissionCase.isBuildErrorCase ? "#2a1d29" : "#152336"
                        radius: 10
                        border.color: selectedSubmissionCase.isBuildErrorCase ? "#9e5160" : "#2f4868"
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
                    text: selectedSubmissionCase.functionCode || "Нет данных."
                    color: "#e8f0ff"
                    font.family: "Consolas"
                    background: Rectangle { color: "#152336"; radius: 10; border.color: "#2f4868" }
                }
            }
        }
    }
}
