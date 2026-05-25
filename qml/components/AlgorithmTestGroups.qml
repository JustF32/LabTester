import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabTester

ColumnLayout {
    id: root

    property var cases: []
    property bool defaultExpanded: false
    property string emptyText: "Детальные тесты недоступны."
    property var groupedCases: []
    property var expandedGroups: ({})

    signal caseClicked(var testCase)

    spacing: 8

    function algorithmKey(testCase) {
        var name = String(testCase && testCase.testName ? testCase.testName : "")
        var suite = name.indexOf(".") >= 0 ? name.split(".")[0] : name
        var suiteParts = suite.split("/")
        var raw = suiteParts[suiteParts.length - 1]
        raw = raw.replace(/_(Basic|Advanced|Performance).*$/, "")

        if (raw.indexOf("BubbleSort") === 0)
            return "Сортировка пузырьком"
        if (raw.indexOf("SelectionSort") === 0)
            return "Сортировка выбором"
        if (raw.indexOf("InsertionSort") === 0)
            return "Сортировка вставками"
        if (raw.indexOf("MergeSort") === 0)
            return "Сортировка слиянием"
        if (raw.indexOf("HeapSort") === 0)
            return "Пирамидальная сортировка"
        if (raw.indexOf("QuickSort") === 0)
            return "Быстрая сортировка"
        if (raw.indexOf("LexicographicSort") === 0 || raw.indexOf("LexSort") === 0)
            return "Лексикографическая сортировка"
        if (raw.indexOf("Stack") === 0 || raw.indexOf("ArrayStack") === 0)
            return "Стек"
        if (raw.indexOf("Queue") === 0 || raw.indexOf("ArrayQueue") === 0)
            return "Очередь"
        if (raw.indexOf("Deque") === 0 || raw.indexOf("ArrayDeque") === 0)
            return "Дек"
        if (raw.indexOf("LinkedList") === 0)
            return "Связный список"
        if (raw.indexOf("BinarySearchTree") === 0 || raw.indexOf("BST") === 0)
            return "Бинарное дерево поиска"
        if (raw.indexOf("HashFunctions") === 0)
            return "Хеш-функции"
        if (raw.indexOf("HashDistribution") === 0)
            return "Распределение хешей"
        if (raw.indexOf("HashTable") === 0)
            return "Хеш-таблица"
        if (raw.indexOf("RabinKarp") === 0)
            return "Рабин-Карп"
        if (raw.indexOf("KMP") === 0)
            return "Кнут-Моррис-Пратт"
        if (raw.indexOf("BoyerMoore") === 0)
            return "Бойер-Мур"
        if (raw.indexOf("AhoCorasick") === 0)
            return "Ахо-Корасик"
        if (name === "Подготовка/сборка" || name === "Удаленный запуск")
            return "Подготовка запуска"

        return raw.length > 0 ? raw : "Прочие тесты"
    }

    function tierKey(testCase) {
        var name = String(testCase && testCase.testName ? testCase.testName : "")
        if (name.indexOf("_Performance") >= 0 || name.indexOf("Performance") >= 0)
            return "performance"
        if (name.indexOf("_Advanced") >= 0 || name.indexOf("Advanced") >= 0)
            return "advanced"
        return "basic"
    }

    function tierLabel(key) {
        if (key === "performance")
            return "3 звезды"
        if (key === "advanced")
            return "2 звезды"
        return "1 звезда"
    }

    function tierRank(key) {
        if (key === "basic")
            return 0
        if (key === "advanced")
            return 1
        return 2
    }

    function groupRank(name) {
        var order = [
            "Сортировка пузырьком",
            "Сортировка выбором",
            "Сортировка вставками",
            "Сортировка слиянием",
            "Пирамидальная сортировка",
            "Быстрая сортировка",
            "Лексикографическая сортировка",
            "Подготовка запуска",
            "Стек",
            "Очередь",
            "Дек",
            "Связный список",
            "Бинарное дерево поиска",
            "Хеш-функции",
            "Распределение хешей",
            "Хеш-таблица",
            "Рабин-Карп",
            "Кнут-Моррис-Пратт",
            "Бойер-Мур",
            "Ахо-Корасик",
        ]
        var index = order.indexOf(name)
        return index >= 0 ? index : order.length
    }

    function shortTestName(testCase) {
        var name = String(testCase && testCase.testName ? testCase.testName : "")
        if (name.indexOf(".") >= 0)
            name = name.split(".").slice(1).join(".")
        var slash = name.lastIndexOf("/")
        if (slash >= 0 && slash + 1 < name.length)
            name = name.substring(slash + 1)
        return name.length > 0 ? name : "Тест"
    }

    function buildGroups() {
        var groups = []
        var byName = {}
        var source = cases || []

        for (var i = 0; i < source.length; ++i) {
            var testCase = source[i]
            var groupName = algorithmKey(testCase)
            var group = byName[groupName]
            if (!group) {
                group = {
                    name: groupName,
                    tests: [],
                    passed: 0,
                    total: 0,
                    durationMs: 0
                }
                byName[groupName] = group
                groups.push(group)
            }

            var passed = !!(testCase && testCase.passed)
            var tier = tierKey(testCase)
            group.tests.push({
                raw: testCase,
                title: shortTestName(testCase),
                tier: tier,
                tierText: tierLabel(tier),
                passed: passed,
                durationMs: Number(testCase && testCase.durationMs ? testCase.durationMs : 0),
                status: testCase && testCase.status ? testCase.status : (passed ? "Успешно" : "Провалено"),
                isBuildErrorCase: !!(testCase && testCase.isBuildErrorCase)
            })
            group.total += 1
            group.durationMs += Number(testCase && testCase.durationMs ? testCase.durationMs : 0)
            if (passed)
                group.passed += 1
        }

        for (var g = 0; g < groups.length; ++g) {
            groups[g].tests.sort(function(a, b) {
                var tierDelta = tierRank(a.tier) - tierRank(b.tier)
                if (tierDelta !== 0)
                    return tierDelta
                return a.title.localeCompare(b.title)
            })
        }

        groups.sort(function(a, b) {
            var rankDelta = groupRank(a.name) - groupRank(b.name)
            if (rankDelta !== 0)
                return rankDelta
            return a.name.localeCompare(b.name)
        })

        return groups
    }

    function refresh() {
        groupedCases = buildGroups()
    }

    function isGroupExpanded(groupName) {
        if (expandedGroups && expandedGroups[groupName] !== undefined)
            return !!expandedGroups[groupName]
        return root.defaultExpanded
    }

    function setGroupExpanded(groupName, expanded) {
        var next = {}
        for (var key in expandedGroups)
            next[key] = expandedGroups[key]
        next[groupName] = expanded
        expandedGroups = next
    }

    Component.onCompleted: refresh()
    onCasesChanged: refresh()

    Rectangle {
        visible: root.groupedCases.length === 0
        Layout.fillWidth: true
        implicitHeight: 46
        radius: 8
        color: "#1f3049"
        border.color: "#355070"

        Label {
            anchors.centerIn: parent
            text: root.emptyText
            color: "#9cb1d0"
        }
    }

    Repeater {
        model: root.groupedCases

        delegate: Rectangle {
            id: groupBlock

            required property var modelData
            property bool expanded: root.isGroupExpanded(modelData.name)

            Layout.fillWidth: true
            implicitHeight: groupContent.implicitHeight + 16
            radius: 10
            color: "#16263b"
            border.color: modelData.passed === modelData.total ? "#356d62" : "#76516a"
            border.width: 1
            clip: true

            ColumnLayout {
                id: groupContent
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                Rectangle {
                    id: groupHeader
                    Layout.fillWidth: true
                    implicitHeight: 48
                    radius: 8
                    color: groupMouse.containsMouse ? "#1f334d" : "transparent"

                    MouseArea {
                        id: groupMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.setGroupExpanded(modelData.name, !groupBlock.expanded)
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 10

                        Label {
                            text: groupBlock.expanded ? "v" : ">"
                            color: "#d8e8ff"
                            font.pixelSize: 17
                            font.bold: true
                            Layout.preferredWidth: 20
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Label {
                                text: modelData.name
                                color: "#edf4ff"
                                font.bold: true
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            Label {
                                text: modelData.passed + "/" + modelData.total + " тестов"
                                      + (modelData.durationMs > 0 ? " • " + modelData.durationMs + " мс" : "")
                                color: "#9fb8dc"
                                font.pixelSize: 12
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                        }

                        StatusBadge {
                            status: modelData.passed === modelData.total ? "Успешно" : "Есть ошибки"
                            statusKey: modelData.passed === modelData.total ? "Passed" : "Failed"
                        }
                    }
                }

                Item {
                    id: groupTestsClip
                    Layout.fillWidth: true
                    Layout.preferredHeight: groupBlock.expanded ? groupTests.implicitHeight : 0
                    clip: true

                    Behavior on Layout.preferredHeight {
                        NumberAnimation { duration: 190; easing.type: Easing.OutCubic }
                    }

                    ColumnLayout {
                        id: groupTests
                        width: parent.width
                        spacing: 6

                        Repeater {
                            model: groupBlock.modelData.tests

                            delegate: Rectangle {
                                required property var modelData

                                Layout.fillWidth: true
                                implicitHeight: 48
                                radius: 8
                                color: testHover.containsMouse
                                       ? (modelData.passed ? "#1f463b" : "#50303a")
                                       : (modelData.passed ? "#1a3a32" : "#432b34")
                                border.color: modelData.passed ? "#2f7c67" : "#9e5160"
                                border.width: 1

                                MouseArea {
                                    id: testHover
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    enabled: groupBlock.expanded && groupTestsClip.Layout.preferredHeight > 1
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.caseClicked(modelData.raw)
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 9
                                    spacing: 10

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1

                                        Label {
                                            text: modelData.title
                                            color: "#edf4ff"
                                            font.bold: true
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                        }

                                        Label {
                                            text: modelData.tierText
                                            color: "#9fb8dc"
                                            font.pixelSize: 12
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                        }
                                    }

                                    Label {
                                        text: modelData.durationMs + " мс"
                                        color: "#9bb2d3"
                                        font.pixelSize: 12
                                    }

                                    StatusBadge {
                                        status: modelData.status
                                        statusKey: modelData.passed ? "Passed" : (modelData.isBuildErrorCase ? "BuildError" : "Failed")
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
