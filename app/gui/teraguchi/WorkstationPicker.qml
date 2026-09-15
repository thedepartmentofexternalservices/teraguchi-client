import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: page
    required property WorkstationFlow flow
    property bool detailsOpen: false
    color: theme.canvas
    TeraguchiTheme {
        id: theme
    }

    function statusText(status) {
        if (status === "ready")
            return qsTr("Available");
        if (status === "offline")
            return qsTr("Offline");
        if (status === "occupied")
            return qsTr("In use");
        if (status === "incompatible")
            return qsTr("Needs attention");
        return qsTr("Not checked");
    }
    function statusHelp(status) {
        if (status === "ready")
            return qsTr("Ready for a connection check.");
        if (status === "offline")
            return qsTr("Check your network connection. If it stays offline, contact your studio administrator.");
        if (status === "occupied")
            return qsTr("Another artist is connected. This workstation will be available when they disconnect.");
        if (status === "incompatible")
            return qsTr("This workstation needs a compatibility check. Contact your studio administrator before connecting.");
        return qsTr("Refresh to check whether this workstation is available.");
    }
    function heading() {
        if (flow.phase === "checking")
            return qsTr("Checking your connection");
        if (flow.phase === "connecting")
            return qsTr("Opening your workstation");
        if (flow.phase === "connected")
            return qsTr("You're connected");
        if (flow.phase === "interrupted")
            return qsTr("Connection interrupted");
        if (flow.phase === "blocked")
            return flow.problemTitle;
        return flow.selected ? flow.selected.name : flow.workstations.length === 0 ? qsTr("Your workstation will appear here") : qsTr("Choose a workstation");
    }
    function description() {
        if (flow.phase === "checking")
            return qsTr("Confirming access, picture requirements, and your selected displays.");
        if (flow.phase === "connecting")
            return qsTr("Starting the session with your selected display layout.");
        if (flow.phase === "connected")
            return qsTr("Disconnect when you're finished. Your desktop and applications stay open.");
        if (flow.phase === "interrupted")
            return qsTr("The client lost its connection. Try reconnecting to the same workstation, or return to the list.");
        if (flow.phase === "blocked")
            return flow.problem;
        return flow.selected ? statusHelp(flow.selected.status) : qsTr("Only workstations assigned to you appear here. Contact your studio administrator if one is missing.");
    }

    readonly property bool compact: width < 1000
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: page.compact ? 24 : 32
        spacing: page.compact ? 20 : 24
        RowLayout {
            Layout.fillWidth: true
            spacing: 24
            Label {
                text: "TERAGUCHI"
                color: theme.text
                font.family: theme.display
                font.pixelSize: page.compact ? 38 : 52
                font.letterSpacing: -1.5
            }
            Item {
                Layout.fillWidth: true
            }
            ColumnLayout {
                spacing: 8
                Label {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("REMOTE WORKSTATIONS")
                    color: theme.muted
                    font.family: theme.mono
                    font.pixelSize: 10
                    font.letterSpacing: 1
                }
                Label {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("01 / CONNECT")
                    color: theme.text
                    font.family: theme.mono
                    font.pixelSize: 11
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: theme.text
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: page.compact ? 24 : 32
            ColumnLayout {
                Layout.preferredWidth: page.compact ? 208 : 248
                Layout.fillHeight: true
                spacing: 16
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: qsTr("WORKSTATIONS")
                        color: theme.text
                        font.family: theme.sans
                        font.pixelSize: 12
                        font.letterSpacing: 1.4
                        font.weight: Font.Medium
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    Label {
                        text: (flow.workstations.length < 10 ? "0" : "") + flow.workstations.length
                        color: theme.quiet
                        font.family: theme.mono
                        font.pixelSize: 12
                    }
                }
                Label {
                    text: qsTr("Assigned to you")
                    color: theme.muted
                    font.family: theme.sans
                    font.pixelSize: 12
                }
                ListView {
                    id: list
                    objectName: "workstationList"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 0
                    model: flow.workstations
                    ScrollBar.vertical: TeraguchiScrollBar {}
                    delegate: ItemDelegate {
                        id: row
                        required property var modelData
                        required property int index
                        readonly property bool chosen: flow.selectedId === modelData.id
                        readonly property color ink: chosen ? theme.canvas : theme.text
                        objectName: "host-" + modelData.id
                        width: ListView.view.width
                        height: 80
                        leftPadding: 16
                        rightPadding: 16
                        enabled: flow.canChoose
                        hoverEnabled: true
                        focusPolicy: Qt.StrongFocus
                        Accessible.name: modelData.name + ", " + page.statusText(modelData.status)
                        onClicked: flow.selectWorkstation(modelData.id)
                        background: Rectangle {
                            color: row.chosen ? theme.bone : row.hovered ? theme.hover : theme.canvas
                            border.width: row.activeFocus ? 2 : 0
                            border.color: theme.accent
                            Rectangle {
                                width: 3
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                color: theme.accent
                                visible: row.chosen
                            }
                            Rectangle {
                                anchors.bottom: parent.bottom
                                width: parent.width
                                height: 1
                                color: theme.stroke
                                visible: !row.chosen && !row.activeFocus
                            }
                        }
                        contentItem: RowLayout {
                            spacing: 14
                            Label {
                                Layout.alignment: Qt.AlignTop
                                Layout.topMargin: 12
                                text: (row.index < 9 ? "0" : "") + (row.index + 1)
                                color: row.chosen ? theme.stroke : theme.quiet
                                font.family: theme.mono
                                font.pixelSize: 10
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Label {
                                    Layout.fillWidth: true
                                    text: row.modelData.name
                                    textFormat: Text.PlainText
                                    elide: Text.ElideRight
                                    color: row.ink
                                    font.family: theme.sans
                                    font.pixelSize: 17
                                    font.weight: Font.Bold
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: page.statusText(row.modelData.status)
                                    color: row.chosen ? theme.stroke : theme.muted
                                    font.family: theme.sans
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                            }
                            Label {
                                text: row.chosen ? "→" : ""
                                color: row.ink
                                font.family: theme.sans
                                font.pixelSize: 20
                            }
                        }
                    }
                    Label {
                        anchors.fill: parent
                        visible: flow.workstations.length === 0
                        text: qsTr("No workstations assigned yet.")
                        wrapMode: Text.WordWrap
                        color: theme.muted
                        font.family: theme.sans
                        font.pixelSize: 14
                    }
                }
                TeraguchiButton {
                    objectName: "refreshButton"
                    text: qsTr("Refresh status")
                    Layout.fillWidth: true
                    enabled: !flow.busy && !flow.sessionOpen
                    onClicked: flow.refresh()
                }
            }
            Rectangle {
                Layout.fillHeight: true
                width: 1
                color: theme.stroke
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 16
                ScrollView {
                    id: contentScroll
                    objectName: "connectionScroll"
                    rightPadding: 14
                    ScrollBar.vertical: TeraguchiScrollBar {
                        objectName: "connectionScrollBar"
                        parent: contentScroll
                        x: contentScroll.width - width
                        y: contentScroll.topPadding
                        height: contentScroll.availableHeight
                    }
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentWidth: availableWidth
                    ColumnLayout {
                        width: contentScroll.availableWidth
                        spacing: 16
                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                Layout.fillWidth: true
                                text: flow.selected && flow.phase !== "idle" ? flow.selected.name : qsTr("CONNECT TO YOUR DESK")
                                textFormat: Text.PlainText
                                color: theme.muted
                                font.family: theme.mono
                                font.pixelSize: 10
                                font.letterSpacing: 1
                                elide: Text.ElideRight
                            }
                            Rectangle {
                                width: 6
                                height: 6
                                visible: flow.selected !== null
                                color: flow.phase === "blocked" || flow.phase === "interrupted" ? theme.azure : flow.selected && flow.selected.status === "ready" ? theme.available : theme.quiet
                            }
                            Label {
                                visible: flow.selected !== null
                                text: flow.phase === "idle" && flow.selected ? page.statusText(flow.selected.status).toUpperCase() : flow.phase.toUpperCase()
                                color: theme.muted
                                font.family: theme.mono
                                font.pixelSize: 10
                            }
                        }
                        Label {
                            objectName: "flowHeading"
                            Layout.fillWidth: true
                            text: page.heading()
                            textFormat: Text.PlainText
                            color: theme.text
                            font.family: theme.display
                            font.pixelSize: page.compact ? 32 : 44
                            font.letterSpacing: -1
                            wrapMode: Text.WordWrap
                            Accessible.role: Accessible.Heading
                        }
                        Label {
                            objectName: "flowDescription"
                            Layout.fillWidth: true
                            text: page.description()
                            textFormat: Text.PlainText
                            color: theme.muted
                            font.family: theme.sans
                            font.pixelSize: 14
                            lineHeight: 1.35
                            wrapMode: Text.WordWrap
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: profileColumn.implicitHeight + 40
                            color: theme.panel
                            visible: flow.selected !== null
                            ColumnLayout {
                                id: profileColumn
                                anchors.fill: parent
                                anchors.margins: 20
                                spacing: 12
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label {
                                        text: qsTr("DISPLAY LAYOUT")
                                        color: theme.text
                                        font.family: theme.sans
                                        font.pixelSize: 12
                                        font.letterSpacing: 1.2
                                    }
                                    Item {
                                        Layout.fillWidth: true
                                    }
                                    Label {
                                        text: qsTr("4K / 60 FPS TARGET")
                                        color: theme.muted
                                        font.family: theme.mono
                                        font.pixelSize: 10
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12
                                    Repeater {
                                        model: 2
                                        TeraguchiDisplayChoice {
                                            required property int index
                                            objectName: "display-" + (index + 1)
                                            displayCount: index + 1
                                            Layout.fillWidth: true
                                            selected: flow.displayCount === index + 1
                                            enabled: flow.canChoose
                                            onClicked: flow.chooseDisplays(index + 1)
                                        }
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Required: native 10-bit · 4:4:4 · Hardware decode")
                                    color: theme.text
                                    font.family: theme.sans
                                    font.pixelSize: 12
                                    wrapMode: Text.WordWrap
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Every selected display must pass the connection check.")
                                    color: theme.muted
                                    font.family: theme.sans
                                    font.pixelSize: 12
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                        ToolButton {
                            objectName: "detailsButton"
                            text: page.detailsOpen ? qsTr("Hide connection details −") : qsTr("Connection details +")
                            visible: flow.selected !== null
                            implicitHeight: 44
                            focusPolicy: Qt.StrongFocus
                            background: Rectangle {
                                color: parent.hovered ? theme.raised : "transparent"
                                border.width: parent.activeFocus ? 2 : 0
                                border.color: theme.accent
                            }
                            contentItem: Text {
                                text: parent.text
                                color: theme.muted
                                font.family: theme.sans
                                font.pixelSize: 12
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: page.detailsOpen = !page.detailsOpen
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: page.detailsOpen && flow.selected !== null
                            text: qsTr("Requested: %1 display(s), up to 4K60 each.\nRequired: native 10-bit source, HEVC RExt 4:4:4 10-bit, hardware decoding.\nConnection checks do not certify physical display output or production readiness.").arg(flow.displayCount)
                            color: theme.muted
                            font.family: theme.sans
                            font.pixelSize: 12
                            lineHeight: 1.4
                            wrapMode: Text.WordWrap
                        }
                        Item {
                            Layout.preferredHeight: 4
                        }
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: theme.stroke
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    TeraguchiButton {
                        objectName: "connectButton"
                        visible: !flow.busy && !flow.sessionOpen
                        text: flow.phase === "blocked" ? qsTr("Check again") : qsTr("Connect")
                        primary: true
                        enabled: flow.canConnect
                        onClicked: flow.begin(false)
                    }
                    TeraguchiButton {
                        objectName: "cancelButton"
                        visible: flow.busy
                        text: qsTr("Cancel")
                        onClicked: flow.cancel()
                    }
                    TeraguchiButton {
                        objectName: "reconnectButton"
                        visible: flow.phase === "interrupted" || (flow.phase === "blocked" && flow.resumeAttempt)
                        text: qsTr("Reconnect")
                        primary: true
                        enabled: flow.selected !== null && flow.selected.status === "ready"
                        onClicked: flow.begin(true)
                    }
                    TeraguchiButton {
                        objectName: "disconnectButton"
                        visible: flow.sessionOpen && !flow.busy
                        text: flow.phase === "connected" ? qsTr("Disconnect") : qsTr("Return to workstations")
                        onClicked: flow.disconnect()
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    BusyIndicator {
                        implicitWidth: 32
                        implicitHeight: 32
                        running: flow.busy
                        visible: running
                    }
                }
                Label {
                    Layout.fillWidth: true
                    visible: flow.phase === "connected"
                    text: qsTr("Disconnect closes this connection. Logging out inside Rocky ends your desktop session.")
                    color: theme.muted
                    font.family: theme.sans
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("Built on Alan Latteri's PLANK.")
                color: theme.quiet
                font.family: theme.sans
                font.pixelSize: 11
            }
            Item {
                Layout.fillWidth: true
            }
            Label {
                text: qsTr("TERAGUCHI / DXS")
                color: theme.muted
                font.family: theme.mono
                font.pixelSize: 10
                font.letterSpacing: 1
            }
        }
    }
}
