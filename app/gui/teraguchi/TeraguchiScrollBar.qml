import QtQuick 2.15
import QtQuick.Controls 2.15

ScrollBar {
    id: control
    TeraguchiTheme {
        id: theme
    }
    policy: ScrollBar.AsNeeded
    active: true
    padding: 0
    implicitWidth: 6
    minimumSize: 0.1
    contentItem: Rectangle {
        implicitWidth: 6
        implicitHeight: 24
        color: control.pressed ? theme.accent : theme.muted
        visible: control.size < 1
    }
    background: Rectangle {
        color: theme.raised
        visible: control.size < 1
    }
}
