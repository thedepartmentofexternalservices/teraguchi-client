import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Dialog {
    id: dialog
    required property QtObject studioSetup
    required property QtObject permissions
    property bool busy: false
    signal importRequested()
    signal permissionsRequested()
    title: qsTr("Settings")
    modal: true
    anchors.centerIn: parent
    width: Math.min(540, parent ? parent.width - 32 : 540)
    height: Math.min(420, parent ? parent.height - 32 : 420)
    standardButtons: Dialog.Close
    closePolicy: Popup.CloseOnEscape
    contentItem: ScrollView {
        id: scroll
        clip: true
        contentWidth: availableWidth
        ColumnLayout {
            width: scroll.availableWidth
            spacing: 18
            Label { text: qsTr("Studio"); font.bold: true }
            StudioSetupPanel {
                Layout.fillWidth: true
                setup: dialog.studioSetup
                busy: dialog.busy
                onImportRequested: { dialog.close(); dialog.importRequested(); }
            }
            Label { text: qsTr("Mac input"); font.bold: true }
            MacPermissionsPanel {
                Layout.fillWidth: true
                permissions: dialog.permissions
                onReviewRequested: { dialog.close(); dialog.permissionsRequested(); }
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Workstation access comes from your Tailscale invitation and studio account.")
                wrapMode: Text.Wrap
            }
        }
    }
}
