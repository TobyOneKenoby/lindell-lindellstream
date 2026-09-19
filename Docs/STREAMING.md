> Historical v0.2 handoff. For the implemented v0.3 sender and current build/test scope, see README.md and WEBSITE-API.md. Statements below about a local-only sender or proposed API describe the old package. AAX remains untested.

# Emmanuel's integration handoff

## Product contract

One stereo mix-bus insert publishes to one producer-owned playlist. The artist
opens their existing playlist link, clicks Listen Live, and hears the feed.
Normal playback and A/B comparison stop before live playback begins. Live audio
must bypass the website's per-track EQ/limiter: this is the output of the last
Pro Tools insert. Codec conversion, if used, must be disclosed; do not describe
a lossy or resampled stream as bit-exact.

Plugin UI: logo, stereo meter, account pairing, pasted playlist link and Connect button, Go Live / Stop,
connection status. Add verified listener count only when the service supplies it.
The current editor is a development preview; its local test never sends audio.

The destination workflow is now **paste playlist link -> Connect -> verify
producer ownership -> display playlist name -> Go Live**. This replaces the
previous dropdown proposal. Read PLAYLIST-LINK.md first for the new contract.

## Separation of responsibilities

Audio callback -> bounded PCM queue -> worker/resampler/encoder -> authenticated
media service -> browser live receiver. PHP controls ownership and session access;
it does not carry continuous PCM through normal request handlers.

The queue carries interleaved float stereo, source sample rate, prepare epoch,
first source-frame index and frame count. Packets are at most 512 frames. Larger
host blocks are split. Queue capacity is 32 packets. On saturation, new packets
are dropped and the source-frame clock still advances. The callback never waits
for the worker, never logs, opens sockets, allocates heap memory or takes a mutex.

The worker currently discards locally captured packets and counts them. Replace
that extension point with a separate transport component; do not modify the host
buffer or add network calls to processBlock. Define cancellation before adding
blocking I/O: the current unbounded join is safe only for the bounded local sink.

## Transport decision to validate before implementation

Suggested first experiment: WebRTC stereo audio through a media relay, with a
browser-native receiver. Treat this as an engineering proposal, not a chosen
provider or a codec already present in this source. Evaluate encoder controls
for music, resampling, CPU, latency and stereo preservation before committing.
Disable voice-specific echo cancellation, noise reduction and automatic gain
processing wherever the chosen send path exposes them. Do not rely on default
voice settings. Native and browser sender/receiver stacks have different APIs.

For 88.2/96 kHz host sessions, a transport may need explicit conversion. Preserve
the host bus untouched and convert only the outgoing copy. A lossless listening
mode would require its own bandwidth, buffering and browser-decoder evaluation.
Do not promise a latency figure before testing across Toby's real connections.

WebRTC standard: https://www.w3.org/TR/webrtc/

## Required production state machine

Offline -> Pairing -> Ready -> Connecting -> Live -> Stopping -> Ready.
Authentication failure, expired permissions or transport failure lead to an
explicit disconnected/error state. Never display LIVE solely because the user
pressed a button. Require a media-service publish acknowledgement.

No auto-start after project recall, duplication, sample-rate reconfiguration or
application restart. Keep remembered playlist ID separate from live state.
Use an explicit broadcast generation in addition to the prepare epoch. On Stop,
invalidate that generation, cancel sending, revoke/end the server session and
let the sole consumer discard old packets. On restart, the first-frame cutoff
and generation must prevent old audio from being sent. The development local
test has no such network session and must not simply be renamed Go Live.

Similarly, handle host bypass/offline render as a production publish stop or
suspension. The starter stops enqueueing, but a real transport must also flush
queued/encoded media and tell listeners the feed has stopped. Test pause/play,
looping and host callback suspension explicitly. Sample-frame timestamps are
capture-clock timestamps, not song timeline timecodes.

Stale media must not build up indefinitely. Bound encoded queues as well as PCM
queues; on long disconnect drop stale audio, rebase transport timestamps and
reconnect deliberately. A worker may not move the producer's write cursor or
reset the shared queue concurrently. Respect the SPSC ownership rules.

## Proposed control API — NOT IMPLEMENTED

All endpoint names below are design suggestions. They do not exist on the PHP
site. Integrate with its actual account and playlist schema; do not install these
as unauthenticated standalone PHP scripts.

| Operation | Proposed endpoint | Responsibility |
| --- | --- | --- |
| Start device pairing | POST /api/live/device/start | Short-lived device/user codes, expiry, rate limits |
| Authorize plugin in browser | POST /api/live/device/approve | Signed-in producer, CSRF, explicit device approval |
| Poll device pairing | POST /api/live/device/token | Rate-limited exchange; no account password in plugin |
| Resolve pasted playlist link | POST /api/live/resolve-playlist | Producer authentication, share-token lookup and ownership check; return verified playlist ID/title |
| Begin publish | POST /api/live/sessions | Validate ownership, enforce one publisher policy |
| End publish | DELETE /api/live/sessions/{id} | Publisher/owner authorization; revoke media capability |
| Get listener capability | POST /api/live/listen | Validate existing playlist share access server-side |
| Live status | GET /api/live/status | Only authorized playlist viewers; no publishing secrets |

A publish response could contain session_id, transport, endpoint, short-lived
publish_token and expires_at. Listener credentials must be receive-only, scoped
to exactly that session, and expire quickly. Never give a share-link visitor a
publish token. Never reuse playlist share tokens as media publish credentials.
Keep long-lived credentials in macOS Keychain; do not persist secrets in a Pro
Tools session, plugin state, logs or query strings. Refresh only on a non-audio
thread. Restrict destinations to the configured service, use TLS verification.

Server tasks: media relay and network traversal as appropriate, quotas, explicit
account pause/revocation handling, publishing leases/heartbeats, stale-live
expiry, one-publisher arbitration, session cleanup, browser authorization and
listener counts. Revoke access when playlist sharing is revoked. Return honest
status when the producer disconnects.

## Website integration

Add Live beside Play and A/B Compare only when the authenticated status endpoint
allows it. Clicking Listen Live performs a user-initiated audio start, then
pauses normal audio, A/B transport and linked video. Do not attempt to synchronize
a prerecorded YouTube video to the live mix. Stop the live receiver before
returning to saved tracks. Keep listener volume control. Clearly show buffering,
reconnecting and ended states. The stream must not silently continue under A/B.

Live comments, if added, should use a separate live session/time domain; current
song timestamp comments cannot meaningfully target an unsaved live mix. Recording
and later attaching a stream to a song are out of scope for version one.

## Practical development order

1. Compile Standalone/AU on Mac and verify UI, meters and untouched bus samples.
2. Build AAX Native using your SDK; test mono rejection, stereo insert and lifecycle.
3. Select transport, make one native sender talk to one browser locally.
4. Implement account pairing/session authorization and relay deployment.
5. Wire Go Live, playlist-link Connect flow and server-acknowledged status.
6. Implement playlist Live receiver, revoke/stop behavior and reconnection.
7. Run the acceptance matrix, then sign and package via your release pipeline.
