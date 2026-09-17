import QtQuick 2.15
import QtQuick.Controls 2.15

RadioButton {
    required property int displayCount
    property bool selected: false
    text: displayCount === 1 ? qsTr("One display") : qsTr("Two displays")
    checked: selected
    focusPolicy: Qt.StrongFocus
    font.pixelSize: 13
    Accessible.name: text
    Accessible.description: selected ? qsTr("Selected") : qsTr("Not selected")
}
