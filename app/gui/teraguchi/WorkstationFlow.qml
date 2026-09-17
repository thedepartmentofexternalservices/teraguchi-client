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
    property string supportCode: "none"
    property int generation: 0
    property string attemptId: ""
    property int attemptDisplays: 0
    property bool resumeAttempt: false
    property bool retainsSession: false
    property bool runtimePending: false
    // Local cache freshness is independent of session authorization/lease state.
    property bool catalogFresh: false
    property bool catalogRefreshing: false
    property int catalogGeneration: 0
    property double catalogRequestedAt: 0
    property double catalogAcceptedAt: 0
    property double catalogExpiresAt: 0
    property string catalogProblem: ""
    property Timer catalogExpiry: Timer {
        onTriggered: flow.invalidateCatalog()
    }
    property Timer catalogTimeout: Timer {
        interval: 15000
        onTriggered: flow.rejectCatalog(flow.catalogGeneration)
    }
    readonly property var selected: findWorkstation(selectedId)
    readonly property bool busy: phase === "checking" || phase === "connecting"
    readonly property bool sessionOpen: retainsSession
    readonly property bool canConnect: catalogFresh && !catalogRefreshing && selected !== null && selected.status === "ready" && !busy && !sessionOpen && !runtimePending
    readonly property bool canChoose: !busy && !sessionOpen && !runtimePending

    signal checkRequested(int token, string workstationId, int displays, bool resume)
    signal connectionRequested(int token, string workstationId, int displays, bool resume)
    signal cancelRequested(int token)
    signal disconnectRequested(string workstationId)
    signal refreshRequested(int token)
    signal catalogCancelRequested(int token)

    function catalogIsCurrent() {
        var now = Date.now();
        if (catalogFresh && (now < catalogAcceptedAt || now >= catalogExpiresAt))
            invalidateCatalog();
        return catalogFresh && !catalogRefreshing;
    }

    function invalidateCatalog() {
        var oldCatalogToken = catalogGeneration;
        var wasRefreshing = catalogRefreshing;
        var oldToken = generation;
        // Native startup has its own background assignment guard. UI cache
        // expiry cannot cancel a still-authorized host display transition.
        var wasBusy = busy && !runtimePending;
        ++catalogGeneration;
        // Retire connection callbacks before any property notifications or signals.
        if (wasBusy)
            ++generation;
        catalogFresh = false;
        catalogRefreshing = false;
        catalogExpiry.stop();
        catalogTimeout.stop();
        if (wasBusy) {
            phase = resumeAttempt ? "interrupted" : "idle";
            cancelRequested(oldToken);
        }
        if (wasRefreshing)
            catalogCancelRequested(oldCatalogToken);
        // A failed refresh is not revocation. Keep an established session;
        // explicit removal in an accepted snapshot still disconnects it.
    }

    function acceptCatalog(token, entries, validityMs) {
        if (token !== catalogGeneration || !catalogRefreshing)
            return false;
        var now = Date.now();
        if (now < catalogRequestedAt || now - catalogRequestedAt >= catalogTimeout.interval || !Array.isArray(entries) || !validCatalogLifetime(validityMs))
            return rejectCatalog(token);
        setWorkstations(entries, validityMs);
        return true;
    }

    function rejectCatalog(token) {
        if (token !== catalogGeneration || !catalogRefreshing)
            return false;
        catalogProblem = qsTr("Couldn't refresh your assignments. Check your connection and try Refresh again.");
        invalidateCatalog();
        return false;
    }

    function validCatalogLifetime(value) {
        return typeof value === "number" && isFinite(value) && value > 0 && value <= 60000 && Math.floor(value) === value;
    }

    function findWorkstation(id) {
        for (var i = 0; i < workstations.length; ++i) {
            if (workstations[i].id === id)
                return workstations[i];
        }
        return null;
    }

    function setWorkstations(entries, validityMs) {
        // Trusted push snapshots and offline fixtures only. Async replies must
        // use acceptCatalog so an old request cannot restore revoked entries.
        // The adapter may shorten the local 60-second maximum cache lifetime.
        if (validityMs === undefined)
            validityMs = 60000;
        if (!Array.isArray(entries) || !validCatalogLifetime(validityMs)) {
            catalogProblem = qsTr("Assignments could not be verified. Try Refresh again.");
            invalidateCatalog();
            return false;
        }
        // Consume only an assigned catalog. Unknown/malformed entries stay hidden.
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
        var oldCatalogToken = catalogGeneration;
        var wasRefreshing = catalogRefreshing;
        ++catalogGeneration;
        catalogTimeout.stop();
        catalogRefreshing = false;
        catalogProblem = "";
        catalogAcceptedAt = Date.now();
        catalogExpiresAt = catalogAcceptedAt + validityMs;
        catalogExpiry.interval = validityMs;
        catalogExpiry.restart();
        workstations = accepted;
        catalogFresh = true;
        if (removed) {
            resumeAttempt = false;
            retainsSession = false;
            selectedId = "";
            block(qsTr("Workstation no longer assigned"), qsTr("Ask your studio administrator to check your workstation assignment."), "assignment");
            if (wasBusy)
                cancelRequested(oldToken);
            if (hadSession)
                disconnectRequested(oldId);
        } else if (unavailable) {
            block(qsTr("Availability changed"), qsTr("Check the workstation status before trying again."), "workstation");
            cancelRequested(oldToken);
        }
        if (wasRefreshing)
            catalogCancelRequested(oldCatalogToken);
        return true;
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
        supportCode = "none";
        return true;
    }

    function chooseDisplays(count) {
        if (!canChoose || (count !== 1 && count !== 2))
            return false;
        displayCount = count;
        // Changing a request never turns a failed check into a passed check.
        return true;
    }

    function block(title, detail, code) {
        phase = "blocked";
        problemTitle = title;
        problem = detail;
        supportCode = ["assignment", "permissions", "displays", "tablet", "trust", "seat", "workstation", "login", "video", "connection"].indexOf(code) >= 0 ? code : "unknown";
    }

    function begin(resume) {
        if (!catalogIsCurrent())
            return false;
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
        supportCode = "none";
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
        if (!catalogIsCurrent() || token !== generation || phase !== "checking")
            return false;
        if (!result || result.outcome !== "pass") {
            var reason = result ? result.outcome : "unknown";
            if (reason === "occupied")
                block(qsTr("Workstation is in use"), qsTr("Another artist is connected. Try again when the workstation is available."), "seat");
            else if (reason === "permissions")
                block(qsTr("Mac permissions needed"), qsTr("Enable Accessibility and Input Monitoring for the client, then check again."), "permissions");
            else if (reason === "host-not-ready")
                block(qsTr("Workstation needs checking"), qsTr("The shared machine did not confirm a compatible workstation. Contact your studio administrator."), "workstation");
            else if (reason === "offline")
                block(qsTr("Workstation is offline"), qsTr("Check your network connection. If it stays offline, contact your studio administrator."), "workstation");
            else
                block(qsTr("Connection check failed"), qsTr("The workstation could not confirm a compatible session. Check again or contact your studio administrator."), "connection");
            return false;
        }
        // These are adapter attestations, not facts inferred from a bookmark.
        if (result.authorized !== true || result.seatAvailable !== true) {
            block(qsTr("Access could not be confirmed"), qsTr("Sign in through the workstation's trusted login flow, then check again."), "login");
            return false;
        }
        if (result.displays !== attemptDisplays) {
            block(qsTr("Selected displays unavailable"), qsTr("The session cannot provide every selected display. Reconnect the missing display or explicitly choose a different layout."), "displays");
            return false;
        }
        if (result.nativeSourceDepth !== 10 || result.profile !== "hevc-rext-444-10" || result.hardwareDecode !== true) {
            block(qsTr("Picture requirements not met"), qsTr("This session needs native 10-bit capture, HEVC 4:4:4 10-bit, and hardware decoding. Ask your studio administrator to check compatibility."), "video");
            return false;
        }
        phase = "connecting";
        connectionRequested(token, attemptId, attemptDisplays, resumeAttempt);
        return true;
    }

    // The production native Session performs admission inside exec(). Moving
    // into startup does not attest to video, a free seat, or a live connection.
    function beginNativeSession(token) {
        if (token !== generation || phase !== "checking" || !catalogIsCurrent() || runtimePending)
            return false;
        runtimePending = true;
        phase = "connecting";
        return true;
    }

    function acceptNativeConnection(token) {
        if (token !== generation || phase !== "connecting" || !runtimePending)
            return false;
        retainsSession = true;
        phase = "connected";
        return true;
    }

    function acceptConnection(token, success) {
        if (!catalogIsCurrent() || token !== generation || phase !== "connecting")
            return false;
        if (success !== true) {
            block(qsTr("Couldn't open the workstation"), qsTr("The session did not start. Check again; your selected display layout is unchanged."), "connection");
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
        supportCode = "none";
        retainsSession = false;
        if (wasBusy)
            cancelRequested(oldToken);
        disconnectRequested(id);
        return true;
    }

    function refresh() {
        if (busy || catalogRefreshing)
            return false;
        invalidateCatalog();
        catalogProblem = "";
        catalogRequestedAt = Date.now();
        catalogRefreshing = true;
        catalogTimeout.restart();
        refreshRequested(catalogGeneration);
        return true;
    }
}
