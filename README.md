# Stream Song Sync — v0.1.1 (real CI build run, one bug found and fixed)

Answers the idea from the tweet: play a second, copyright-safe track on its own audio output
(a virtual cable), kept in sync with the real level song's start point and every restart, so
OBS can capture that device instead of the real one while you still hear the original song.

## Status — read before building

**A real build has now run**, via the GitHub Actions workflow
(`geode-sdk/build-geode-mod@main`, Windows runner) on `Mifu999/stream-song-sync`. This sandbox
still has no Windows/MSVC toolchain itself, so that CI run is the only real compile this mod
has had — but it got much further than "unbuilt": `mod.json`'s `"geode": "5.9.0"` correctly
drove the SDK install, CMake configured cleanly, and 32 of 33 ninja steps compiled, including
`main.cpp`, `FMODAudioEngine.cpp`, and `PlayLayer.cpp` — i.e. every hook signature, every
`getSettingValue<...>` call, and the `#include <fmod.hpp>` path (previously flagged as
unverified) are all now confirmed correct on a real toolchain.

One file failed: `SecondaryAudioEngine.cpp`, on a single bad helper call (`FMOD_ErrorString`,
see "Build #1 fix" below). That's been fixed here, but **the fix itself hasn't been through CI
yet** — next build should be the first clean one.

### Build #1 fix: `FMOD_ErrorString` doesn't exist in Geode's bundled header

The error logging helper in `SecondaryAudioEngine.cpp` originally called `FMOD_ErrorString(r)`
to turn an `FMOD_RESULT` into a readable string. That symbol lives in FMOD's own
`fmod_errors.h`, which is **not** part of what Geode bundles under `Geode/fmod` — a distinction
the earlier draft got wrong by pulling the helper from a reference copy of `fmod.hpp` in a
different skill, instead of confirming it against what a real Geode project actually sees.

Clang reported this as two errors, but it's one bug: once `FMOD_ErrorString` failed to resolve,
Clang's typo-correction silently substituted the unrelated OpenGL symbol `gluErrorString`
("did you mean..."), whose return type (`const unsigned char*`) then failed `fmt`'s
`static_assert` against formatting raw non-char pointers — a second, confusing error from the
same root cause. Fix: log the raw `(int)` `FMOD_RESULT` code instead of a string; every other
`log::` call site in the project was re-checked for the same pattern and is clean.

## What was verified, and how

Every GD class/method used here was looked up in the live-pinned Broma data (`geode-sdk-live`,
snapshot archived 2026-08-08/10) rather than guessed:

| Symbol | File:line | Hookable on win/mac/m1/ios |
|---|---|---|
| `PlayLayer::init(GJGameLevel*, bool, bool)` | `GeometryDash.bro:15039` | yes, all 4 |
| `PlayLayer::resetLevel()` | `GeometryDash.bro:15010` | yes, all 4 — hooked in 21 of the corpus's real mods |
| `PlayLayer::loadFromCheckpoint(CheckpointObject*)` | `GeometryDash.bro:15045` | yes, all 4 |
| `PlayLayer::onQuit()` | `GeometryDash.bro:15048` | yes, all 4 |
| `FMODAudioEngine::update(float)` | `GeometryDash.bro:4885` | yes, all 4 — the real hook used by 4 corpus mods for audio taps |
| `FMODAudioEngine::getMusicTimeMS(int)` | `GeometryDash.bro:4914` | yes, all 4 (returns `unsigned int`, milliseconds — exactly what `FMOD::Channel::setPosition` wants) |
| `FMODAudioEngine::m_system` (`FMOD::System*`) | `GeometryDash.bro:4876` member list | confirms the whole FMOD API is reachable from a mod |

I deliberately used `getMusicTimeMS`, not `getMusicTime` — the float version is **inline on
Windows** (no real address, can't be relied on the same way), the ms version has a real address
on every platform.

The FMOD calls in `SecondaryAudioEngine.cpp` (`System_Create`, `setDriver`, `getNumDrivers`,
`getDriverInfo`, `init`, `createStream`, `playSound`, `Channel::setPosition`, `setPaused`,
`System::update/close/release`) are all real signatures, checked against the FMOD header
bundled in the `geode-modding` skill (`references/geode-api/fmod.hpp`) rather than typed from
memory.

**I could not confirm `bindings/main` hasn't moved since that 2026-08-08 snapshot.** The SDK's
own `VERSION` file is confirmed current (5.9.0, checked live just now). The two fallback URLs
for checking bindings freshness are both blocked from this sandbox — `github.com/.../commits/main`
by robots.txt, `api.github.com` with an HTTP 403 — which is exactly the untested gap the
`geode-sdk-live` skill doc flagged when it was built. `PlayLayer` and `FMODAudioEngine` are old,
stable classes, so this is unlikely to matter, but re-run the Step 0 check yourself before
relying on this for anything you'd ship.

**Now confirmed by the real build:** `#include <fmod.hpp>` is exactly right — no extra CMake
step needed. That was the one line in the first draft with zero direct evidence behind it;
the CI compiler accepted it without complaint across every file that uses it.

## How it works

- **A second, independent `FMOD::System`** (`SecondaryAudioEngine`), separate from GD's own
  `FMODAudioEngine::sharedEngine()`. `setDriver()` points it at whatever output device you pick
  — normally a virtual cable (VB-Audio, Voicemeeter Input, etc.) that nothing else is using, so
  you don't hear it and only OBS does.
- **On `PlayLayer::init`**: loads your replacement track on that system and syncs it.
- **On `PlayLayer::resetLevel`** (every death/restart, normal mode): reads
  `FMODAudioEngine::getMusicTimeMS(0)` — GD's own current song position — and seeks the
  secondary track to match. This is the whole trick: GD already solves "where should the song
  be after this restart" for its own music; the mod just mirrors that answer.
- **On `PlayLayer::loadFromCheckpoint`**: same idea, gated behind its own setting, off by default.
- **`FMODAudioEngine::update`** is hooked purely to call `SecondaryAudioEngine::tick()` every
  frame — FMOD streams need `System::update()` pumped regularly, and this rides GD's own audio
  tick instead of adding a scheduler.

## Known risk: practice mode

`gd-modding-internals` documents that **practice-mode music sync is a known GD bug** — Misc
Bugfixes ships a dedicated fix for it (`PracticeMusicSyncPulseFix.cpp`). Since this mod's
checkpoint sync leans on the same GD-reported position, it likely inherits whatever that bug
class is. That's why "Resync on checkpoint restore" defaults to **off**. Test it specifically
before trusting it mid-grind on stream — normal-mode restart sync (the common case) doesn't go
through that code path and should be far more solid.

## Building

1. Install the Geode CLI / SDK matching your current setup (ways-of-working notes 5.8–5.10.x
   active — bump `mod.json`'s `"geode"` minimum if your install is newer than 5.9.0).
2. `GEODE_SDK=<path to your Geode checkout>` then the normal `geode build` / CMake flow.
3. First run: check the Geode console for the numbered output-device list `main.cpp` prints,
   find your virtual cable's index, put it in the "Output device index" setting.
4. In OBS: Audio Input Capture → that same virtual cable.
5. Pick a replacement track file, flip "Enabled", play a level, confirm in OBS's audio mixer
   that only the replacement track is coming through that source.

## Deliberately deferred to v2

- A real device-picker popup instead of a raw index + console log.
- Per-level track mapping (right now it's one global replacement track).
- Drift correction between the two systems over very long songs (two independent clocks will
  creep apart over minutes; `resetLevel` re-anchors on every restart, so this mostly matters for
  single long uninterrupted plays).
- Handling `GJBaseGameLayer`-level restarts outside `PlayLayer` if any exist (platformer mode,
  editor playtesting) — not checked yet.
