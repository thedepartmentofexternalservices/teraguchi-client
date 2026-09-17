import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: panel
    required property QtObject permissions
    signal reviewRequested()
    implicitHeight: permissionContent.implicitHeight + 24
    RowLayout {
        id: permissionContent
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: !panel.permissions || !panel.permissions.checked ? qsTr("Checking Mac input permissions…") :
                !panel.permissions.supported ? qsTr("Mac input permissions could not be checked on this platform.") :
                panel.permissions.ready ? qsTr("Mac input permissions are allowed.") :
                qsTr("Allow Accessibility and Input Monitoring before connecting.")
        }
        Button {
            objectName: "reviewPermissions"
            text: qsTr("Review permissions…")
            onClicked: panel.reviewRequested()
        }
    }
}
