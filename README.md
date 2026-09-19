# Lindell Streams Live — macOS VST3 0.3

Stereo mix-bus sender for the existing Lindell Streams Live website API. Based on
Tobias's 0.1 starter and recovered 0.2 playlist-link work; see Docs/WEBSITE-API.md.

## Implemented
- Native VST3 for Intel and Apple Silicon, macOS 12+; optional AAX source target.
- Byte-unchanged stereo audio pass-through and peak/RMS/clip meters.
- Playlist link plus private publishing key, HTTPS Resolve/Start/Poll/Answer/Stop.
- Server-verified playlist title; explicit start/stop, connected-peer count.
- Native WebRTC SRTP using libdatachannel, music Opus stereo at 192 kbps target,
  48 kHz outgoing rate; libsamplerate sinc conversion of the outgoing copy only.
- Separate signaling and media threads, bounded real-time PCM queue, generation
  filtering, cancellation watchdog and stale-queue trimming. No network, locks,
  or allocation in the host callback.
- No automatic broadcast on session recall/reconfigure; bypass/offline render stop
  capture. A 3-second missing-audio watchdog ends a suspended host's broadcast.
- Private keys kept in memory, never serialized or sent to untrusted URL hosts;
  redirects disabled. Playlist share links remain in saved DAW state by design.

## Dependencies and build
JUCE 8.0.15, libdatachannel v0.23.2 (MPL-2.0), Opus v1.5.2 (BSD),
libsamplerate 0.2.2 (BSD), Mbed TLS v3.6.4 (Apache-2.0), and libdatachannel's
pinned submodules. CMake builds static network/codec dependencies into the bundle.
Retain upstream notices. The JUCE licensing decision remains with Lindell.
AAX still requires Emmanuel's licensed Avid SDK, allocated IDs and signing pipeline.

GitHub Actions builds the universal plugin, runs core/parser/codec tests, loads
the actual VST3 for a null test and editor check, and sends two distinct sine
signals over real local WebRTC to headless Chromium, checking decoded stereo.
Synthetic tests use localhost only and no production account/key.

## Required deployment and limits
The website v22 patch and its server configuration are separate and are not
installed by this repository. Use Docs/WEBSITE-API.md and Docs/TURN-CONFIG.md.
The provided starter alone had no network sender; 0.3 adds that sender.

UDP ICE/STUN/TURN via libjuice; TURN TCP/TLS is not supported in this build.
One native connection per listener, maximum eight; no SFU or recording. Live
streaming is lossy and may resample. No promised latency. Real website authorization,
relay routing, Safari/mobile listening and long-session tests require the deployed
service and user's devices. The plugin never broadcasts until Go Live is pressed.

## Install
See INSTALL-PLUGIN.txt. Ad-hoc development signing only, not Apple notarized.
