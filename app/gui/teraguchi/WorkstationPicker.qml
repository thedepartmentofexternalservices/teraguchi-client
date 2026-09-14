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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 30
        spacing: 24
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Rectangle {
                width: 33
                height: 33
                radius: 6
                color: theme.accent
                Text {
                    anchors.centerIn: parent
                    text: "t"
                    font.pixelSize: 28
                    font.weight: Font.Bold
                    color: theme.accentInk
                }
            }
            Label {
                text: "teraguchi"
                color: theme.text
                font.pixelSize: 24
                font.weight: Font.DemiBold
            }
            Item {
                Layout.fillWidth: true
            }
            Label {
                text: qsTr("YOUR REMOTE WORKSPACE")
                color: theme.quiet
                font.pixelSize: 10
                font.letterSpacing: 1.5
            }
        }
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: theme.stroke
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 28
            ColumnLayout {
                Layout.preferredWidth: 270
                Layout.fillHeight: true
                spacing: 12
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: qsTr("Workstations")
                        color: theme.text
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    Label {
                        text: flow.workstations.length
                        color: theme.quiet
                        font.pixelSize: 13
                    }
                }
                Label {
                    text: qsTr("Assigned to you")
                    color: theme.muted
                    font.pixelSize: 12
                }
                ListView {
                    id: list
                    objectName: "workstationList"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 8
                    model: flow.workstations
                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                        active: true
                    }
                    delegate: ItemDelegate {
                        id: row
                        required property var modelData
                        objectName: "host-" + modelData.id
                        width: ListView.view.width
                        height: 76
                        enabled: flow.canChoose
                        hoverEnabled: true
                        focusPolicy: Qt.StrongFocus
                        Accessible.name: modelData.name + ", " + page.statusText(modelData.status)
                        onClicked: flow.selectWorkstation(modelData.id)
                        background: Rectangle {
                            radius: theme.radius
                            color: row.hovered ? theme.hover : flow.selectedId === row.modelData.id ? theme.raised : theme.panel
                            border.width: row.activeFocus || flow.selectedId === row.modelData.id ? 1 : 0
                            border.color: theme.accent
                        }
                        contentItem: RowLayout {
                            spacing: 12
                            Rectangle {
                                width: 7
                                height: 7
                                radius: 4
                                color: row.modelData.status === "ready" ? theme.accent : row.modelData.status === "incompatible" ? theme.warning : theme.quiet
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Label {
                                    Layout.fillWidth: true
                                    text: row.modelData.name
                                    textFormat: Text.PlainText
                                    elide: Text.ElideRight
                                    color: theme.text
                                    font.pixelSize: 15
                                    font.weight: Font.Medium
                                }
                                Label {
                                    text: page.statusText(row.modelData.status)
                                    color: theme.muted
                                    font.pixelSize: 12
                                }
                            }
                            Label {
                                text: "›"
                                color: flow.selectedId === row.modelData.id ? theme.accent : theme.quiet
                                font.pixelSize: 22
                            }
                        }
                    }
                    Label {
                        anchors.fill: parent
                        visible: flow.workstations.length === 0
                        text: qsTr("No workstations assigned yet.")
                        wrapMode: Text.WordWrap
                        color: theme.muted
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
                spacing: 14
                ScrollView {
                    id: contentScroll
                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                        active: true
                    }
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentWidth: availableWidth
                    ColumnLayout {
                        width: contentScroll.availableWidth
                        spacing: 22
                        Label {
                            Layout.fillWidth: true
                            text: flow.selected && flow.phase !== "idle" ? flow.selected.name : qsTr("CONNECT TO YOUR DESK")
                            textFormat: Text.PlainText
                            color: theme.muted
                            font.pixelSize: 11
                            font.letterSpacing: 1.2
                        }
                        Label {
                            objectName: "flowHeading"
                            Layout.fillWidth: true
                            text: page.heading()
                            textFormat: Text.PlainText
                            color: theme.text
                            font.pixelSize: 29
                            font.weight: Font.DemiBold
                            wrapMode: Text.WordWrap
                            Accessible.role: Accessible.Heading
                        }
                        Label {
                            objectName: "flowDescription"
                            Layout.fillWidth: true
                            text: page.description()
                            textFormat: Text.PlainText
                            color: theme.muted
                            font.pixelSize: 14
                            lineHeight: 1.35
                            wrapMode: Text.WordWrap
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: profileColumn.implicitHeight + 40
                            radius: theme.radius
                            color: theme.panel
                            visible: flow.selected !== null
                            ColumnLayout {
                                id: profileColumn
                                anchors.fill: parent
                                anchors.margins: 20
                                spacing: 18
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label {
                                        text: qsTr("Your display layout")
                                        color: theme.text
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                    }
                                    Item {
                                        Layout.fillWidth: true
                                    }
                                    Label {
                                        text: qsTr("4K / 60 fps target")
                                        color: theme.muted
                                        font.pixelSize: 11
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Repeater {
                                        model: 2
                                        TeraguchiButton {
                                            required property int index
                                            objectName: "display-" + (index + 1)
                                            text: index === 0 ? qsTr("1 display") : qsTr("2 displays")
                                            Layout.fillWidth: true
                                            primary: flow.displayCount === index + 1
                                            selected: flow.displayCount === index + 1
                                            enabled: flow.canChoose
                                            onClicked: flow.chooseDisplays(index + 1)
                                            Accessible.description: flow.displayCount === index + 1 ? qsTr("Selected") : qsTr("Not selected")
                                        }
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Required: native 10-bit · 4:4:4 · Hardware decode")
                                    color: theme.text
                                    font.pixelSize: 12
                                    wrapMode: Text.WordWrap
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Every selected display must pass the connection check.")
                                    color: theme.muted
                                    font.pixelSize: 12
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                        ToolButton {
                            objectName: "detailsButton"
                            text: page.detailsOpen ? qsTr("Hide connection details −") : qsTr("Connection details +")
                            visible: flow.selected !== null
                            focusPolicy: Qt.StrongFocus
                            background: Rectangle {
                                radius: 4
                                color: parent.hovered ? theme.raised : "transparent"
                                border.width: parent.activeFocus ? 1 : 0
                                border.color: theme.accent
                            }
                            contentItem: Text {
                                text: parent.text
                                color: theme.muted
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
                            font.pixelSize: 12
                            lineHeight: 1.4
                            wrapMode: Text.WordWrap
                        }
                        Item {
                            Layout.preferredHeight: 4
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
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
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }
        }
        Label {
            text: qsTr("Built on Alan Latteri's PLANK.  Teraguchi by DXS.")
            color: theme.quiet
            font.pixelSize: 11
        }
    }
}
