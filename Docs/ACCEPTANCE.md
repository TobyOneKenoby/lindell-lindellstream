# Validation and release checklist

## Completed in this workspace

Dependency-free C++17 core compiled with GCC using -Wall -Wextra -Werror,
-pthread and -O2. Tests passed for:

- Byte-unchanged left/right source buffers while capturing.
- Accurate packet samples and boundaries for a 1,301-frame callback.
- Source rates 44,100, 48,000, 88,200 and 96,000 Hz.
- Zero-length callbacks and capture disabled.
- Peak/clip/RMS meter behavior with deterministic stereo signals.
- Full queue dropping new packets without overwriting queued packets.
- Epoch change and source clock reset on prepare.
- 100,000 concurrent packets, queue wraparound, ordered untorn payloads.

The playlist parser tests also passed: accepted canonical URLs, trimming and
case normalization; rejected song links, foreign/deceptive hosts, user-info,
ports, malformed tokens, duplicate/extra parameters, fragments and invalid paths.

These tests exercise AudioCore.h and PlaylistLink.h. They do NOT validate the JUCE wrapper, AAX
wrapper, editor rendering, macOS binaries, any streaming service or Pro Tools.
No native plugin binary is supplied. CMake generation was not run here.

## Emmanuel's Mac validation

- Compile with your vetted JUCE/AAX SDK and resolve any version-specific API changes.
- Build arm64 and Intel; verify actual supported macOS/Pro Tools combinations.
- Open editor at normal/Retina scales; verify text, button layout, meter response,
  clip reset, repeated open/close and VoiceOver labels where needed.
- Null test the host output against an unprocessed reference, local test on/off.
- Exercise block sizes 32–2,048 and variable/zero blocks; 44.1/48/88.2/96 kHz.
- Verify stereo-only bus negotiation and absence of MIDI/double-precision claims.
- Test bypass, offline bounce, transport stop/start, looping, session recall,
  duplicate instances, rate changes, suspension and removal during capture.
- Profile callback CPU/allocations; stress many plugin instances and slow worker.
- Add sanitizers and a concurrency stress run to your native CI/toolchain.

## After real networking exists

- No stale packets or unexpected auto-broadcast after Stop, reconnect or recall.
- Offline bounce/bypass cannot send previously queued audio.
- Meter state does not falsely claim listeners are receiving audio.
- Unauthorized producer cannot publish into another producer's playlist.
- Share-link listener cannot publish or access another playlist's live session.
- Token expiry, account suspension and sharing revocation terminate access.
- CPU, queues and teardown remain bounded during network stalls or server failure.
- Sender deletion closes transport promptly without touching a dead processor.
- Browser autoplay, mobile backgrounding and Bluetooth are tested on real devices.
- Live audio never overlaps normal playback, A/B or linked video playback.
- Compare local mix and remote stream for stereo, dynamics, spectral changes,
  clipping and any codec/resampling effects. Measure latency, don't infer it.
- Release only after AAX signing, Mac packaging and actual Pro Tools validation.
