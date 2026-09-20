## 0.4.0

- macOS TURN over verified TLS, with an explicit relay-only test mode.
- Show waiting until a listener actually connects; copy sanitized ICE/gathering/relay diagnostics.
- Preserve website setup errors instead of replacing them with generic HTTP errors.
- TLS certificate/hostname and framing CI tests. External Cloudflare audio delivery still requires a real cross-network beta test.

# 0.3.1 — REAPER clipboard fix

- Request keyboard focus from the VST3 host so text fields can handle Command-V.
- Add separate mouse-operated Paste buttons for the playlist link and private key.
- Verify system clipboard paste and text-editor shortcuts in the macOS UI test.

# 0.3.0 — Native VST3 streaming beta

- Add authenticated playlist Resolve/Start/Poll/Answer/Stop integration with the saved website Live API.
- Add native WebRTC stereo Opus sender and sample-rate conversion on a background worker.
- Keep host audio unchanged; stop broadcasting on bypass, offline render, reconfiguration, recall, and missing callbacks.
- Add universal macOS CI, actual VST3 load/null checks and native-to-Chromium stereo test.
- Production website deployment, relay traversal, REAPER lifecycle and AAX require separate validation.

# 0.2.0 — Playlist link handoff

- Replaced destination placeholder with editable Playlist link and CONNECT.
- Added strict URL parser and tests grounded in the existing PHP share format.
- Added validated destination persistence, schema migration and editor recall sync.
- Stop local capture on destination changes and state recall.
- Keep GO LIVE disabled until real ownership verification and streaming exist.
- Added PLAYLIST-LINK.md and revised streaming/API/build notes.

Source-only developer package. JUCE/Mac/AAX build and host persistence tests still
required. The portable audio-core and URL-parser tests pass locally.
