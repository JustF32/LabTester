import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import LabTester

Popup {
    id: root

    property var controller
    property int defaultStudentId: 0
    property int defaultLabId: 0
    property int selectedLabId: 0

    signal importCompleted()

    function openDialog(studentId, labId, initialPaths) {
        defaultStudentId = Number(studentId)
        defaultLabId = Number(labId)
        selectedLabId = defaultLabId
        rowsModel.clear()
        pickDefaultLab()
        appendPaths(initialPaths || [])
        open()
    }

    function pickDefaultLab() {
        const labs = controller ? (controller.labWorks || []) : []
        if (labs.length === 0) {
            selectedLabId = 0
            labCombo.currentIndex = -1
            return
        }

        let index = 0
        if (defaultLabId > 0) {
            for (let i = 0; i < labs.length; ++i) {
                if (Number(labs[i].id) === defaultLabId) {
                    index = i
                    break
                }
            }
        }

        labCombo.currentIndex = index
        selectedLabId = Number(labs[index].id)
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
            }
        }
        return text
    }

    function fileName(path) {
        if (!path || path.toString().trim().length === 0)
            return ""
        const normalized = path.toString().replace(/\\/g, "/")
        const parts = normalized.split("/")
        return parts.length > 0 ? parts[parts.length - 1] : normalized
    }

    function appendPaths(paths) {
        const source = paths || []
        const normalizedInputs = []

        for (let i = 0; i < source.length; ++i) {
            const normalized = localPathFromUrlLike(source[i])
            if (normalized && normalized.length > 0)
                normalizedInputs.push(normalized)
        }

        let resolvedPaths = normalizedInputs
        if (controller && normalizedInputs.length > 0 && controller.resolveImportSources) {
            const expanded = controller.resolveImportSources(normalizedInputs)
            if (expanded && expanded.length > 0)
                resolvedPaths = expanded
        }

        for (let i = 0; i < resolvedPaths.length; ++i) {
            const path = localPathFromUrlLike(resolvedPaths[i])
            if (!path || path.length === 0)
                continue

            let duplicate = false
            for (let j = 0; j < rowsModel.count; ++j) {
                if (rowsModel.get(j).filePath === path) {
                    duplicate = true
                    break
                }
            }

            if (duplicate)
                continue

            rowsModel.append({
                filePath: path,
                fileName: fileName(path),
                studentId: defaultStudentId
            })
        }
    }

    function removeRow(index) {
        if (index < 0 || index >= rowsModel.count)
            return
        rowsModel.remove(index, 1)
    }

    function applyImport() {
        if (!controller || rowsModel.count === 0)
            return

        const items = []
        for (let i = 0; i < rowsModel.count; ++i) {
            const row = rowsModel.get(i)
            items.push({
                filePath: row.filePath,
                studentId: Number(row.studentId),
                labId: Number(selectedLabId)
            })
        }

        const ok = controller.importBatchSubmissions(
            items,
            Number(defaultStudentId),
            Number(selectedLabId)
        )

        if (ok) {
            close()
            importCompleted()
        }
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    focus: true
    dim: false
    width: Math.min(parent ? parent.width - 40 : 1100, 1100)
    height: Math.min(parent ? parent.height - 40 : 720, 720)
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

    ListModel {
        id: rowsModel
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: "Добавление работ"
                color: "#edf4ff"
                font.pixelSize: 17
                font.bold: true
                Layout.fillWidth: true
            }

            ToolButton {
                id: closeButton
                implicitWidth: 36
                implicitHeight: 36
                hoverEnabled: true
                focusPolicy: Qt.NoFocus
                clip: true
                background: Rectangle {
                    radius: 10
                    color: closeButton.hovered ? "#2a415f" : "#1a2a40"
                    border.color: closeButton.hovered ? "#5889c3" : "#36557a"
                    border.width: 1
                }
                contentItem: Text {
                    text: "✕"
                    color: "#d8e8ff"
                    font.pixelSize: 16
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: root.close()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            AppComboBox {
                id: labCombo
                Layout.fillWidth: true
                model: controller ? controller.labWorks : []
                textRole: "title"
                valueRole: "id"
                onActivated: selectedLabId = Number(currentValue)
            }

            AppButton {
                text: "Выбрать файлы"
                Layout.preferredWidth: 160
                onClicked: filesDialog.open()
            }

            AppButton {
                text: "Выбрать папку"
                Layout.preferredWidth: 160
                onClicked: folderDialog.open()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 10
            color: "#121f31"
            border.color: "#304864"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "Файл / папка"
                        color: "#9eb8da"
                        font.bold: true
                        Layout.fillWidth: true
                    }

                    Label {
                        text: "Студент"
                        color: "#9eb8da"
                        font.bold: true
                        Layout.preferredWidth: 340
                    }

                    Item {
                        Layout.preferredWidth: 42
                    }
                }

                ListView {
                    id: rowsView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 6
                    model: rowsModel

                    delegate: Rectangle {
                        required property int index
                        required property string filePath
                        required property string fileName
                        required property int studentId

                        width: rowsView.width
                        height: 56
                        radius: 10
                        color: "#1a2a40"
                        border.color: "#355070"
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 8

                            Label {
                                Layout.fillWidth: true
                                text: fileName
                                color: "#edf4ff"
                                elide: Text.ElideMiddle
                            }

                            StudentPickerField {
                                Layout.preferredWidth: 340
                                studentsModel: controller ? controller.students : []
                                selectedStudentId: Number(studentId)
                                allowAllOption: true
                                allOptionText: "Студент не выбран"
                                allowAddAction: true
                                addActionText: "Добавить студента"
                                onStudentChosen: function(studentIdValue) {
                                    rowsModel.setProperty(index, "studentId", Number(studentIdValue))
                                }
                                onAddStudentRequested: addStudentDialog.openDialog()
                            }

                            AppButton {
                                Layout.preferredWidth: 42
                                text: "×"
                                quiet: true
                                danger: true
                                onClicked: root.removeRow(index)
                            }
                        }
                    }
                }

                Label {
                    visible: rowsModel.count === 0
                    text: "Добавьте файлы или папку."
                    color: "#8fa4c3"
                    font.pixelSize: 13
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                Layout.fillWidth: true
                text: rowsModel.count > 0 ? ("Элементов: " + rowsModel.count) : ""
                color: "#9db4d5"
                elide: Text.ElideRight
            }

            AppButton {
                text: "Отмена"
                quiet: true
                Layout.preferredWidth: 120
                onClicked: root.close()
            }

            AppButton {
                text: "Добавить"
                Layout.preferredWidth: 150
                enabled: rowsModel.count > 0 && selectedLabId > 0
                onClicked: root.applyImport()
            }
        }
    }

    onOpened: pickDefaultLab()

    FileDialog {
        id: filesDialog
        title: "Выбор файлов работ"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Файлы C++ (*.cpp *.cc *.cxx)", "Все файлы (*.*)"]
        onAccepted: root.appendPaths(selectedFiles)
    }

    FolderDialog {
        id: folderDialog
        title: "Выбор папки с работами"
        onAccepted: root.appendPaths([selectedFolder])
    }

    AddStudentDialog {
        id: addStudentDialog
        titleText: "Новый студент"

        onSubmitRequested: {
            if (!controller)
                return
            if (controller.addStudent(studentName, groupName))
                addStudentDialog.close()
        }
    }
}
