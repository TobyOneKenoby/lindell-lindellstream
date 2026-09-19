# 0.2.0 — Playlist link handoff

- Replaced destination placeholder with editable Playlist link and CONNECT.
- Added strict URL parser and tests grounded in the existing PHP share format.
- Added validated destination persistence, schema migration and editor recall sync.
- Stop local capture on destination changes and state recall.
- Keep GO LIVE disabled until real ownership verification and streaming exist.
- Added PLAYLIST-LINK.md and revised streaming/API/build notes.

Source-only developer package. JUCE/Mac/AAX build and host persistence tests still
required. The portable audio-core and URL-parser tests pass locally.
