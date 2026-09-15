import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts 1.15

Rectangle {
    id: page
    required property WorkstationFlow flow
    property bool detailsOpen: false
    property bool powerDetailsOpen: false
    property string searchText: ""
    readonly property var visibleWorkstations: flow.workstations.filter(function (host) {
        return host.name.toLowerCase().indexOf(searchText.trim().toLowerCase()) >= 0;
    })
    function chooseAdjacent(delta) {
        if (!flow.canChoose || (powerEnabled && studioPower.pending))
            return;
        var index = visibleWorkstations.findIndex(function (host) {
            return host.id === flow.selectedId;
        });
        var next = Math.max(0, Math.min(visibleWorkstations.length - 1, index + delta));
        if (visibleWorkstations.length)
            flow.selectWorkstation(visibleWorkstations[next].id);
    }
    // Optional managed studio adapter. Absent in a standard installation.
    // It supplies presentation reports; server-side authorization still decides.
    property QtObject studioPower: null
    readonly property bool powerEnabled: studioPower !== null && studioPower.enabled === true
    readonly property var powerReport: reportFor(flow.selectedId)
    readonly property bool powerVisible: powerEnabled && flow.selected !== null && flow.selected.status === "offline" && !flow.sessionOpen && !flow.busy
    readonly property bool canRequestPower: powerVisible && flow.catalogFresh && !flow.catalogRefreshing && studioPower.pending !== true && powerReport.fresh === true && powerReport.startAllowed === true && ["off", "standby"].indexOf(powerReport.state) >= 0

    function reportFor(id) {
        // Reading revision makes adapter report updates observable in QML.
        var revision = studioPower ? studioPower.revision : 0;
        if (!powerEnabled || !id)
            return ({});
        var report = studioPower.report(id);
        return report && report.workstationId === id ? report : ({});
    }
    function workstationStatus(host) {
        if (!flow.catalogFresh)
            return qsTr("Needs refresh");
        return powerEnabled && host.status === "offline" ? powerLabel(reportFor(host.id)) : statusText(host.status);
    }
    function powerLabel(report) {
        if (report.fresh !== true)
            return qsTr("Power status unavailable");
        if (report.state === "off")
            return qsTr("Powered off");
        if (report.state === "standby")
            return qsTr("Ready to power on");
        if (report.state === "starting")
            return qsTr("Starting");
        if (report.state === "online")
            return qsTr("Powered on");
        return qsTr("Power needs checking");
    }
    function requestPower() {
        // Recheck the selected stable ID and current presentation policy at click.
        if (flow.catalogIsCurrent() && canRequestPower)
            studioPower.requestStart(flow.selectedId);
    }
    function powerDescription() {
        if (studioPower.pending === true)
            return qsTr("The start request is being tracked by your studio. You can connect when the workstation becomes available.");
        if (powerReport.fresh !== true || powerReport.state === "unavailable")
            return qsTr("Your studio's power service could not verify the workstation. Check status again or contact studio support.");
        if (powerReport.state === "off" || powerReport.state === "standby")
            return powerReport.startAllowed === true ? qsTr("Power on this workstation, then connect when it becomes available.") : qsTr("This workstation is off. Ask your studio administrator for permission to power it on.");
        if (powerReport.state === "starting")
            return qsTr("The workstation is starting. Wait for it to become available before connecting.");
        return powerReport.outlet === "on" ? qsTr("The outlet has power, but the workstation is not responding. It may be starting or have a connection problem. Contact studio support if it stays unavailable.") : qsTr("The workstation's power state is unknown. Check status again or contact studio support.");
    }
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
        if (!flow.sessionOpen && !flow.catalogFresh)
            return flow.catalogRefreshing ? qsTr("Refreshing workstations") : qsTr("Refresh your assignments");
        if (powerVisible)
            return flow.selected.name;
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
        if (flow.phase !== "connected" && !flow.catalogFresh)
            return flow.catalogRefreshing ? qsTr("Checking your current workstation assignments.") : flow.catalogProblem || qsTr("Refresh before connecting. Your previous workstation list is shown until it can be checked.");
        if (powerVisible)
            return powerDescription();
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

    readonly property bool compact: width < 850
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 48
            color: theme.toolbar
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12
                Label {
                    text: "Teraguchi"
                    font.family: theme.sans
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    color: theme.text
                }
                Label {
                    text: qsTr("Workstations")
                    font.pixelSize: 13
                    color: theme.muted
                }
                Item {
                    Layout.fillWidth: true
                }
                TeraguchiButton {
                    objectName: "refreshButton"
                    text: flow.catalogRefreshing ? qsTr("Refreshing…") : qsTr("Refresh")
                    enabled: !flow.catalogRefreshing && !flow.busy && !(page.powerEnabled && page.studioPower.pending)
                    onClicked: {
                        flow.refresh();
                        if (page.powerEnabled && flow.selected && !page.studioPower.pending)
                            page.studioPower.requestRefresh(flow.selectedId);
                    }
                    Accessible.description: qsTr("Refresh workstation availability")
                }
            }
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: theme.stroke
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            Rectangle {
                Layout.preferredWidth: page.compact ? 212 : 236
                Layout.fillHeight: true
                color: theme.sidebar
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12
                    TextField {
                        id: search
                        objectName: "workstationSearch"
                        Layout.fillWidth: true
                        placeholderText: qsTr("Search workstations")
                        font.pixelSize: 13
                        text: page.searchText
                        onTextEdited: page.searchText = text
                        Accessible.name: qsTr("Search assigned workstations")
                    }
                    Label {
                        Layout.leftMargin: 8
                        text: qsTr("Assigned to you")
                        color: theme.muted
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }
                    ListView {
                        id: list
                        objectName: "workstationList"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 2
                        model: page.visibleWorkstations
                        currentIndex: page.visibleWorkstations.findIndex(function (host) {
                            return host.id === flow.selectedId;
                        })
                        onCurrentIndexChanged: if (currentIndex >= 0)
                            positionViewAtIndex(currentIndex, ListView.Contain)
                        ScrollBar.vertical: TeraguchiScrollBar {}
                        delegate: Basic.ItemDelegate {
                            id: row
                            required property var modelData
                            readonly property bool chosen: flow.selectedId === modelData.id
                            objectName: "host-" + modelData.id
                            width: ListView.view.width
                            height: 48
                            padding: 8
                            enabled: flow.canChoose && !(page.powerEnabled && page.studioPower.pending)
                            hoverEnabled: true
                            focusPolicy: Qt.StrongFocus
                            Accessible.name: modelData.name + ", " + page.workstationStatus(modelData)
                            onClicked: flow.selectWorkstation(modelData.id)
                            Keys.onDownPressed: page.chooseAdjacent(1)
                            Keys.onUpPressed: page.chooseAdjacent(-1)
                            background: Rectangle {
                                radius: 6
                                color: row.chosen ? theme.accent : row.hovered ? theme.hover : "transparent"
                                border.width: row.activeFocus && !row.chosen ? 2 : 0
                                border.color: theme.accent
                            }
                            contentItem: RowLayout {
                                spacing: 8
                                TeraguchiComputerIcon {
                                    ink: row.chosen ? theme.selectedText : theme.muted
                                    Layout.preferredWidth: 24
                                    Layout.preferredHeight: 24
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3
                                    Label {
                                        Layout.fillWidth: true
                                        text: row.modelData.name
                                        textFormat: Text.PlainText
                                        elide: Text.ElideRight
                                        color: row.chosen ? theme.selectedText : theme.text
                                        font.pixelSize: 13
                                        font.weight: row.chosen ? Font.DemiBold : Font.Normal
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: page.workstationStatus(row.modelData)
                                        color: row.chosen ? theme.selectedText : theme.muted
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                        Label {
                            anchors.fill: parent
                            anchors.margins: 8
                            visible: list.count === 0
                            text: flow.workstations.length === 0 ? qsTr("No workstations assigned") : qsTr("No matching workstations")
                            wrapMode: Text.WordWrap
                            color: theme.muted
                            font.pixelSize: 12
                        }
                    }
                    Label {
                        Layout.leftMargin: 8
                        text: flow.workstations.length === 1 ? qsTr("1 workstation") : qsTr("%1 workstations").arg(flow.workstations.length)
                        color: theme.muted
                        font.pixelSize: 11
                    }
                    Label {
                        Layout.fillWidth: true
                        Layout.leftMargin: 8
                        text: qsTr("Built on Alan Latteri's PLANK")
                        color: theme.muted
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                }
            }
            Rectangle {
                width: 1
                Layout.fillHeight: true
                color: theme.stroke
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: page.compact ? 20 : 24
                spacing: 16
                RowLayout {
                    id: selectionHeader
                    Layout.fillWidth: true
                    spacing: 16
                    TeraguchiComputerIcon {
                        visible: flow.selected !== null
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        scale: 1.2
                        ink: theme.muted
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Label {
                            objectName: "flowHeading"
                            Layout.fillWidth: true
                            text: page.heading()
                            textFormat: Text.PlainText
                            color: theme.text
                            font.pixelSize: 20
                            font.weight: Font.DemiBold
                            wrapMode: Text.WordWrap
                            Accessible.role: Accessible.Heading
                        }
                        RowLayout {
                            visible: flow.selected !== null
                            spacing: 6
                            Rectangle {
                                width: 6
                                height: 6
                                radius: 3
                                color: flow.catalogFresh && flow.selected && flow.selected.status === "ready" ? theme.available : theme.muted
                            }
                            Label {
                                text: flow.selected ? page.workstationStatus(flow.selected) : ""
                                color: theme.muted
                                font.pixelSize: 12
                            }
                        }
                    }
                }
                Label {
                    objectName: "flowDescription"
                    Layout.fillWidth: true
                    text: page.description()
                    textFormat: Text.PlainText
                    color: theme.muted
                    font.pixelSize: 13
                    lineHeight: 1.3
                    wrapMode: Text.WordWrap
                }
                // Actions stay next to the selected workstation, outside the scroll area.
                RowLayout {
                    id: actionRow
                    Layout.fillWidth: true
                    spacing: 8
                    TeraguchiButton {
                        objectName: "connectButton"
                        visible: !flow.busy && !flow.sessionOpen && !page.powerVisible
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
                        enabled: flow.catalogFresh && !flow.catalogRefreshing && flow.selected !== null && flow.selected.status === "ready"
                        onClicked: flow.begin(true)
                    }
                    TeraguchiButton {
                        objectName: "disconnectButton"
                        visible: flow.sessionOpen && !flow.busy
                        text: qsTr("Disconnect")
                        onClicked: flow.disconnect()
                    }
                    TeraguchiButton {
                        objectName: "powerOnButton"
                        visible: page.powerVisible && page.canRequestPower
                        text: qsTr("Power on")
                        primary: true
                        enabled: page.canRequestPower
                        onClicked: page.requestPower()
                    }
                    TeraguchiButton {
                        objectName: "powerRefreshButton"
                        visible: page.powerVisible
                        text: qsTr("Check status")
                        enabled: page.powerEnabled && !page.studioPower.pending
                        onClicked: if (page.powerVisible && !page.studioPower.pending)
                            page.studioPower.requestRefresh(flow.selectedId)
                    }
                    // Pure-QML spinner avoids the optional Qt WebP image plugin.
                    Basic.BusyIndicator {
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                        running: flow.busy || (page.powerVisible && page.studioPower.pending)
                        visible: running
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                }
                ScrollView {
                    id: contentScroll
                    objectName: "connectionScroll"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentWidth: availableWidth
                    rightPadding: 12
                    ScrollBar.vertical: TeraguchiScrollBar {
                        objectName: "connectionScrollBar"
                        parent: contentScroll
                        x: contentScroll.width - width
                        y: contentScroll.topPadding
                        height: contentScroll.availableHeight
                    }
                    ColumnLayout {
                        width: contentScroll.availableWidth
                        spacing: 16
                        Rectangle {
                            objectName: "connectionSettings"
                            Layout.fillWidth: true
                            visible: flow.selected !== null && !page.powerVisible
                            color: theme.panel
                            radius: theme.radius
                            border.color: theme.stroke
                            implicitHeight: settings.implicitHeight + 32
                            ColumnLayout {
                                id: settings
                                anchors.fill: parent
                                anchors.margins: 16
                                spacing: 14
                                Label {
                                    text: qsTr("Connection")
                                    color: theme.text
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 16
                                    Label {
                                        text: qsTr("Displays")
                                        font.pixelSize: 13
                                        color: theme.muted
                                        Layout.preferredWidth: 76
                                    }
                                    Repeater {
                                        model: 2
                                        TeraguchiDisplayChoice {
                                            required property int index
                                            objectName: "display-" + (index + 1)
                                            displayCount: index + 1
                                            selected: flow.displayCount === index + 1
                                            enabled: flow.canChoose
                                            onClicked: flow.chooseDisplays(index + 1)
                                        }
                                    }
                                    Item {
                                        Layout.fillWidth: true
                                    }
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: theme.stroke
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 16
                                    Label {
                                        text: qsTr("Picture")
                                        font.pixelSize: 13
                                        color: theme.muted
                                        Layout.preferredWidth: 76
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 5
                                        Label {
                                            text: qsTr("Up to 4K at 60 fps per display")
                                            Layout.fillWidth: true
                                            wrapMode: Text.WordWrap
                                            font.pixelSize: 13
                                            color: theme.text
                                        }
                                        Label {
                                            text: qsTr("Native 10-bit · 4:4:4 · Hardware decoding")
                                            Layout.fillWidth: true
                                            wrapMode: Text.WordWrap
                                            font.pixelSize: 12
                                            color: theme.muted
                                        }
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Each selected display is checked before connecting.")
                                    font.pixelSize: 12
                                    color: theme.muted
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                        Rectangle {
                            objectName: "studioPowerPanel"
                            Layout.fillWidth: true
                            visible: page.powerVisible
                            color: theme.panel
                            radius: theme.radius
                            border.color: theme.stroke
                            implicitHeight: powerColumn.implicitHeight + 32
                            ColumnLayout {
                                id: powerColumn
                                anchors.fill: parent
                                anchors.margins: 16
                                spacing: 12
                                Label {
                                    text: qsTr("Studio power")
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                    color: theme.text
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: page.powerReport.state === "standby" && page.powerReport.fresh === true ? qsTr("Starting briefly cycles this workstation's power outlet. Your studio has verified the computer is off.") : qsTr("Managed by your studio. Powering on does not connect you automatically.")
                                    font.pixelSize: 12
                                    color: theme.muted
                                    wrapMode: Text.WordWrap
                                }
                                TeraguchiButton {
                                    objectName: "powerDetailsButton"
                                    text: page.powerDetailsOpen ? qsTr("Hide power details") : qsTr("Show power details")
                                    onClicked: page.powerDetailsOpen = !page.powerDetailsOpen
                                    Accessible.description: page.powerDetailsOpen ? qsTr("Power details expanded") : qsTr("Power details collapsed")
                                }
                                Row {
                                    Layout.fillWidth: true
                                    visible: page.powerDetailsOpen
                                    spacing: 32
                                    Column {
                                        width: 140
                                        spacing: 6
                                        visible: typeof page.powerReport.outlet === "string"
                                        Label {
                                            text: qsTr("Outlet")
                                            color: theme.muted
                                            font.pixelSize: 12
                                        }
                                        Label {
                                            objectName: "outletPowerLabel"
                                            text: page.powerReport.fresh !== true ? qsTr("Unknown") : page.powerReport.outlet === "on" ? qsTr("On") : page.powerReport.outlet === "off" ? qsTr("Off") : qsTr("Unknown")
                                            color: theme.text
                                            font.pixelSize: 13
                                        }
                                    }
                                    Column {
                                        width: 140
                                        spacing: 6
                                        Label {
                                            text: qsTr("Workstation")
                                            color: theme.muted
                                            font.pixelSize: 12
                                        }
                                        Label {
                                            objectName: "machinePowerLabel"
                                            text: page.powerReport.fresh !== true ? qsTr("Unknown") : ["off", "standby"].indexOf(page.powerReport.state) >= 0 ? qsTr("Off") : page.powerReport.state === "starting" ? qsTr("Starting") : page.powerReport.state === "online" ? qsTr("On") : qsTr("Unknown")
                                            color: theme.text
                                            font.pixelSize: 13
                                        }
                                    }
                                }
                            }
                        }
                        TeraguchiButton {
                            objectName: "detailsButton"
                            visible: flow.selected !== null && !page.powerVisible
                            text: page.detailsOpen ? qsTr("Hide connection details") : qsTr("Show connection details")
                            onClicked: page.detailsOpen = !page.detailsOpen
                            Accessible.description: page.detailsOpen ? qsTr("Connection details expanded") : qsTr("Connection details collapsed")
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: page.detailsOpen && flow.selected !== null && !page.powerVisible
                            text: qsTr("Requested: %1 display(s), up to 4K60 each.\nRequired: native 10-bit source, HEVC RExt 4:4:4 10-bit, hardware decoding.\nConnection checks do not certify physical display output or production readiness.").arg(flow.displayCount)
                            color: theme.muted
                            font.pixelSize: 12
                            lineHeight: 1.4
                            wrapMode: Text.WordWrap
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: flow.phase === "connected"
                            text: qsTr("Disconnect keeps your desktop and applications open. Logging out inside Rocky ends the desktop session.")
                            color: theme.muted
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                        Item {
                            Layout.preferredHeight: 4
                        }
                    }
                }
            }
        }
    }
}
