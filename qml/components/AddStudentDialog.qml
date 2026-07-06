import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root

    property string titleText: "Добавить студента"
    property string submitButtonText: "Добавить"
    signal submitRequested(string studentName, string groupName)

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    focus: true
    dim: false
    width: Math.min(parent ? parent.width - 40 : 520, 520)
    height: 310
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function openDialog(initialName, initialGroup) {
        const nameValue = initialName !== undefined && initialName !== null
            ? initialName.toString()
            : ""
        const groupValue = initialGroup !== undefined && initialGroup !== null
            ? initialGroup.toString()
            : ""
        nameField.text = nameValue
        groupField.text = groupValue
        hintLabel.text = ""
        open()
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

    onOpened: nameField.forceActiveFocus()

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: root.titleText
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
                text: "ФИО"
                color: "#9fb8d8"
                font.pixelSize: 12
            }

            TextField {
                id: nameField
                Layout.fillWidth: true
                placeholderText: "Например: Иван Петров"
                selectByMouse: true
                color: "#eef4ff"
                selectedTextColor: "#f8fbff"
                selectionColor: "#3e6ea8"
                placeholderTextColor: "#7087a9"
                background: Rectangle {
                    radius: 10
                    border.width: 1
                    border.color: nameField.activeFocus ? "#5b85bd" : "#344c6e"
                    gradient: Gradient {
                        GradientStop { position: 0; color: "#1b2636" }
                        GradientStop { position: 1; color: "#141d2b" }
                    }
                }
            }

            Label {
                text: "Группа"
                color: "#9fb8d8"
                font.pixelSize: 12
            }

            TextField {
                id: groupField
                Layout.fillWidth: true
                placeholderText: "Например: СЕ-23"
                selectByMouse: true
                color: "#eef4ff"
                selectedTextColor: "#f8fbff"
                selectionColor: "#3e6ea8"
                placeholderTextColor: "#7087a9"
                background: Rectangle {
                    radius: 10
                    border.width: 1
                    border.color: groupField.activeFocus ? "#5b85bd" : "#344c6e"
                    gradient: Gradient {
                        GradientStop { position: 0; color: "#1b2636" }
                        GradientStop { position: 1; color: "#141d2b" }
                    }
                }
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
                text: root.submitButtonText
                Layout.preferredWidth: 130
                enabled: nameField.text.trim().length > 0 && groupField.text.trim().length > 0
                onClicked: {
                    const name = nameField.text.trim()
                    const group = groupField.text.trim()
                    if (name.length === 0 || group.length === 0) {
                        hintLabel.text = "Заполните имя и группу."
                        return
                    }
                    hintLabel.text = ""
                    root.submitRequested(name, group)
                }
            }
        }
    }
}



