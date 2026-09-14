import QtQuick 2.15
import QtQuick.Controls 2.15

Button {
    id: control
    property bool primary: false
    property bool selected: false
    TeraguchiTheme {
        id: theme
    }
    implicitHeight: 44
    implicitWidth: Math.max(110, contentItem.implicitWidth + 32)
    leftPadding: 16
    rightPadding: 16
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    Accessible.name: text
    background: Rectangle {
        radius: theme.radius
        color: !control.enabled ? (control.selected ? "#314339" : theme.panel) : control.primary ? theme.accent : control.down || control.hovered ? theme.hover : theme.raised
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? theme.accent : theme.stroke
    }
    contentItem: Text {
        text: control.text
        textFormat: Text.PlainText
        color: !control.enabled ? (control.selected ? theme.accent : theme.quiet) : control.primary ? theme.accentInk : theme.text
        font.pixelSize: 14
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
