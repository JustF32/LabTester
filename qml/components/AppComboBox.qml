import QtQuick
import QtQuick.Controls

ComboBox {
    id: control

    implicitHeight: 42
    font.pixelSize: 14
    padding: 10
    leftPadding: 12
    rightPadding: 36
    hoverEnabled: true
    focusPolicy: Qt.NoFocus

    function itemText(data) {
        if (data === undefined || data === null)
            return ""
        if (typeof data === "object" && control.textRole && data[control.textRole] !== undefined)
            return data[control.textRole]
        return data.toString()
    }

    delegate: ItemDelegate {
        id: optionDelegate
        width: ListView.view ? ListView.view.width : control.width
        text: control.itemText(modelData)
        highlighted: control.highlightedIndex === index
        hoverEnabled: true
        leftPadding: 12
        rightPadding: 12
        topPadding: 8
        bottomPadding: 8
        focusPolicy: Qt.NoFocus
        background: Rectangle {
            radius: 10
            color: optionDelegate.highlighted
                ? "#284f7f"
                : optionDelegate.hovered
                    ? "#1f3047"
                    : "transparent"
            border.width: optionDelegate.highlighted || optionDelegate.hovered ? 1 : 0
            border.color: optionDelegate.highlighted ? "#6ea6eb" : "#38577e"

            Behavior on color {
                ColorAnimation { duration: 130 }
            }

            Behavior on border.color {
                ColorAnimation { duration: 130 }
            }
        }
        contentItem: Text {
            text: optionDelegate.text
            color: "#eef4ff"
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    indicator: Text {
        x: control.width - width - control.rightPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        text: "▾"
        color: control.pressed ? "#9bc6ff" : "#c8daff"
        font.pixelSize: 16
        rotation: control.popup.visible ? 180 : 0

        Behavior on rotation {
            NumberAnimation {
                duration: 140
                easing.type: Easing.OutCubic
            }
        }

        Behavior on color {
            ColorAnimation { duration: 120 }
        }
    }

    contentItem: Text {
        leftPadding: 4
        rightPadding: control.indicator.width + control.spacing
        text: control.displayText
        font: control.font
        color: "#f0f5ff"
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 13
        border.width: 1
        border.color: control.hovered ? "#4c6f9f" : "#334a68"
        gradient: Gradient {
            GradientStop { position: 0; color: "#1b2636" }
            GradientStop { position: 1; color: "#141d2b" }
        }

        Behavior on border.color {
            ColorAnimation { duration: 130 }
        }
    }

    popup: Popup {
        y: control.height + 6
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 12, 300)
        padding: 6
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

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
                    from: control.height - 4
                    to: control.height + 6
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
                    from: control.height + 6
                    to: control.height - 2
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

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            spacing: 4
            ScrollIndicator.vertical: ScrollIndicator { }
        }
    }
}
