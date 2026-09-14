import QtQuick 2.15

// Presentation state only. A trusted adapter must supply authenticated results;
// catalog availability never authorizes a session. No discovery or transport here.
QtObject {
    id: flow
    property var workstations: []
    property string selectedId: ""
    property int displayCount: 1
    property string phase: "idle"
    property string problem: ""
    property string problemTitle: ""
    property int generation: 0
    property string attemptId: ""
    property int attemptDisplays: 0
    property bool resumeAttempt: false
    property bool retainsSession: false
    readonly property var selected: findWorkstation(selectedId)
    readonly property bool busy: phase === "checking" || phase === "connecting"
    readonly property bool sessionOpen: retainsSession
    readonly property bool canConnect: selected !== null && selected.status === "ready" && !busy && !sessionOpen
    readonly property bool canChoose: !busy && !sessionOpen

    signal checkRequested(int token, string workstationId, int displays, bool resume)
    signal connectionRequested(int token, string workstationId, int displays, bool resume)
    signal cancelRequested(int token)
    signal disconnectRequested(string workstationId)
    signal refreshRequested(string workstationId)

    function findWorkstation(id) {
        for (var i = 0; i < workstations.length; ++i) {
            if (workstations[i].id === id)
                return workstations[i];
        }
        return null;
    }

    function setWorkstations(entries) {
        // Consume only an assigned catalog. Unknown/malformed entries stay hidden.
        if (!Array.isArray(entries))
            entries = [];
        var accepted = [];
        var seen = [];
        for (var i = 0; i < entries.length; ++i) {
            var item = entries[i];
            if (!item || item.assigned !== true || typeof item.id !== "string" || !/^[a-zA-Z0-9_-]{1,128}$/.test(item.id) || seen.indexOf(item.id) >= 0 || typeof item.name !== "string")
                continue;
            seen.push(item.id);
            accepted.push({
                id: item.id,
                name: item.name,
                assigned: true,
                status: ["ready", "offline", "occupied", "incompatible"].indexOf(item.status) >= 0 ? item.status : "unknown"
            });
        }
        var hadSession = sessionOpen;
        var wasBusy = busy;
        var oldId = selectedId;
        var oldToken = generation;
        var nextSelected = null;
        for (var j = 0; j < accepted.length; ++j) {
            if (accepted[j].id === oldId)
                nextSelected = accepted[j];
        }
        var removed = oldId !== "" && nextSelected === null;
        var unavailable = wasBusy && nextSelected !== null && nextSelected.status !== "ready";
        // Retire callbacks before publishing the catalog or emitting cancellation.
        // QML signal handlers may call the completion API synchronously.
        if (removed || unavailable)
            ++generation;
        workstations = accepted;
        if (removed) {
            resumeAttempt = false;
            retainsSession = false;
            selectedId = "";
            block(qsTr("Workstation no longer assigned"), qsTr("Ask your studio administrator to check your workstation assignment."));
            if (wasBusy)
                cancelRequested(oldToken);
            if (hadSession)
                disconnectRequested(oldId);
        } else if (unavailable) {
            block(qsTr("Availability changed"), qsTr("Check the workstation status before trying again."));
            cancelRequested(oldToken);
        }
    }

    function selectWorkstation(id) {
        if (!canChoose || !findWorkstation(id))
            return false;
        ++generation;
        selectedId = id;
        phase = "idle";
        resumeAttempt = false;
        problem = "";
        problemTitle = "";
        return true;
    }

    function chooseDisplays(count) {
        if (!canChoose || (count !== 1 && count !== 2))
            return false;
        displayCount = count;
        // Changing a request never turns a failed check into a passed check.
        return true;
    }

    function block(title, detail) {
        phase = "blocked";
        problemTitle = title;
        problem = detail;
    }

    function begin(resume) {
        if (resume) {
            if (!retainsSession || !(phase === "interrupted" || (phase === "blocked" && resumeAttempt)) || !selected || selected.status !== "ready")
                return false;
        } else if (!canConnect)
            return false;
        resumeAttempt = resume === true;
        attemptId = selectedId;
        attemptDisplays = displayCount;
        problem = "";
        problemTitle = "";
        phase = "checking";
        ++generation;
        checkRequested(generation, attemptId, attemptDisplays, resumeAttempt);
        return true;
    }

    function cancel() {
        if (!busy)
            return false;
        var oldToken = generation;
        ++generation;
        phase = resumeAttempt ? "interrupted" : "idle";
        cancelRequested(oldToken);
        return true;
    }

    function acceptCheck(token, result) {
        if (token !== generation || phase !== "checking")
            return false;
        if (!result || result.outcome !== "pass") {
            var reason = result ? result.outcome : "unknown";
            if (reason === "occupied")
                block(qsTr("Workstation is in use"), qsTr("Another artist is connected. Try again when the workstation is available."));
            else if (reason === "permissions")
                block(qsTr("Mac permissions needed"), qsTr("Enable Accessibility and Input Monitoring for the client, then check again."));
            else if (reason === "offline")
                block(qsTr("Workstation is offline"), qsTr("Check your network connection. If it stays offline, contact your studio administrator."));
            else
                block(qsTr("Connection check failed"), qsTr("The workstation could not confirm a compatible session. Check again or contact your studio administrator."));
            return false;
        }
        // These are adapter attestations, not facts inferred from a bookmark.
        if (result.authorized !== true || result.seatAvailable !== true) {
            block(qsTr("Access could not be confirmed"), qsTr("Sign in through the workstation's trusted login flow, then check again."));
            return false;
        }
        if (result.displays !== attemptDisplays) {
            block(qsTr("Selected displays unavailable"), qsTr("The session cannot provide every selected display. Reconnect the missing display or explicitly choose a different layout."));
            return false;
        }
        if (result.nativeSourceDepth !== 10 || result.profile !== "hevc-rext-444-10" || result.hardwareDecode !== true) {
            block(qsTr("Picture requirements not met"), qsTr("This session needs native 10-bit capture, HEVC 4:4:4 10-bit, and hardware decoding. Ask your studio administrator to check compatibility."));
            return false;
        }
        phase = "connecting";
        connectionRequested(token, attemptId, attemptDisplays, resumeAttempt);
        return true;
    }

    function acceptConnection(token, success) {
        if (token !== generation || phase !== "connecting")
            return false;
        if (success !== true) {
            block(qsTr("Couldn't open the workstation"), qsTr("The session did not start. Check again; your selected display layout is unchanged."));
            return false;
        }
        retainsSession = true;
        phase = "connected";
        return true;
    }

    function interrupted(token) {
        if (token !== generation || phase !== "connected")
            return false;
        phase = "interrupted";
        return true;
    }

    function disconnect() {
        if (!sessionOpen)
            return false;
        var id = attemptId;
        var oldToken = generation;
        var wasBusy = busy;
        ++generation;
        phase = "idle";
        resumeAttempt = false;
        problem = "";
        problemTitle = "";
        retainsSession = false;
        if (wasBusy)
            cancelRequested(oldToken);
        disconnectRequested(id);
        return true;
    }

    function refresh() {
        if (busy || sessionOpen)
            return false;
        refreshRequested(selectedId);
        return true;
    }
}
