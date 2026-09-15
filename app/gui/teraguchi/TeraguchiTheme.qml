import QtQuick 2.15

// 1986 Studios Coolant: README.md and colors_and_type.css are the design source.
// OKLCH accents are converted to sRGB and channel-clipped for Qt Quick colors.
QtObject {
    readonly property color canvas: "#0A0A0A"
    readonly property color panel: "#141414"
    readonly property color raised: "#1A1A1A"
    readonly property color hover: "#1A1A1A"
    readonly property color stroke: "#4A4845"
    readonly property color text: "#FFFFFF"
    readonly property color muted: "#C9C7C1"
    readonly property color quiet: "#8C8A85"
    readonly property color accent: "#0099AF"
    readonly property color accentInk: "#0A0A0A"
    readonly property color azure: "#3C79D1"
    readonly property color available: "#6FC267"
    readonly property color bone: "#F5F4F1"
    readonly property int radius: 0
    // Preview uses installed faces. Packaging must supply licensed font assets.
    readonly property string sans: Qt.fontFamilies().indexOf("Archivo") >= 0 ? "Archivo" : "Helvetica Neue"
    readonly property string display: Qt.fontFamilies().indexOf("Archivo Black") >= 0 ? "Archivo Black" : sans
    readonly property string mono: Qt.fontFamilies().indexOf("JetBrains Mono") >= 0 ? "JetBrains Mono" : "Menlo"
}
