import QtQuick 2.15

// Reuses ComputerModel's native TLS/PAM and Session constructors. The parent
// owns the credential dialog and Session exec/deletion lifecycle.
QtObject {
    id: login
    required property TailscaleAssignments assignments
    required property QtObject computers
    property var presentationWindow: null
    property string displayToken: ""
    property var target: null
    property int token: -1
    property string requestId: ""
    property string preparingNodeId: ""
    property int preparationPolls: 0
    property Timer preparationTimer: Timer {
        interval: 250
        repeat: true
        onTriggered: {
            if (++login.preparationPolls >= 40) {
                login.assignments.flow.acceptCheck(login.token, {outcome: "host-not-ready"});
                login.cancel();
            } else login.finishPreparation();
        }
    }
    function permissionsReady() {
        if (computers.assignedInputPermissionsReady()) return true;
        assignments.flow.acceptCheck(token, {outcome: "permissions"});
        cancel();
        return false;
    }
    function finishPreparation() {
        if (!assignments.resolveLoginTarget(token, preparingNodeId)) { assignments.flow.invalidateCatalog(); cancel(); return; }
        var resolved = computers.assignedLoginTarget(assignments.provider, preparingNodeId);
        if (!resolved || !resolved.computerId || !resolved.hostId) return;
        target = resolved;
        target.displayToken = displayToken;
        preparingNodeId = "";
        preparationTimer.stop();
        if (!current()) { cancel(); return; }
        if (displaysReady() && permissionsReady()) credentialsRequested(token);
    }
    signal credentialsRequested(int token)
    signal sessionPrepared(int token, var session)

    function displaysReady() {
        if (computers.assignedDisplaysCurrent(displayToken)) return true;
        assignments.flow.block(qsTr("Selected displays unavailable"), qsTr("Your selected displays changed. Check their arrangement and start a new connection."));
        cancel();
        return false;
    }
    function current() {
        return target !== null && assignments.targetStillCurrent(token, target.id, target.address, target.identity);
    }
    function cancel() {
        // Retire both the callback and the request-scoped native result.
        // In-flight PAM may finish, but its credentials cannot be retained.
        if (requestId) computers.cancelAssignedAuthentication(requestId);
        if (displayToken) computers.cancelAssignedDisplays(displayToken);
        displayToken = "";
        preparationTimer.stop();
        preparingNodeId = "";
        target = null;
        requestId = "";
        token = -1;
    }
    function submit(expectedToken, username, password) {
        if (expectedToken !== token || !current() || requestId !== "")
            return false;
        if (!displaysReady() || !permissionsReady()) return false;
        requestId = computers.authenticateAssignedTarget(assignments.provider, target, username, password);
        if (!requestId) {
            assignments.flow.acceptCheck(token, {outcome: "unknown"});
            cancel();
            return false;
        }
        return true;
    }
    property Connections assignmentEvents: Connections {
        target: login.assignments
        function onLoginRequested(token, nodeId, address, identity, displays, resume) {
            login.cancel();
            var selection = login.computers.prepareAssignedDisplays(displays, login.presentationWindow);
            if (!selection.token) {
                login.assignments.flow.block(qsTr("Selected displays unavailable"), selection.error);
                return;
            }
            login.displayToken = selection.token;
            login.token = token;
            if (!login.permissionsReady()) return;
            login.preparingNodeId = nodeId;
            login.finishPreparation();
            if (login.preparingNodeId) {
                if (!login.computers.prepareAssignedTarget(login.assignments.provider, nodeId)) {
                    login.assignments.flow.acceptCheck(token, {outcome: "host-not-ready"});
                    login.cancel();
                    return;
                }
                login.preparationPolls = 0;
                login.preparationTimer.start();
            }
        }
    }
    property Connections flowEvents: Connections {
        target: login.assignments ? login.assignments.flow : null
        function onCancelRequested(token) { if (token === login.token) login.cancel(); }
        function onDisconnectRequested(workstationId) { login.cancel(); }
    }
    property Connections modelEvents: Connections {
        target: login.computers
        function onAssignedAuthenticationCompleted(requestId, computerId, error) {
            if (!login.requestId || requestId !== login.requestId || !login.target || computerId !== login.target.computerId)
                return;
            if (!login.current()) { login.assignments.flow.invalidateCatalog(); login.cancel(); return; }
            if (error !== undefined && error !== null) {
                login.assignments.flow.acceptCheck(login.token, {outcome: "unknown"});
                login.cancel();
                return;
            }
            if (!login.displaysReady() || !login.permissionsReady()) return;
            var session = login.computers.createAssignedSession(login.assignments.provider, login.target, login.assignments.flow.attemptDisplays, login.requestId);
            if (!session) {
                login.assignments.flow.acceptCheck(login.token, {outcome: "unknown"});
                login.cancel();
                return;
            }
            var token = login.token;
            login.cancel();
            // The existing Session must prove video, seat and live connection.
            // Preparing it does not manufacture a successful check/connection.
            login.sessionPrepared(token, session);
        }
    }
}
