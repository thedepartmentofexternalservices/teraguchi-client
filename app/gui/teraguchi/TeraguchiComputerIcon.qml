import QtQuick 2.15

// Original, simple workstation glyph; no downloaded icon or font assets.
Item {
    property color ink: "#737378"
    implicitWidth: 24
    implicitHeight: 24
    Rectangle {
        x: 2
        y: 3
        width: 20
        height: 14
        radius: 2
        color: "transparent"
        border.color: parent.ink
        border.width: 1.5
    }
    Rectangle {
        x: 11
        y: 17
        width: 2
        height: 3
        color: parent.ink
    }
    Rectangle {
        x: 7
        y: 20
        width: 10
        height: 1.5
        radius: 0.75
        color: parent.ink
    }
}
