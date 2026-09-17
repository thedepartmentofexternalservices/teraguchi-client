import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Dialog {
    id: dialog
    required property WorkstationFlow flow
    required property QtObject diagnostics
    required property QtObject permissions
    property string studioState: "unknown"
    property string tailscaleState: "unknown"
    property alias topicIndex: topic.currentIndex
    signal reviewPermissionsRequested()
    modal: true
    dim: false
    anchors.centerIn: parent
    width: Math.min(600, parent ? parent.width - 32 : 600)
    height: Math.min(600, parent ? parent.height - 32 : 600)
    title: qsTr("Help and support")
    standardButtons: Dialog.Close
    closePolicy: Popup.CloseOnEscape
    onAboutToShow: {
        var code = flow ? flow.supportCode : "unknown";
        topic.currentIndex = code === "permissions" || code === "tablet" ? 1 :
            code === "video" ? 3 : ["assignment", "trust", "seat", "login", "workstation", "connection"].indexOf(code) >= 0 ? 2 : 0;
    }
    function prepareReport() {
        if (!flow || !diagnostics || !permissions) return;
        // Never hand the serializer a workstation object or a free-form error.
        diagnostics.prepare({studio_setup: studioState, tailscale: tailscaleState,
            phase: flow.phase, issue: flow.supportCode, selected_displays: flow.displayCount,
            catalog_fresh: flow.catalogFresh, catalog_refreshing: flow.catalogRefreshing,
            runtime_pending: flow.runtimePending, workstation: flow.selected ? flow.selected.status : "none",
            permissions_checked: permissions.checked, permissions_supported: permissions.supported,
            accessibility: permissions.accessibility, input_monitoring: permissions.inputMonitoring});
    }
    function guidance(index) {
        switch (index) {
        case 0: return qsTr("Finish disconnecting before changing your displays. In System Settings → Displays, check that each display is connected and extends the desktop rather than mirroring it.") + "\n\n" +
            qsTr("One display: move the workstation window onto the screen you want to use, then connect.") + "\n\n" +
            qsTr("Two displays: this candidate requires exactly two independent screens in landscape orientation, arranged side by side with some vertical overlap. Keep the chosen resolution and arrangement unchanged during the session.") + "\n\n" +
            qsTr("After a cable, resolution or arrangement change, return to the workstation list and start a new connection. To use one screen instead, choose One display yourself. The client keeps your selection until you change it.");
        case 1: return qsTr("Check the pen in a local drawing app first. If it also fails there, check the tablet connection and your Wacom settings before starting another remote session.") + "\n\n" +
            qsTr("If the pen works locally but pressure, tilt, buttons or shortcuts fail in Flame, finish disconnecting and review this client's Accessibility and Input Monitoring access. If macOS asks you to quit and reopen the app, finish disconnecting first.") + "\n\n" +
            qsTr("Check again in a disposable Flame test setup. Tell studio support which action failed: pressure, tilt, hover, eraser, buttons, shortcuts or movement between screens. Permission status alone cannot prove the tablet works through the full connection.") + "\n\n" +
            qsTr("A support report contains status values only. It does not capture pen movement, keystrokes or artwork.");
        case 2: return qsTr("Use the Tailscale account that accepted the invitation for this workstation. Connect Tailscale, accept any pending invitation, then refresh the workstation list.") + "\n\n" +
            qsTr("If workstation verification or studio setup fails, ask your studio administrator for a current setup file and verified client. A changed certificate needs the administrator's review before login.") + "\n\n" +
            qsTr("Use your studio workstation account for the login prompt. If login keeps failing, contact studio support without sending your password. If another artist is connected, wait until they disconnect.") + "\n\n" +
            qsTr("After an interruption, allow cleanup to finish, refresh and reconnect to the same workstation. If it keeps failing, prepare a support report.");
        case 3: return qsTr("This candidate requires native 10-bit capture, HEVC 4:4:4 10-bit and hardware decoding for every selected display.") + "\n\n" +
            qsTr("A picture-requirements error needs a studio compatibility check. Give support the report and say whether you selected one or two displays. Changing codec or reducing picture quality cannot qualify this configuration.") + "\n\n" +
            qsTr("Local status checks do not prove color accuracy, tablet behavior or stable playback. Those checks still need a supervised session on the intended hardware.");
        }
        return "";
    }
    contentItem: ColumnLayout {
        spacing: 12
        ComboBox {
            id: topic
            objectName: "supportTopic"
            Layout.fillWidth: true
            model: [qsTr("Displays"), qsTr("Tablet and shortcuts"), qsTr("Access and login"), qsTr("Picture requirements"), qsTr("Support report")]
            Accessible.name: qsTr("Help topic")
        }
        ScrollView {
            objectName: "supportGuidanceScroll"
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: topic.currentIndex !== 4
            clip: true
            contentWidth: availableWidth
            Label {
                objectName: "supportGuidance"
                width: parent.width
                text: dialog.guidance(topic.currentIndex)
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
            }
        }
        Button {
            objectName: "supportReviewPermissions"
            visible: topic.currentIndex === 1
            text: qsTr("Review permissions…")
            onClicked: { dialog.close(); dialog.reviewPermissionsRequested(); }
        }
        ColumnLayout {
            visible: topic.currentIndex === 4
            Layout.fillWidth: true
            Layout.fillHeight: true
            Label {
                Layout.fillWidth: true
                text: qsTr("Prepare a snapshot of version and status values, then review it before saving. It excludes identities, addresses, logs, credentials, keystrokes, pen data and artwork. Nothing is sent automatically.")
                wrapMode: Text.Wrap
            }
            Button {
                objectName: "prepareSupportReport"
                text: qsTr("Prepare report")
                onClicked: dialog.prepareReport()
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                TextArea {
                    objectName: "supportReportPreview"
                    readOnly: true
                    selectByMouse: true
                    textFormat: TextEdit.PlainText
                    text: dialog.diagnostics ? dialog.diagnostics.preview : ""
                    placeholderText: qsTr("Your report preview will appear here.")
                    wrapMode: TextEdit.Wrap
                    font.family: "monospace"
                    font.pixelSize: 11
                    Accessible.name: qsTr("Support report preview")
                }
            }
            RowLayout {
                Button {
                    objectName: "saveSupportReport"
                    text: qsTr("Save report")
                    enabled: dialog.diagnostics && dialog.diagnostics.preview.length > 0 && !dialog.diagnostics.saved
                    onClicked: dialog.diagnostics.save()
                }
                Button {
                    objectName: "showSupportFolder"
                    text: qsTr("Show folder")
                    enabled: dialog.diagnostics && dialog.diagnostics.saved
                    onClicked: dialog.diagnostics.showFolder()
                }
            }
            Label {
                objectName: "supportSaveMessage"
                Layout.fillWidth: true
                text: dialog.diagnostics ? dialog.diagnostics.message : ""
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
            }
        }
    }
}
