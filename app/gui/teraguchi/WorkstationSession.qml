import QtQuick 2.15

// Own the Session until both exec() and deferred native cleanup have completed.
// Signals from retired attempts cannot alter a later attempt or close its view.
QtObject {
    id: runtime
    required property WorkstationFlow flow
    property var session: null
    property var presentationWindow: null
    property int token: -1
    property bool executing: false
    property bool cleanupComplete: false
    property bool stopped: false
    property string failure: ""
    readonly property bool pending: session !== null
    signal released()
    signal presentationStarted()
    function prepare(requestToken, nativeSession) {
        if (session || !nativeSession) return false;
        session = nativeSession;
        token = requestToken;
        cleanupComplete = false;
        failure = "";
        stopped = false;
        if (!flow.beginNativeSession(token)) stop();
        startTimer.start();
        return true;
    }
    function stop() {
        if (!session) return;
        stopped = true;
        session.requestDisconnect();
    }
    function releaseIfReady() {
        if (executing || !cleanupComplete || !session) return;
        session = null;
        token = -1;
        flow.runtimePending = false;
        released();
    }
    property Timer startTimer: Timer {
        interval: 1
        onTriggered: {
            runtime.executing = true;
            runtime.session.exec(runtime.presentationWindow);
            runtime.executing = false;
            runtime.releaseIfReady();
        }
    }
    property Connections events: Connections {
        target: runtime.session
        function onPresentationReady() {
            if (runtime.stopped || !runtime.flow.acceptNativeConnection(runtime.token)) {
                runtime.stop();
                return;
            }
            runtime.presentationStarted();
        }
        function onDisplayLaunchError(text) { runtime.failure = text; }
        function onStageFailed(stage, code, ports) {
            runtime.failure = qsTr("The workstation connection failed. Refresh the list and try again.");
        }
        function onSessionFinished() {
            if (runtime.token === runtime.flow.generation) {
                runtime.flow.retainsSession = false;
                runtime.flow.resumeAttempt = false;
                if (runtime.failure && !runtime.stopped)
                    runtime.flow.block(qsTr("Couldn't open the workstation"), runtime.failure, "connection");
                else
                    runtime.flow.phase = "idle";
            }
        }
        function onReadyForDeletion() {
            runtime.cleanupComplete = true;
            runtime.releaseIfReady();
        }
    }
    property Connections flowEvents: Connections {
        target: runtime.flow
        function onCancelRequested(token) { if (token === runtime.token) runtime.stop(); }
        function onDisconnectRequested(id) { runtime.stop(); }
    }
}
