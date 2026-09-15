import QtQuick 2.15

// UI cancellation only. Native login, launch and the streaming loop repeat the
// real OS checks; a cached green label cannot authorize input capture.
QtObject {
    id: gate
    required property WorkstationFlow flow
    required property QtObject provider
    function refresh() { provider.refresh(); }
    property Connections events: Connections {
        target: gate.provider
        function onStatusChanged() {
            if (!gate.provider.checked || gate.provider.ready ||
                    (!gate.flow.busy && !gate.flow.sessionOpen)) return;
            if (gate.flow.busy) gate.flow.cancel();
            if (gate.flow.sessionOpen) gate.flow.disconnect();
            gate.flow.block(qsTr("Mac permissions needed"),
                qsTr("Enable Accessibility and Input Monitoring for this client, then connect again."), "permissions");
        }
    }
}
