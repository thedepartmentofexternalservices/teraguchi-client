import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Dialog {
    id: dialog
    required property QtObject permissions
    property string settingsError: ""
    modal: true
    dim: false
    anchors.centerIn: parent
    width: Math.min(500, parent ? parent.width - 32 : 500)
    title: qsTr("Mac input permissions")
    standardButtons: Dialog.Close
    closePolicy: Popup.CloseOnEscape
    onOpened: { settingsError = ""; permissions.refresh(); }
    function openSettings(kind) {
        var opened = kind === "accessibility" ? permissions.openAccessibilitySettings() : permissions.openInputMonitoringSettings();
        settingsError = opened ? "" : qsTr("Couldn't open System Settings. Open Privacy & Security from the Apple menu, then choose the permission below.");
    }
    contentItem: ColumnLayout {
        spacing: 14
        Label {
            Layout.fillWidth: true
            text: qsTr("Allow %1 to capture workstation shortcuts while you're connected.").arg(dialog.permissions ? dialog.permissions.applicationName : qsTr("this client"))
            wrapMode: Text.Wrap
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("These checks apply to this running app. A diagnostic tool or another copy may have different access.")
            wrapMode: Text.Wrap
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("System Settings → Privacy & Security")
            font.bold: true
            wrapMode: Text.Wrap
        }
        ColumnLayout {
            Layout.fillWidth: true
            Label {
                objectName: "accessibilityStatus"
                Layout.fillWidth: true
                text: qsTr("Accessibility: %1").arg(dialog.permissions && dialog.permissions.checked && dialog.permissions.accessibility ? qsTr("Allowed") : qsTr("Needed"))
                wrapMode: Text.Wrap
            }
            Button {
                objectName: "openAccessibility"
                text: qsTr("Open Accessibility")
                enabled: dialog.permissions !== null && dialog.permissions.supported
                onClicked: dialog.openSettings("accessibility")
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Label {
                objectName: "inputMonitoringStatus"
                Layout.fillWidth: true
                text: qsTr("Input Monitoring: %1").arg(dialog.permissions && dialog.permissions.checked && dialog.permissions.inputMonitoring ? qsTr("Allowed") : qsTr("Needed"))
                wrapMode: Text.Wrap
            }
            Button {
                objectName: "openInputMonitoring"
                text: qsTr("Open Input Monitoring")
                enabled: dialog.permissions !== null && dialog.permissions.supported
                onClicked: dialog.openSettings("input-monitoring")
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: dialog.permissions && dialog.permissions.ready ?
                qsTr("Both permissions are allowed. Close this panel and connect when you're ready.") :
                qsTr("Enable this app in both sections, then return and check again. If macOS asks you to quit and reopen the app, do that before connecting.")
        }
        Label {
            Layout.fillWidth: true
            visible: !!dialog.settingsError
            text: dialog.settingsError
            wrapMode: Text.Wrap
        }
        Button {
            objectName: "recheckPermissions"
            text: qsTr("Check again")
            onClicked: dialog.permissions.refresh()
        }
    }
}
