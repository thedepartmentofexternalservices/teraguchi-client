# Teraguchi workstation interface

These QML components are an isolated first slice of Teraguchi's workstation
picker and connection flow. They are not loaded by `main.qml` or the production
client resource file yet. The root repository's `probes/workstation-picker`
harness compiles them into an offline preview with sample assigned workstations.
The existing installed client and its bookmarks are unchanged.

- `WorkstationPicker.qml`: assigned list, availability, display selection,
  connection/recovery actions and expandable details.
- `WorkstationFlow.qml`: presentation state and an asynchronous adapter boundary.
- `TeraguchiButton.qml`, `TeraguchiDisplayChoice.qml`, `TeraguchiScrollBar.qml`:
  native-style buttons, display radio selection and scrolling.
- `TeraguchiTheme.qml`: system font, light/dark surfaces and selection colors.
- `TeraguchiComputerIcon.qml`: original QML workstation glyph.

Use the root `scripts/test/check-workstation-ui.sh` with pinned Qt 6.10.2 to
compile, test and render the preview. The root documentation records exact
source commits and qualification limits. Alan Latteri's PLANK remains the
client/host foundation; no Replay, transport or authentication code is copied.
The preview uses Qt Quick Controls' macOS style and the application system
font. Set the style before loading QML; do not override native control
backgrounds/content items. The searchable sidebar intentionally uses a Basic
delegate. A Basic spinner avoids an optional WebP dependency in Qt's macOS
spinner. No custom font, remote asset or copied Apple icon is required.

Use Cocoa and the default Mac graphics backend for native visual captures.
Offscreen software rendering remains useful for behavior tests but does not
paint native controls accurately. The root checker supports explicit native
captures and records that distinction. Full product resource, lifecycle and
accessibility integration remain open.

## Adapter contract

The adapter supplies a current assigned catalog through `setWorkstations`.
Only explicit assigned entries appear. Available means a check may start;
it is not certificate trust, account authorization, seat ownership, hardware
attestation or physical output qualification.

`checkRequested` includes a generation token, stable workstation ID, selected
display count and reconnect flag. The adapter must use existing trusted PLANK
login, session and strict-video checks before returning `acceptCheck`. Its
positive result requires explicit authorization, seat availability, exactly the
requested displays, native ten-bit source, the selected HEVC profile and
hardware decode. This is an in-process presentation contract, not a new network
schema. The actual backend remains the security and media-policy authority.

Only a matching successful check emits `connectionRequested`; completion returns
through `acceptConnection`. Cancellation and changing catalog membership retire
the old generation. An adapter must discard stale work and release its own
resources even when the view ignores a callback. A stable ID must be resolved
to the current model entry at use time, never retained as an old row index.

A dropped connection retains its recovery context until explicit reconnect
or `disconnectRequested`. Reconnect checks the same selected displays again.
Assignment removal cancels pending work and disconnects this client's retained
session. There is no logout, takeover, shutdown or restart action. The adapter
must preserve the desktop when handling disconnect and retain existing input
cleanup. It must also handle window close/application Quit; the preview does
not stand in for production lifecycle integration.

Public host metadata alone cannot establish assignment or authentication.
Do not connect this view directly to mDNS discoveries or label existing
`authorized`/online bookmark roles as proof that a seat is free. Production
integration needs authoritative assignment, freshness and occupancy handling.
No password collection, settings persistence or permission changes occur here.

## Optional studio power presentation

`WorkstationPicker.studioPower` is null by default. A managed native adapter may
supply `enabled`, `revision`, `pending`, `report(id)`, `requestStart(id)` and
`requestRefresh(id)`. Reports match a stable assigned workstation ID and contain
`state`, boolean `fresh`, boolean `startAllowed`, and optional `outlet` telemetry.
Only a fresh, permitted `off` or verified `standby` report for an offline assigned
workstation permits Power on. A powered outlet with unknown machine state is
never enough. Session/connection logic is separate and unchanged.

The adapter latches pending before dispatch and enforces duplicate suppression,
freshness, identity and policy changes. The private service repeats authorization
and safety checks and tracks durable jobs; QML flags are not security authority.
A timeout must reconcile the accepted job, never blindly repeat a cycle. No
Power off, raw cycle, Slack commands, controller address or credential is exposed.
The root studio-power design records the full contract and implementation order.
Power telemetry is available behind Show power details; the verified-standby
cycle explanation remains visible before Power on. Only a fake provider exists in the root preview harness; no live adapter is built.
