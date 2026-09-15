import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Frame {
    id: panel
    required property QtObject setup
    property bool busy: false
    signal importRequested()
    RowLayout {
        anchors.fill: parent
        ColumnLayout {
            Layout.fillWidth: true
            Label {
                objectName: "studioSetupTitle"
                Layout.fillWidth: true
                font.bold: true
                textFormat: Text.PlainText
                text: panel.setup && panel.setup.ready ? panel.setup.label : qsTr("Studio setup needed")
            }
            Label {
                objectName: "studioSetupMessage"
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                text: !panel.setup ? "" : panel.setup.message || (panel.setup.state === "development" ? qsTr("Using the development launcher’s studio setup.") :
                    panel.setup.ready ? qsTr("Verified studio setup. Accept your workstation invitation in Tailscale, then refresh.") :
                    panel.setup.canImport ? qsTr("Import the studio setup file provided by your administrator.") :
                    qsTr("Ask your administrator for the configured client and studio setup file."))
            }
        }
        Button {
            objectName: "importStudioSetup"
            text: qsTr("Import setup…")
            enabled: panel.setup && panel.setup.canImport && !panel.busy
            onClicked: panel.importRequested()
        }
    }
}
