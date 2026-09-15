import QtQuick 2.15
import QtQuick.Controls 2.15

Button {
    id: control
    property bool primary: false
    property bool selected: false
    TeraguchiTheme {
        id: theme
    }
    implicitHeight: 48
    implicitWidth: Math.max(120, contentItem.implicitWidth + 40)
    leftPadding: 20
    rightPadding: 20
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    Accessible.name: text
    readonly property bool inverted: enabled && (primary ? !(hovered || down) : hovered || down)
    background: Rectangle {
        radius: theme.radius
        color: control.inverted ? theme.text : theme.canvas
        border.width: control.activeFocus || control.selected ? 2 : 1
        border.color: control.activeFocus || control.selected ? theme.accent : control.enabled ? theme.text : theme.stroke
    }
    contentItem: Text {
        text: control.text.toUpperCase()
        textFormat: Text.PlainText
        color: control.inverted ? theme.canvas : control.enabled || control.selected ? theme.text : theme.quiet
        font.family: theme.sans
        font.pixelSize: 12
        font.weight: Font.Medium
        font.letterSpacing: 1.1
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
