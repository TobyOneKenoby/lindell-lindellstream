# Lindell Live v1: website-to-AAX integration

This is the IMPLEMENTED website API in v22. It supersedes the proposed
`/api/live/...` endpoints in the AAX v0.2 starter. The plugin's Go Live button must
remain disabled until this sender integration is implemented and tested.

## Destination and authorization

The producer pastes the same shared URL given to listeners, for example:
`https://lindell-streams.com/?share=<playlist-token>`.

The producer signs into the website, opens that playlist's Live panel and creates
a private connection key. They enter that key into the plugin separately from
the playlist link. Add a masked key field and Connect button to the plugin. Keep
the key in macOS Keychain; do not serialize it into a Pro Tools session, send it
in a URL or log it. The website stores only its SHA-256 digest. The key is scoped
to that producer and playlist, expires after 30 days, and is invalid after producer
account epoch changes, disabling, ownership changes or explicit revocation.

Device/browser pairing can replace manual key entry later. The listener's share
link is NEVER publishing authorization. A plugin connection key cannot publish
to another producer's playlist or another playlist owned by the same producer.

## HTTP contract

Use HTTPS POST to the fixed site entry point:
`https://lindell-streams.com/?action=livePluginResolve` (and actions below).

- Header: `Authorization: Bearer <64-hex-connection-key>`
- Body: `application/x-www-form-urlencoded`, not JSON or multipart audio.
- Responses: JSON. Non-2xx contains `error`.
- No PHP CSRF or cookie required for plugin actions: the separate Bearer key is
  required on EVERY request. Browser management still uses login and CSRF.
- Do not forward keys through redirects or to an arbitrary pasted host. Validate
  the destination against the configured service origin and verify TLS.
- Shared URL must exactly match the current site-generated URL on Resolve/Start.
- Hostinger must pass Authorization to PHP. A 401 despite a correct key warrants
  checking HTTP_AUTHORIZATION / REDIRECT_HTTP_AUTHORIZATION forwarding.

| Action suffix | Form fields | Result |
| --- | --- | --- |
| Resolve | link | playlist, title, protocol, iceServers |
| Start | link | session, heartbeat_seconds=5, lease_seconds=25, max_listeners=8, iceServers |
| Poll | session | peers: [{id, offer, answered}], lease_seconds |
| Answer | session, peer, answer | ok |
| Stop | session | ok |

Prefix each suffix with `livePlugin`. Resolve's protocol is
`lindell-live-webrtc-v1`. Start returns 409 if a broadcaster already exists. A
new key or website End broadcast can terminate the old session. Start is not
idempotent: if its response is lost, do not repeatedly create broadcasters;
stop from the website or let its 25-second lease expire before retrying.

## WebRTC signaling and audio

This version uses complete non-trickle SDP. It has no streaming URL and no PCM
upload endpoint. The plugin needs a real native WebRTC stack; posting PCM to PHP
will not work. Use the current starter's separate capture worker and add your
native sender outside the real-time audio callback.

1. Resolve link with key. Display server-verified playlist title.
2. Prepare stereo capture/encoder and press Go Live explicitly, then Start.
3. Poll every 1–2 seconds (at most 5 seconds between successful polls). Poll extends
   the lease 25 seconds. Stop on authorization failure or session-expired 410.
4. For each new `peers[].id`, create a distinct RTCPeerConnection using the returned
   ICE configuration. Set its remote description to {type: offer, sdp: offer}.
5. Attach the mix-bus stereo audio track. Browser offers have one audio m-line,
   direction recvonly. The answer must contain one audio m-line, direction sendonly.
6. Create and set the answer, gather ICE until COMPLETE, then POST Answer with
   its full SDP. No separate ICE candidate endpoint exists. SDP limit is 60 KB.
   Duplicate identical answers are accepted; differing answers return 409.
7. Continue Poll even after connections are established. Its peer array is the
   entire current membership: close/delete connections for absent peers. A peer
   expires 35 seconds after its last listener poll. Do not retain departed peers.
8. Stop must immediately stop transmitting and close all peer connections, then
   call Stop. On network error, stop media locally; do not wait for PHP to enforce
   revocation. Disable auto-restart on session recall, plugin duplication or rate
   change, as specified in the starter package.

Start marks a producer as online before any listener joins. Call it only after
your sender is ready. The listener UI says “Listening live” only once its peer
connection is connected, not merely after Start or successful signaling.

Use one stereo track and negotiate music-appropriate codec settings. Browser
Opus is the expected initial path; test stereo preservation and disable voice
processing in the sender. Do not assume host 96 kHz reaches listeners at 96 kHz:
resample only the outgoing copy when required. Host bus remains unchanged.
No promise of bit-perfect/lossless transmission or a specific latency.

## Scaling and privacy

There is no SFU. One plugin connects independently to up to eight listeners and
its upload/encoding load scales accordingly. ICE peers can learn network address
information. A TURN service improves reachability; relay-only policy can be
added on both ends if direct addresses must be concealed. Current policy permits
direct connections. TURN is not a substitute for SFU fanout.

Short-lived TURN credentials are issued for 10 minutes. A new listener/status
request gets fresh credentials. Long sessions must reconnect/renegotiate using
fresh configuration if TURN reauthentication fails. This version does not supply
a seamless native ICE-restart protocol: a listener leaves/rejoins with a new peer.

Share-link rotation invalidates the live session. Listener polls stop playback;
plugin polls must stop sending on 410. Disabling/revoking a producer/key does the
same. This relies on a compliant publisher stopping direct media when API access
is revoked. PHP alone cannot forcibly terminate packets between malicious peers.

## Browser receiver endpoints (implemented by live.js)

GET liveStatus: playlist in query plus existing share token, or owner login.
POST liveJoin: playlist, offer; returns peer, secret, session.
POST livePoll: playlist, peer, secret; returns answer or null, session.
POST liveLeave: playlist, peer, secret.
Browser POST requests also need the normal CSRF value; listener access is checked
on every call. Peer capabilities are bound to the PHP browser session. One peer
per browser-session/playlist avoids stale duplicate tabs consuming the room.

Producer-only browser POST actions: livePair, liveUnpair, liveStop (playlist +
CSRF). Pair rotates any previous key and terminates its session.

Use actual Pro Tools/AAX and outside-network devices for final integration QA.
