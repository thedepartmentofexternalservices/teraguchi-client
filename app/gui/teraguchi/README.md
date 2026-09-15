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
  square controls, visual display selection and scrolling.
- `TeraguchiTheme.qml`: 1986 Studios Coolant color and typography tokens.

Use the root `scripts/test/check-workstation-ui.sh` with pinned Qt 6.10.2 to
compile, test and render the preview. The root documentation records exact
source commits and qualification limits. Alan Latteri's PLANK remains the
client/host foundation; no Replay, transport or authentication code is copied.
The Coolant wordmark is a design proposal, not final package artwork.

The visual source is the supplied 1986 Studios Coolant design system's README
and `colors_and_type.css`. The theme uses its current cyan palette, not the
older orange reference in its skill metadata. Archivo, Archivo Black and
JetBrains Mono are used when installed; Helvetica Neue/Archivo and Menlo are
explicit fallbacks. No remote fonts or brand images are loaded. Bundle licensed
fonts and their notices before claiming this design is portable to clean Macs.
The root design notes record the token mapping and qualification limits.

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
