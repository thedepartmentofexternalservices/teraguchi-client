import QtQuick 2.15

// The native provider reads only local Tailscale state. A session adapter still
// owns host identity, PAM, strict-video admission, and session lifetime.
QtObject {
    id: assignments
    required property WorkstationFlow flow
    required property QtObject provider
    signal loginRequested(int token, string nodeId, string address, string identity, int displays, bool resume)

    function resolveLoginTarget(token, nodeId) {
        if (token !== flow.generation || flow.phase !== "checking" || !flow.catalogIsCurrent())
            return null;
        var target = provider.resolve(nodeId);
        if (!target || target.id !== nodeId || target.status !== "ready" || !target.address || !target.identity)
            return null;
        return target;
    }
    function targetStillCurrent(token, nodeId, address, identity) {
        var target = resolveLoginTarget(token, nodeId);
        return target !== null && target.address === address && target.identity === identity;
    }
    function forgetIdentity() {
        if (flow.busy)
            flow.cancel();
        if (flow.sessionOpen)
            flow.disconnect();
        // A confirmed account/configuration change must clear the previous list.
        flow.setWorkstations([]);
        flow.invalidateCatalog();
    }
    property Connections flowEvents: Connections {
        target: assignments.flow
        function onRefreshRequested(token) { assignments.provider.refresh(token); }
        function onCatalogCancelRequested(token) { assignments.provider.cancel(token); }
        function onCheckRequested(token, nodeId, displays, resume) {
            var target = assignments.resolveLoginTarget(token, nodeId);
            if (!target) {
                assignments.flow.invalidateCatalog();
                return;
            }
            assignments.loginRequested(token, nodeId, target.address, target.identity, displays, resume);
        }
    }
    property Connections providerEvents: Connections {
        target: assignments.provider
        function onCatalogReady(token, entries, validityMs) {
            // Native QVariantList signals expose a QML sequence, not a JS Array.
            // Normalize here so the flow keeps its strict array/schema checks.
            assignments.flow.acceptCatalog(token, Array.from(entries), validityMs);
        }
        function onCatalogFailed(token, reason) { assignments.flow.rejectCatalog(token); }
        function onCatalogInvalidated() { assignments.flow.invalidateCatalog(); }
        function onIdentityChanged() { assignments.forgetIdentity(); }
    }
}
