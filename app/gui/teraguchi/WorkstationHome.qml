import QtQuick 2.15

WorkstationPicker {
    id: home
    required property QtObject studioSetup
    required property QtObject permissions
    property string tailscaleState: "unavailable"
    property bool cleanupPending: false
    signal permissionsRequested()
    studioLabel: studioSetup && studioSetup.ready ? studioSetup.label : ""
    settingsAvailable: true
    readonly property string nextStep: cleanupPending ? "cleanup" :
        !studioSetup || !studioSetup.ready ? "studio" :
        ["ready", "refreshing"].indexOf(tailscaleState) < 0 ? "tailscale" :
        !permissions || !permissions.ready ? "permissions" : ""
    noticeText: {
        switch (nextStep) {
        case "cleanup": return qsTr("Finishing the previous session…");
        case "studio": return qsTr("Studio setup needs attention. Open Settings to check or replace it.");
        case "permissions": return qsTr("Set up Mac input to use workstation shortcuts and your tablet.");
        case "tailscale":
            switch (tailscaleState) {
            case "missing": return qsTr("Install Tailscale and accept your studio’s workstation invitation.");
            case "needs-login": return qsTr("Sign in to Tailscale with the account that accepted your invitation.");
            case "needs-approval": return qsTr("Your Tailscale device is waiting for approval from an administrator.");
            case "stopped": return qsTr("Connect Tailscale, then refresh your workstations.");
            case "no-shared-workstations": return qsTr("No configured studio workstations are visible. Accept your Tailscale invitation, then refresh.");
            default: return qsTr("Tailscale is unavailable. Check the connection, then refresh.");
            }
        }
        return "";
    }
    noticeAction: nextStep === "studio" ? qsTr("Open Settings") :
        nextStep === "permissions" ? qsTr("Set up Mac input") :
        nextStep === "tailscale" ? qsTr("Refresh") : ""
    onNoticeRequested: {
        if (nextStep === "studio") settingsRequested();
        else if (nextStep === "permissions") permissionsRequested();
        else if (nextStep === "tailscale") flow.refresh();
    }
}
