import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Dialog {
    id: dialog
    required property WorkstationFlow flow
    required property TailscaleLogin login
    property int requestToken: -1
    modal: true
    dim: false
    anchors.centerIn: parent
    width: Math.min(420, parent ? parent.width - 32 : 420)
    title: qsTr("Sign in to your workstation")
    closePolicy: Popup.CloseOnEscape
    function clearCredentials() { password.clear(); username.clear(); }
    function submit() {
        if (!username.text.trim() || !password.text || login.requestId) return;
        login.submit(requestToken, username.text.trim(), password.text);
        password.clear();
    }
    onRejected: { clearCredentials(); flow.cancel(); }
    onClosed: clearCredentials()
    contentItem: ColumnLayout {
        spacing: 12
        Label {
            Layout.fillWidth: true
            text: dialog.flow && dialog.flow.selected ? dialog.flow.selected.name : ""
            wrapMode: Text.Wrap
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("Use your workstation username and password.")
            wrapMode: Text.Wrap
        }
        TextField {
            id: username
            objectName: "assignedUsername"
            Layout.fillWidth: true
            placeholderText: qsTr("Username")
            Accessible.name: qsTr("Workstation username")
            enabled: dialog.login !== null && !dialog.login.requestId
            onAccepted: password.forceActiveFocus()
        }
        TextField {
            id: password
            objectName: "assignedPassword"
            Layout.fillWidth: true
            placeholderText: qsTr("Password")
            Accessible.name: qsTr("Workstation password")
            echoMode: TextInput.Password
            inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
            enabled: dialog.login !== null && !dialog.login.requestId
            onAccepted: dialog.submit()
        }
        Label {
            visible: dialog.login !== null && !!dialog.login.requestId
            text: qsTr("Signing in…")
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            Button { text: qsTr("Cancel"); onClicked: dialog.reject(); }
            Button {
                text: qsTr("Sign in")
                enabled: !!username.text.trim() && !!password.text && dialog.login !== null && !dialog.login.requestId
                highlighted: true
                onClicked: dialog.submit()
            }
        }
    }
    Connections {
        target: dialog.login
        function onCredentialsRequested(token) {
            dialog.clearCredentials();
            dialog.requestToken = token;
            dialog.open();
            username.forceActiveFocus();
        }
        function onTokenChanged() { if (dialog.login.token !== dialog.requestToken) dialog.close(); }
    }
}
