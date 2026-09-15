import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import ComputerManager 1.0
import ComputerModel 1.0
import TailscaleWorkstations 1.0
import MacInputPermissions 1.0

ApplicationWindow {
    id: window
    title: qsTr("Teraguchi — Development")
    width: 1100
    height: 720
    minimumWidth: 720
    minimumHeight: 560
    visible: true
    property bool quitting: false
    property bool polling: false
    function updatePolling() {
        var needed = visible && !sessionRuntime.executing && !quitting;
        if (needed === polling) return;
        polling = needed;
        if (polling) ComputerManager.startPolling();
        else ComputerManager.stopPollingAsync();
    }
    function closeSafely() {
        quitting = true;
        if (workstationFlow.busy) workstationFlow.cancel();
        if (workstationFlow.sessionOpen) workstationFlow.disconnect();
        loginBridge.cancel();
        sessionRuntime.stop();
        updatePolling();
        if (!sessionRuntime.pending) Qt.quit();
    }
    onClosing: function(close) {
        if (sessionRuntime.pending) {
            close.accepted = false;
            closeSafely();
        } else {
            // Accept the original close/Quit event once no native Session owns
            // resources. Calling Qt.quit() while rejecting it would loop.
            quitting = true;
            if (workstationFlow.busy) workstationFlow.cancel();
            loginBridge.cancel();
            updatePolling();
            close.accepted = true;
        }
    }
    onVisibleChanged: updatePolling()
    onActiveChanged: if (active && !quitting) permissionGate.refresh()
    Component.onCompleted: {
        computerCatalog.initialize(ComputerManager);
        permissionGate.refresh();
        updatePolling();
        workstationFlow.refresh();
    }
    WorkstationFlow { id: workstationFlow }
    TailscaleWorkstations { id: tailscaleProvider; studioDnsSuffix: studioDnsSuffixConfiguration }
    TailscaleAssignments { id: assignmentBridge; flow: workstationFlow; provider: tailscaleProvider }
    ComputerModel { id: computerCatalog }
    MacInputPermissions { id: inputPermissions }
    MacPermissionsGate { id: permissionGate; flow: workstationFlow; provider: inputPermissions }
    Timer {
        interval: 2000
        repeat: true
        running: window.active && !window.quitting && !sessionRuntime.executing
        onTriggered: permissionGate.refresh()
    }
    TailscaleLogin {
        id: loginBridge
        assignments: assignmentBridge
        computers: computerCatalog
        onSessionPrepared: function(token, session) { sessionRuntime.prepare(token, session); }
        onTokenChanged: if (token < 0) permissionGate.refresh()
    }
    WorkstationSession {
        id: sessionRuntime
        flow: workstationFlow
        presentationWindow: window
        onExecutingChanged: window.updatePolling()
        onPresentationStarted: window.hide()
        onReleased: {
            if (window.quitting) Qt.quit();
            else { permissionGate.refresh(); window.show(); window.raise(); workstationFlow.refresh(); }
            gc();
        }
    }
    Timer {
        interval: 20000
        repeat: true
        running: !window.quitting && !sessionRuntime.pending
        onTriggered: if (!workstationFlow.busy) workstationFlow.refresh()
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Label {
            Layout.fillWidth: true
            Layout.margins: 12
            visible: ["ready", "refreshing"].indexOf(tailscaleProvider.state) < 0
            wrapMode: Text.Wrap
            text: {
                switch (tailscaleProvider.state) {
                case "configuration-needed": return qsTr("Studio setup is needed. Ask your administrator for the configured client launcher.");
                case "missing": return qsTr("Install Tailscale, then accept your studio's workstation invitation.");
                case "needs-login": return qsTr("Sign in to Tailscale with the account that accepted your workstation invitation.");
                case "needs-approval": return qsTr("Your Tailscale device is waiting for approval.");
                case "stopped": return qsTr("Connect Tailscale, then refresh your workstations.");
                case "no-shared-workstations": return qsTr("No shared studio workstations are visible. Accept your invitation in Tailscale, then refresh.");
                default: return qsTr("Tailscale status is unavailable. Check Tailscale, then refresh.");
                }
            }
        }
        MacPermissionsPanel {
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            permissions: inputPermissions
            onReviewRequested: permissionDialog.open()
        }
        WorkstationPicker { Layout.fillWidth: true; Layout.fillHeight: true; flow: workstationFlow }
        Label {
            Layout.margins: 12
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: sessionRuntime.pending && !workstationFlow.busy && !workstationFlow.sessionOpen ? qsTr("Session cleanup must finish before another connection can start.") : qsTr("One display uses the screen containing this window. Move this window to choose a different screen. Two-display Mac support is still in development.")
        }
    }
    MacPermissionsDialog { id: permissionDialog; permissions: inputPermissions }
    AssignedLoginDialog { flow: workstationFlow; login: loginBridge }
}
