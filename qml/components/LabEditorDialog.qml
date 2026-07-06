import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import LabTester

Popup {
    id: root

    property var controller
    property bool editMode: false
    property int editingLabId: 0
    property string originalTemplatePath: ""
    property string originalTestPath: ""
    property string selectedTemplatePath: ""
    property string selectedTestPath: ""

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    focus: true
    dim: false
    width: Math.min(parent ? parent.width - 40 : 760, 760)
    height: 530
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function fileName(path) {
        const value = path ? path.toString().trim() : ""
        if (value.length === 0)
            return "—"
        const normalized = value.replace(/\\/g, "/")
        const parts = normalized.split("/")
        const last = parts.length > 0 ? parts[parts.length - 1] : normalized
        return last.length > 0 ? last : normalized
    }

    function openForCreate() {
        editMode = false
        editingLabId = 0
        titleField.text = ""
        descriptionField.text = ""
        languageCombo.currentIndex = 0
        originalTemplatePath = ""
        originalTestPath = ""
        selectedTemplatePath = ""
        selectedTestPath = ""
        hintLabel.text = ""
        open()
    }

    function openForEdit(labData) {
        editMode = true
        editingLabId = Number(labData.id || 0)
        titleField.text = (labData.title || "").toString()
        descriptionField.text = (labData.description || "").toString()

        const lang = (labData.language || "C++").toString()
        let langIndex = 0
        for (let i = 0; i < languageOptions.length; ++i) {
            if (languageOptions[i] === lang) {
                langIndex = i
                break
            }
        }
        languageCombo.currentIndex = langIndex

        originalTemplatePath = (labData.templateFile || "").toString()
        originalTestPath = (labData.testSuitePath || "").toString()
        selectedTemplatePath = ""
        selectedTestPath = ""
        hintLabel.text = ""
        open()
    }

    function save() {
        const title = titleField.text.trim()
        const description = descriptionField.text.trim()
        const language = languageCombo.currentText
        const templatePath = selectedTemplatePath.trim()
        const testPath = selectedTestPath.trim()

        if (title.length === 0) {
            hintLabel.text = "Введите название лабораторной."
            return
        }

        let ok = false
        if (editMode) {
            ok = controller && controller.updateLabWork(
                editingLabId,
                title,
                description,
                templatePath,
                testPath,
                language
            )
        } else {
            if (templatePath.length === 0 || testPath.length === 0) {
                hintLabel.text = "Для новой лабораторной выберите шаблон и файл тестов."
                return
            }
            ok = controller && controller.createLabWork(
                title,
                description,
                templatePath,
                testPath,
                language
            )
        }

        if (!ok) {
            hintLabel.text = controller && controller.lastRunMessage && controller.lastRunMessage.length > 0
                ? controller.lastRunMessage
                : "Не удалось сохранить лабораторную."
            return
        }

        close()
    }

    function localPathFromUrl(url) {
        if (!url)
            return ""
        let value = url.toString()
        if (value.startsWith("file:///"))
            value = value.substring(8)
        value = decodeURIComponent(value)
        if (Qt.platform.os === "windows" && value.length > 2 && value[0] === "/")
            value = value.substring(1)
        return value
    }

    readonly property var languageOptions: ["C++"]

    Overlay.modal: Rectangle {
        color: "transparent"
    }

    background: Rectangle {
        radius: 14
        color: "#101a29"
        border.color: "#39567f"
        border.width: 1
    }

    onOpened: titleField.forceActiveFocus()

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: root.editMode ? "Редактирование лабораторной" : "Новая лабораторная"
                color: "#edf4ff"
                font.pixelSize: 16
                font.bold: true
                Layout.fillWidth: true
                elide: Text.ElideRight
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
                    text: "×"
                    color: "#d8e8ff"
                    font.pixelSize: 16
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: root.close()
            }
        }

        Label {
            id: hintLabel
            Layout.fillWidth: true
            visible: text.length > 0
            color: "#ffb7c2"
            wrapMode: Text.Wrap
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "Название"
                color: "#9fb8d8"
                font.pixelSize: 12
            }

            TextField {
                id: titleField
                Layout.fillWidth: true
                placeholderText: "Например: Лабораторная 4: Графы"
                selectByMouse: true
                color: "#eef4ff"
                selectedTextColor: "#f8fbff"
                selectionColor: "#3e6ea8"
                placeholderTextColor: "#7087a9"
                background: Rectangle {
                    radius: 10
                    border.width: 1
                    border.color: titleField.activeFocus ? "#5b85bd" : "#344c6e"
                    gradient: Gradient {
                        GradientStop { position: 0; color: "#1b2636" }
                        GradientStop { position: 1; color: "#141d2b" }
                    }
                }
            }

            Label {
                text: "Описание"
                color: "#9fb8d8"
                font.pixelSize: 12
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 104
                radius: 10
                border.width: 1
                border.color: descriptionField.activeFocus ? "#5b85bd" : "#344c6e"
                gradient: Gradient {
                    GradientStop { position: 0; color: "#1b2636" }
                    GradientStop { position: 1; color: "#141d2b" }
                }

                TextArea {
                    id: descriptionField
                    anchors.fill: parent
                    anchors.margins: 8
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    color: "#eef4ff"
                    selectedTextColor: "#f8fbff"
                    selectionColor: "#3e6ea8"
                    placeholderText: "Кратко опишите задачу, требования и ограничения."
                    placeholderTextColor: "#7087a9"
                    background: null
                }
            }

            Label {
                text: "Язык"
                color: "#9fb8d8"
                font.pixelSize: 12
            }

            AppComboBox {
                id: languageCombo
                Layout.fillWidth: true
                model: root.languageOptions
            }

            Label {
                text: "Шаблон файла"
                color: "#9fb8d8"
                font.pixelSize: 12
            }

            FilePickerField {
                Layout.fillWidth: true
                text: root.selectedTemplatePath.length > 0
                    ? root.fileName(root.selectedTemplatePath)
                    : root.fileName(root.originalTemplatePath)
                placeholderText: "Выберите файл шаблона (*.cpp)"
                onBrowseRequested: templateDialog.open()
            }

            Label {
                text: "Файл тестов"
                color: "#9fb8d8"
                font.pixelSize: 12
            }

            FilePickerField {
                Layout.fillWidth: true
                text: root.selectedTestPath.length > 0
                    ? root.fileName(root.selectedTestPath)
                    : root.fileName(root.originalTestPath)
                placeholderText: "Выберите файл тестов (*.cpp)"
                onBrowseRequested: testsDialog.open()
            }
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
                onClicked: root.close()
            }

            AppButton {
                text: root.editMode ? "Сохранить" : "Создать"
                Layout.preferredWidth: 140
                onClicked: root.save()
            }
        }
    }

    FileDialog {
        id: templateDialog
        title: "Выберите файл шаблона"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Файлы C++ (*.cpp *.cc *.cxx *.h *.hpp)", "Все файлы (*.*)"]
        onAccepted: {
            if (selectedFile)
                root.selectedTemplatePath = root.localPathFromUrl(selectedFile)
        }
    }

    FileDialog {
        id: testsDialog
        title: "Выберите файл тестов"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Файлы C++ (*.cpp *.cc *.cxx)", "Все файлы (*.*)"]
        onAccepted: {
            if (selectedFile)
                root.selectedTestPath = root.localPathFromUrl(selectedFile)
        }
    }
}
