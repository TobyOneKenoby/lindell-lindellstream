> Historical v0.2 handoff. For the implemented v0.3 sender and current build/test scope, see README.md and WEBSITE-API.md. Statements below about a local-only sender or proposed API describe the old package. AAX remains untested.

# Playlist link workflow — 0.2 handoff

Toby approved replacing the playlist dropdown with an editable Playlist link
field and CONNECT button. The link identifies the destination. Producer login
is independently required to authorize publishing.

## Implemented in this source

- Text field with paste support and accessible name, CONNECT button and Return key.
- Strict local validation for the current production playlist-share format.
- Canonical HTTPS link retained in processor state; editor reopen restores it.
- JUCE XML session state schema 2 serializes the destination link only. Schema 1,
  unknown/malformed states and invalid stored links restore an empty destination.
- Recall and link editing stop the local test. Recall never starts broadcasting.
- Format-valid Connect reports: “Link valid. Authentication service not integrated.”
- GO LIVE stays disabled. There is no fake connected state or fabricated title.

State locking is confined to UI/host-state operations, never the audio callback.
The editor polls destinationRevision to reflect host recall while it is open.

## Emmanuel: replace the Connect integration stub

1. Parse locally. Do not navigate to, scrape or fetch arbitrary pasted URLs.
2. Initiate the account pairing flow if there is no valid producer session.
3. POST the canonical playlist link to the fixed, trusted service endpoint
   `/api/live/resolve-playlist`, with producer authorization in a header. This
   endpoint is a proposal and does not yet exist in the PHP app.
4. Server validates the producer account is enabled, resolves the playlist share
   token using the real database and confirms the signed-in producer OWNS it.
   Possession of a share token is insufficient. Reject song links and removed or
   revoked shares, expired credentials and playlists owned by someone else.
5. Return a verified playlist ID and display title. Only then show “Connected to
   [title]” and permit starting a publish session. Do not enable GO LIVE until the
   actual transport integration is ready too.
6. On GO LIVE, create an authenticated publishing session and wait for its media
   acknowledgement before showing LIVE. A resolved destination is not a stream.

Use a request generation counter. Editing the link, logging out, recalling state
or closing/removing the processor invalidates pending Connect responses; a late
response must never authorize the wrong/new destination. Cancel/revoke current
broadcast before accepting a new destination. Keep networking off both audio and
GUI threads, and marshal UI results safely without capturing a dead editor.

## Persistence

The current source saves the canonical private listening link in project state,
per the requested workflow. No password, producer token or publish credential is
serialized. Do not log pasted links. In the finished service, prefer retaining
the verified playlist ID and display name after resolution; that avoids embedding
a listening capability in shared Pro Tools projects. Resolve/reauthorize on recall
and leave broadcasting off. If the remembered share link has been revoked, ask
for the new link; never silently select a different playlist.

The link parser is intentionally limited to the site's confirmed current URL
format: HTTPS, exact lindell-streams.com host, / or /index.php, exactly one share
parameter with 64 hexadecimal characters. No arbitrary ports, user-info,
fragments, song links, extra parameters or percent-encoded tokens. If deployment
changes, update the explicit parser contract and tests, not a permissive regex.

## Required tests on Mac/server

- Paste valid link, close/reopen editor, save/reopen session: destination restored,
  broadcasting OFF, ownership not assumed.
- Recall schema 1/2, corrupt state and invalid link with editor open and closed.
- Edit destination during pending Connect and during broadcasting: no stale
  response or old audio can target the newly selected playlist.
- Missing producer login, wrong owner, paused producer, revoked share, missing
  playlist and expired token yield useful errors and keep GO LIVE disabled.
- Reconnect refreshes actual title from server; never treat pasted text as a title.

No website changes or endpoints are included in this starter.
