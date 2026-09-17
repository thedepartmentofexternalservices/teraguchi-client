import QtQuick 2.15
import QtQuick.Controls 2.15

// Preserve native drawing, insets, hover, pressed, disabled and focus states.
Button {
    property bool primary: false
    property bool selected: false
    highlighted: primary
    focusPolicy: Qt.StrongFocus
    font.pixelSize: 13
    Accessible.name: text
}
