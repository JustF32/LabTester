import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var studentsModel: []
    property int selectedStudentId: 0
    property string selectedStudentName: ""
    property string placeholderText: "Выберите студента"
    property bool allowAddAction: false
    property string addActionText: "Добавить студента"
    property bool allowAllOption: false
    property string allOptionText: "Все студенты"
    property int popupMaxHeight: 360

    signal studentChosen(int studentId, string studentName, string groupName)
    signal addStudentRequested()

    implicitHeight: 42
    implicitWidth: 300

    property string searchText: ""
    property string groupFilter: ""
    property var groupOptions: []
    property var filteredStudents: []

    readonly property string displayText: {
        if (selectedStudentId === 0 && allowAllOption)
            return allOptionText
        if (selectedStudentName.length > 0)
            return selectedStudentName
        return placeholderText
    }

    function rebuildGroupOptions() {
        const options = [{ name: "", title: "Все группы" }]
        const values = studentsModel || []

        for (let i = 0; i < values.length; ++i) {
            const groupName = (values[i].group || "").toString().trim()
            if (groupName.length === 0)
                continue

            let exists = false
            for (let j = 1; j < options.length; ++j) {
                if (options[j].name === groupName) {
                    exists = true
                    break
                }
            }
            if (!exists)
                options.push({ name: groupName, title: groupName })
        }

        options.sort(function(a, b) {
            if (a.name.length === 0)
                return -1
            if (b.name.length === 0)
                return 1
            return a.name.localeCompare(b.name, "ru-RU")
        })

        groupOptions = options

        let groupIndex = 0
        for (let k = 0; k < groupOptions.length; ++k) {
            if (groupOptions[k].name === groupFilter) {
                groupIndex = k
                break
            }
        }

        groupCombo.currentIndex = groupIndex
        groupFilter = groupOptions[groupIndex].name || ""
    }

    function syncSelectedStudentName() {
        if (selectedStudentId === 0 && allowAllOption) {
            selectedStudentName = allOptionText
            return
        }

        const values = studentsModel || []
        for (let i = 0; i < values.length; ++i) {
            const item = values[i]
            if (Number(item.id) === selectedStudentId) {
                selectedStudentName = item.name || ""
                return
            }
        }

        selectedStudentName = ""
    }

    function rebuildFilteredStudents() {
        const values = studentsModel || []
        const next = []
        const query = searchText.trim().toLowerCase()
        const selectedGroup = groupFilter.trim().toLowerCase()

        if (allowAllOption) {
            const allName = allOptionText.toLowerCase()
            const allMatchesSearch = query.length === 0 || allName.indexOf(query) >= 0
            if (allMatchesSearch && selectedGroup.length === 0) {
                next.push({
                    id: 0,
                    name: allOptionText,
                    group: ""
                })
            }
        }

        for (let i = 0; i < values.length; ++i) {
            const item = values[i]
            const name = (item.name || "").toString()
            const groupName = (item.group || "").toString()

            if (selectedGroup.length > 0 && groupName.toLowerCase() !== selectedGroup)
                continue

            const combined = (name + " " + groupName).toLowerCase()
            if (query.length > 0 && combined.indexOf(query) < 0)
                continue

            next.push({
                id: Number(item.id),
                name: name,
                group: groupName
            })
        }

        filteredStudents = next
    }

    function chooseStudent(item) {
        selectedStudentName = item.name || ""
        popup.close()
        studentChosen(Number(item.id), selectedStudentName, item.group || "")
    }

    function openPopup() {
        searchText = ""
        rebuildFilteredStudents()
        popup.open()
        searchField.forceActiveFocus()
        searchField.selectAll()
    }

    onStudentsModelChanged: {
        rebuildGroupOptions()
        syncSelectedStudentName()
        rebuildFilteredStudents()
    }

    onSelectedStudentIdChanged: syncSelectedStudentName()
    onSearchTextChanged: rebuildFilteredStudents()
    onGroupFilterChanged: rebuildFilteredStudents()

    Component.onCompleted: {
        rebuildGroupOptions()
        syncSelectedStudentName()
        rebuildFilteredStudents()
    }

    Rectangle {
        anchors.fill: parent
        radius: 13
        border.width: 1
        border.color: mouseArea.containsMouse ? "#4c6f9f" : "#334a68"
        gradient: Gradient {
            GradientStop { position: 0; color: "#1b2636" }
            GradientStop { position: 1; color: "#141d2b" }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 10
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: root.displayText
                color: root.selectedStudentName.length > 0 || (root.selectedStudentId === 0 && root.allowAllOption)
                    ? "#eef4ff"
                    : "#7087a9"
                elide: Text.ElideRight
            }

            Label {
                text: "▾"
                color: "#c8daff"
                font.pixelSize: 14
                rotation: popup.visible ? 180 : 0

                Behavior on rotation {
                    NumberAnimation {
                        duration: 140
                        easing.type: Easing.OutCubic
                    }
                }
            }
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.openPopup()
        }
    }

    Popup {
        id: popup
        y: root.height + 6
        width: root.width
        modal: false
        focus: true
        padding: 8
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        implicitHeight: Math.min(contentColumn.implicitHeight + 16, root.popupMaxHeight)

        enter: Transition {
            ParallelAnimation {
                NumberAnimation {
                    property: "opacity"
                    from: 0
                    to: 1
                    duration: 150
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    property: "y"
                    from: root.height - 4
                    to: root.height + 6
                    duration: 150
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
                    duration: 110
                    easing.type: Easing.InCubic
                }
                NumberAnimation {
                    property: "y"
                    from: root.height + 6
                    to: root.height - 2
                    duration: 110
                    easing.type: Easing.InCubic
                }
            }
        }

        background: Rectangle {
            radius: 14
            border.color: "#3f5a80"
            color: "#121a26"
        }

        contentItem: ColumnLayout {
            id: contentColumn
            spacing: 8

            TextField {
                id: searchField
                Layout.fillWidth: true
                text: root.searchText
                placeholderText: "Поиск студента"
                selectByMouse: true
                color: "#eef4ff"
                selectedTextColor: "#f8fbff"
                selectionColor: "#3e6ea8"
                placeholderTextColor: "#7087a9"

                background: Rectangle {
                    radius: 10
                    border.width: 1
                    border.color: searchField.activeFocus ? "#5b85bd" : "#344c6e"
                    gradient: Gradient {
                        GradientStop { position: 0; color: "#1b2636" }
                        GradientStop { position: 1; color: "#141d2b" }
                    }
                }

                onTextChanged: root.searchText = text
            }

            AppComboBox {
                id: groupCombo
                Layout.fillWidth: true
                model: root.groupOptions
                textRole: "title"
                valueRole: "name"
                onActivated: {
                    root.groupFilter = groupCombo.currentValue ? groupCombo.currentValue.toString() : ""
                }
            }

            ListView {
                id: studentList
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 220
                clip: true
                spacing: 6
                model: root.filteredStudents

                delegate: Rectangle {
                    required property var modelData

                    width: studentList.width
                    height: 50
                    radius: 10
                    color: Number(modelData.id) === root.selectedStudentId ? "#25456a" : "#1b2b42"
                    border.width: 1
                    border.color: Number(modelData.id) === root.selectedStudentId ? "#6da6eb" : "#36557a"

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.chooseStudent(modelData)
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 8

                        Label {
                            Layout.fillWidth: true
                            text: modelData.name
                            color: "#eef4ff"
                            elide: Text.ElideRight
                        }

                        Label {
                            text: modelData.group && modelData.group.length > 0 ? modelData.group : ""
                            visible: modelData.group && modelData.group.length > 0
                            color: "#9fb8d8"
                            font.pixelSize: 12
                        }
                    }
                }
            }

            Label {
                visible: root.filteredStudents.length === 0
                text: "Ничего не найдено"
                color: "#8fa4c3"
                font.pixelSize: 12
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }

            AppButton {
                visible: root.allowAddAction
                Layout.fillWidth: true
                text: root.addActionText
                quiet: true
                onClicked: {
                    popup.close()
                    root.addStudentRequested()
                }
            }
        }
    }
}
