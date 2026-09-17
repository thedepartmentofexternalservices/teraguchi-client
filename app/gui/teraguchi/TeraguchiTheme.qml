import QtQuick 2.15

// App surfaces follow the system appearance. Controls use the macOS native style.
QtObject {
    readonly property SystemPalette system: SystemPalette {
        colorGroup: SystemPalette.Active
    }
    readonly property bool dark: Qt.styleHints.colorScheme === Qt.Dark
    readonly property color canvas: dark ? "#1E1E1E" : "#FFFFFF"
    readonly property color sidebar: dark ? "#282828" : "#F0F0F0"
    readonly property color toolbar: dark ? "#303030" : "#F6F6F6"
    readonly property color panel: dark ? "#292929" : "#F7F7F8"
    readonly property color text: dark ? "#F2F2F2" : "#1D1D1F"
    readonly property color muted: dark ? "#B1B1B6" : "#6E6E73"
    readonly property color quiet: muted
    readonly property color stroke: dark ? "#424244" : "#DDDDDF"
    readonly property color accent: system.highlight
    readonly property color selectedText: system.highlightedText
    readonly property color hover: dark ? "#383838" : "#E4E4E6"
    readonly property color available: dark ? "#32D74B" : "#248A3D"
    readonly property string sans: Qt.application.font.family
    readonly property int radius: 8
}
