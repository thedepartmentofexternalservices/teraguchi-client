import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Button {
    id: control
    required property int displayCount
    property bool selected: false
    TeraguchiTheme {
        id: theme
    }
    text: displayCount === 1 ? qsTr("1 display") : qsTr("2 displays")
    implicitHeight: 100
    implicitWidth: 180
    padding: 16
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    Accessible.name: text
    Accessible.description: selected ? qsTr("Selected") : qsTr("Not selected")
    background: Rectangle {
        color: control.enabled && control.hovered ? theme.raised : theme.canvas
        border.width: control.activeFocus || control.selected ? 2 : 1
        border.color: control.activeFocus || control.selected ? theme.accent : theme.stroke
    }
    contentItem: ColumnLayout {
        spacing: 12
        Row {
            spacing: 6
            Repeater {
                model: control.displayCount
                Item {
                    width: 48
                    height: 36
                    Rectangle {
                        width: 48
                        height: 29
                        color: "transparent"
                        border.width: 1
                        border.color: control.selected ? theme.accent : theme.muted
                    }
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: 29
                        width: 1
                        height: 6
                        color: control.selected ? theme.accent : theme.muted
                    }
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: 35
                        width: 16
                        height: 1
                        color: control.selected ? theme.accent : theme.muted
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: control.text.toUpperCase()
                font.family: theme.sans
                font.pixelSize: 12
                font.letterSpacing: 1
                color: control.enabled || control.selected ? theme.text : theme.quiet
                elide: Text.ElideRight
            }
            Label {
                text: control.selected ? qsTr("SET") : ""
                font.family: theme.mono
                font.pixelSize: 10
                color: theme.accent
            }
        }
    }
}
