# Kinetik KDSP-16 — project memory for Claude

Kinetik KDSP-16 ("Kinetik Drum Synth Program") is a 16-track drum synthesizer plugin (VST3 / AU / CLAP, built
with DPF and NanoVG), inspired by the workflow of classic drum grooveboxes. All synthesis is original code.
Sister product: **Kinetik KMSP-6** (mono synth), repository `Kozzzmo/kinetik-kmsp6`.

## Source map (`src/`)

| File | Role |
|---|---|
| `Params.hpp` | constants (16 tracks, 128 steps, 16 patterns, 40 lockable params per track: 0–7 machine, 8–15 TREMOLO TREM RATE EQ FREQ EQ GAIN CUTOFF TYPE RESO CRUSH, 16–23 DRIVE LEVEL PAN DELAY REVERB ENV AMT ENV DEC VELOCITY, LFO 1/2 at 24/30, SYNC+PHASE at 36+2l via `lfoParam(l,k)`), machines (readable names: KICK, SNARE..., FM ..., PH ... = physical) and their 8 synth params, kit fields, conditions, ratchets, echo divisions, master params incl. ECHO/REVERB MODEL + MOD |
| `MasterFx.hpp` | master echo (5 models) / reverb (5 models, own decay ranges, ROOM/HALL early reflections, SPRING tank loop) / EQ / compressor / limiter; MOD is model-specific (`kEchoModLabel` / `kReverbModLabel`, UI `masterName()`) — **identical in kinetik-kmsp6** |
| `Bank.hpp` | patterns (trigs, locks, conditions, micro-timing, ratchets, per-track lengths), kit (machine, mute, kit fields); text serialization (`serializeBank` / `deserializeBank`); `applyCommand` (P M U K f l c m r T cl ct cp tr pc) |
| `Voice.hpp` | drum machines (analog-style, FM, physical models, noise generators, SAMPLE = user sample per track) |
| `Sample.hpp` | SAMPLE machine data (mono float) + text codec (16-bit base64); plugin states `samples` (all tracks, saved with the project) and `smp` (UI → DSP, one track); the DSP swaps them in `run()` with a try-lock and frees old ones outside the audio thread |
| `FileDrop.hpp/.cpp`, `FileDropMac.mm` | files dropped on the window (Windows OLE `IDropTarget`, macOS dragging methods added to the DPF view at run time; Linux: none) |
| `third_party/dr_libs/dr_wav.h` | WAV / AIFF decoder (public domain / MIT-0), UI side only |
| `Engine.hpp` | sequencer + voices + master FX, 4 output buses |
| `KinetikPlugin.cpp` | DPF plugin glue: parameters, states, MIDI (notes 60–75 = C3–D#4 = tracks since 0.18.1, `kDrumNoteBase`; or chromatic per track MIDI channel), `DrumDirect` (live trig recording, click) |
| `KinetikUI.cpp` | the whole UI (1180×720 canvas; pages SYNTH FILTER FX MIX LFO 1 LFO 2 TRACK, `pageParam` lists the knobs of each; the upper LCD draws a graphic per page via `drawPageGraphic` — the SYNTH page renders one hit with a private `Voice`; overlays: machine picker, presets, help, skins, save, LFO destination menu `ovDest`) |
| `Presets.hpp` | factory presets built in code (`PB` builder; categories, level trims); the grooves themselves are in `PresetsV2.hpp` (0.20: 24 presets of 64 steps, `PB2` helpers `kBars` `kRep` `cyc` `lfo` `copy` `chain` `send` `echo` `verb`, `mix({dB...})` = per-track level from the mix checker) |
| `Random.hpp`, `Help.hpp`, `Skins.hpp`, `Fonts.hpp`, `Dsp.hpp` | randomizers, help texts, 4 skins, embedded fonts, DSP helpers |

User presets: `%APPDATA%\Kinetik\KDSP-16\Presets\*.kdsp` / `~/Library/Application Support/Kinetik/KDSP-16/Presets`
(header `KDSP16-PRESET 2`, `V <param> <value>` lines, then `BANK` + serialized bank).

## Working with the owner (read first)

- The owner (GitHub **Kozzzmo**) writes in **French**: answer in French. The plugin UI, code, comments and
  README stay in **English**.
- Git: commit as `Kozzzmo <Kozzzmo@users.noreply.github.com>`. **No `Co-Authored-By` or any Claude
  attribution** in commits, PRs or files — the owner asked explicitly not to appear as co-authored.
- The code is **proprietary** (see `LICENSE`). Never add GPL/LGPL code. Permissive third-party code is OK only
  if it is added to `THIRD_PARTY_NOTICES.txt`.
- Legal red lines: no Elektron names, firmware, samples, graphics or trade dress ("Machinedrum", "Monomachine",
  "Elektron" never appear in product text); no presets named after real artists; no VST logo (text mention only).
- UI preferences the owner validated: English UI; large readable text (labels use `textSize()` floor 9.5 px ×1.13
  mono); one logo for every skin (the "Orbit" mark + product name in Unbounded, see 0.13 / 0.8); **NEON** is the default skin;
  machine names without prefixes; no instruction text lines in the lower part of the window; window resizable
  (zoom buttons 80–200 % + drag grip); SAVE AS PRESET under PRESETS; factory "Init" = base params + empty
  sequencer; RANDOMIZE (sound / sequence / pattern / everything) with UNDO; REC records knob moves as locks.
- Launch tracker (claude.ai artifact, tasks in its db collection `tasks`):
  https://claude.ai/artifact/3DAkMghoFDeC59JUTAcdkn — update task status with the ArtifactData tool when work
  from the list is done.

## Build, test, release

- `make setup` (clones DPF at the pinned revision into `dpf/` and applies `scripts/dpfpatch.py`), then `make`
  (native) or `make universal` (macOS arm64 + x86_64). Windows cross-build from Linux:
  `make clean && make WINDOWS=true CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++`.
  `make clean` also wipes `dpf/build` — required when switching target OS, otherwise DGL links fail.
- DPF patches (`scripts/dpfpatch.py`), all needed:
  1. VST3: ignore stale host parameter values right after a UI edit (Ableton Live sends the pre-edit value back).
  2. VST3: auxiliary stereo outputs flagged as aux buses, not main.
  3. Resize: size constraint follows the dragged edge (window could shrink but not grow).
- Tests are standalone programs: `g++ -O2 -std=c++17 tests/<name>.cpp -o /tmp/t && /tmp/t`.
  Also run Steinberg's VST3 `validator` on the built `.vst3` when touching plugin glue (all tests must pass).
- CI (`.github/workflows/build.yml`): macOS universal (+ `auval` AU validation, architectures reported as
  annotations), Windows (MinGW), Linux. Every push to `main` publishes/refreshes the GitHub Release named
  `v$(cat VERSION)`. Bump `VERSION` for a new release. Pushing git tags is refused by the session proxy —
  the workflow creates the tag itself.
- Read CI results with `gh api repos/Kozzzmo/<repo>/actions/runs` and
  `gh api repos/Kozzzmo/<repo>/check-runs/<job id>/annotations` (artifact zips cannot be downloaded from here).

## Architecture rules that bit us before

- Host parameters = continuous sound parameters only. **Machine, mute and all TRACK/kit settings live in the
  Bank** (state), never as host parameters: Ableton sends stale values for them (machine "offset" bug, mute
  needing three clicks).
- UI → DSP edits go through `setState("cmd", "<lines>")` (idempotent text commands, see `applyCommand` in
  `Bank.hpp`); the UI applies the same command to its own Bank copy. Whole bank: state `bank`. View: state
  `skin` = "<skin index> <zoom %>" (also saved to a per-user file for new instances).
- DSP side: the master Bank is guarded by a mutex; `run()` only `try_lock`s and copies dirty patterns
  (bitmask, `kKitBit` for kit changes). Never block or allocate in `run()`.
- UI parameter edits are sent twice: host parameter + `pv <index> <value>` line in the cmd state (`setParam` queues,
  `flushPv` in `uiIdle`). The plugin applies `pv` itself and, for 1.5 s, ignores host values that disagree
  (`uiEdit` / `uiHold`): hosts sent stale values and lost parts of bulk edits (randomize needed two presses).
- Solo: `Bank::solo` (command `S t 0/1`, serialized only when on), `trackSilenced()`; the plugin copies `solo`
  with `mute` on kit changes.
- Output parameters: `out_step` (absolute step counter modulo 2^20), `out_leds`, `out_pattern`.
- `paramSymbol()` tables have a `static_assert` on their size — a too-short table crashed Windows hosts once.
- Engine: per-block event list (sorted by frame then kind), swing, micro-timing, ratchets, conditions
  (probability, FILL, PRE, 1ST, A:B), pattern change at the master length, 4 stereo buses (Main + Out 2–4),
  master echo / FDN reverb / EQ / compressor / drive / safety limiter, denormals flushed (SSE and ARM64).
- Parameter and machine names must stay readable and must not copy any hardware maker's abbreviations.
- Bank header `KNTK4` (`KNTK3` = filter base/width locks, converted on load); user presets `KDSP16-PRESET 2` (v1 converted by `presetIndexV1` + `migrateFilterV1`).
- Screenshots: `jackd -d dummy &`, `Xvfb :77 &`, run `bin/KinetikKDSP16` with DISPLAY=:77 (window offset 110,90 on a 1400x900 screen), `xdotool` to click, `import` to grab.
- `Skins.hpp`, `Fonts.hpp` (embedded OFL fonts), `MasterFx.hpp`, `Dsp.hpp`, `FsUtf8.hpp`, `MidiFile.hpp`, `Updater.*`,
  `License.hpp` and `tests/regress.cpp` are shared with the sister plugin
  (kinetik-kdsp16 ⇄ kinetik-kmsp6): keep them identical in both repositories.

## Added in KDSP-16 0.10 / KMSP-6 0.5 (both plug-ins)

- Undo/redo (`KinetikUI.cpp`): stacks of `Snap` (values + serialized bank + label), depth 40. `setParam` / `sendCmd` call
  `autoSnap` (coalesced: 1.2 s for knobs, 0.5 s for edits; `undoable()` skips live / transport commands);
  `applyState` sets `restoring` so bulk changes are one step. Right-click UNDO = redo; Ctrl/Cmd+Z, Ctrl+Shift+Z, Ctrl+Y.
- Tooltips: `tipAt(x, y)` text table, shown by `uiIdle` after 0.8 s of hover, hidden on click (`tipBlockUntil`).
- Preset browser: `presetView` (filtered list of `PEntry`), tabs FACTORY / USER / FAVOURITES (`presetPage`), search
  typed in the window (`presetType`), favourites in `<Presets folder>/favourites.txt` ("F name" factory, "U name"
  user). MORPH slider (`morphBegin/morphDrag/morphApply`): base = sound when first moved; switch-like knobs
  (`switchTrackParam` / `switchGlobal`: LFO wave/mode/dest/target/sync, or at most 12 distinct value texts) and the
  machines (+ their synth knobs) change at 50 %; tempo, play, pattern never morph.
- MIDI export (`exportMidi`, `MidiFile.hpp`, shared): the edited pattern over its master length to
  `fs::exportDir("<product> MIDI")` (= Documents/Kinetik/...), then `upd::openFolder`. Conditions are not applied.
- Track tools: `Pattern::rotateTrack` (shift buttons), COPY also keeps the sound (`soundClip`), right-click PASTE
  pastes it; EUCLID / ROTATE on the TRACK page (`euclidSlot`, `applyEuclid`).
- `FsUtf8.hpp` (shared): every file path is UTF-8; on Windows the helpers use the wide CRT. Use `fs::open`,
  `fs::list`, `fs::makeDir`, `fs::remove`, `fs::env` — never fopen/opendir/getenv on user paths.
- `DriveAA` (`Dsp.hpp`): ADAA tanh with a 3-tap lift (flat to 10 kHz), one per channel: track DRIVE (`Track::distAA`)
  and master DRIVE (`MasterFx::drvL/drvR`). Voice-internal drives still use `drive()`.
- `MasterFx` idle: echo / reverb stop when their sends are silent and the tail is below -120 dB (estimated from
  feedback / decay); their buffers are cleared when they fall asleep. Reverb line modulation every 32 samples.
- Unspecified evaluation order: never call a random generator twice in one function call's arguments
  (`f(r.range(), r.range())`): gcc and clang evaluate them in opposite orders, so presets differed between Windows and macOS.
- Sound regression: `tests/regress.cpp` (shared) fingerprints every factory preset against `tests/regress-ref.txt`
  (1 dB tolerance; Linux CI strict, macOS report only). After an intended change of sound run
  `./regress --write` from the repo root and say so in the commit.
- Shared files: `scripts/check-shared.sh` checks them against `scripts/shared.sha256` (CI); after editing one,
  copy it to the sister repo and run `scripts/check-shared.sh <sister repo>` to refresh both lists.
- MIDI CC learn is not built in on purpose: every sound knob is a host parameter, so the host's own MIDI learn /
  mapping works on all of them; a plug-in-side learn would fight host automation and DPF's CLAP wrapper cannot
  report parameter changes made by the DSP.
- `Sample.hpp` codec (0.10): PCM16 packed losslessly (fixed predictor order 0-3 per 4096-sample block + Rice codes,
  verbatim blocks for noise) then base64. Packed lines start with `z` (`z<track> name<TAB>sr<TAB>data`, also
  `SMP z<track> ...` in presets) so versions < 0.10 skip them (silent track) instead of decoding noise. Plain
  `<track> ...` lines (old format) are still read. Always build lines with `smp::encodeLine`. Test: `tests/samplecodec.cpp`.

## Added in KDSP-16 0.11 / KMSP-6 0.6 (both plug-ins)

- New globals (same order in both `Params.hpp`, before the outputs, so `MasterFx` indices match): `gMF_HP/LP/RESO`,
  chorus `gCHO_MODEL..gCHO_LEV` (8), CHAOS (ex glitch) `gGL_CHANCE..gGL_MIX` (7), then `gCHO_SEND + t` = one chorus send per track
  (a global host parameter, not lockable). `gOUT_STEP = gCHO_SEND + kTracks`. Outputs moved (not automatable).
- `MasterFx::process(..., sendCL, sendCR, beat0, playing, ...)`: chorus = send effect (CHORUS / ENSEMBLE / FLANGER,
  150 Hz high-pass on its input, sleeps like echo / reverb); glitch on the summed mix before the EQ: a slice
  (`kGlSizeBeats`) captured at a beat (chance per bar) or on HOLD (`glHold`, UI command `gh 0/1`, `Engine::setGlitchHold`)
  is replayed for `kGlLenBeats` with pitch glide, fade + darkening, random reverse, crossfaded edges. Engines pass the
  segment's song position (`blockB0 + segPos * blockBpf`).
- UI master tabs: ECHO REVERB MASTER FILTER CHORUS GLITCH (+ LINK on KMSP), width from `kMPages`; `fxTabDots()` marks
  active effects; GLITCH knob 8 = HOLD pad (`glHoldPad`, right-click latches); chorus send knob `choSlot(k)` / `dkChoSend`.
- Per-track SPEED: kit field `kfSpeed` (index into `kTrackSpeedMult`, 0 = 1x). The sequencer runs each track on its own
  grid (`sbt = sb / mult`; patterns still switch on the global grid; FIRST conditions use the track's start step).
  `out_step` is now in 1/24 steps (`kFineSteps`, modulo 32768 steps); UI `fineStep()`, `absStep()`, `trackK(t)`.
  Test: `tests/speed.cpp` (shared).
- RESONANCE: `resoQ(v, qMin, qMax)` in `Dsp.hpp` (exponential, x^1.25) for track filters, master filter and the
  filter graphic. Factory presets are converted in `buildPreset` (knob values and locks) so they sound as before.
- Updates: silent automatic install (see Updates below).
- KDSP-16 0.11 also: RANDOMIZE ALL SOUNDS (`randomize(4)`, OFF tracks skipped); master FILTER tab (`gMF_HP/LP/RESO`, `Engine::masterFilter`, same code as KMSP-6); CHORUS send knob on the MIX page (slot 6).

## Added in KDSP-16 0.12 / KMSP-6 0.7 (both plug-ins)

- New saved globals appended after the chorus sends (rule: saved host parameters never move, new ones go just before
  `gOUT_STEP`): `gCHA_SEND + t` (CHAOS send per track, default 127), `gMF_ON` (default 1), `gMORPH`. Defaults of every
  saved global: `globalDefault(g)` in `Params.hpp` (used by the UI, the engine and the preset builder).
- `Scene.hpp` (shared): `Scenes` in the Bank (two sound snapshots, `uint8` 0..127 for every saved parameter + machines),
  bank lines `Z a <hex> <hex>` / `Zc a`, pattern MORPH lane `Pattern::morph[step]` (`z p s v`, 255 = fader).
  `Engine::process` wraps `processBlock`: `applyMorph()` blends trackParam / global between the scenes for the block and
  restores them after (the knobs never change). Morph = fader, or the lane value on lane steps, + LFOs sent to MORPH;
  switch-like params (`morphSwitch`) jump at 50 %; synth knobs of tracks whose machine differs are not morphed.
- LFO destinations: `kDestOfBin` (bins of 4 values on KDSP, 3 on KMSP) keeps every pre-0.12 saved DEST value and adds
  virtual destinations `kDestL1Rate..`, `kDestMorph` (KMSP also `kDestPitch`). LFO -> LFO and -> MORPH use the previous
  block's LFO output (`lfoRateNext`, `lfoDepthNext`, `morphLfoNext`). Rate mod = +-2 octaves, depth mod = x0..x2.
- CHAOS (MasterFx glitch) works on `sendGL/GR` (main-bus tracks x CHAOS send): the slice is taken from the sends and
  the repeats replace the sent tracks' dry sound (`l += (repeat - send) * mix`).
- UI: master tabs ECHO REVERB CHORUS CHAOS FILTER (LINK) MORPH MASTER; clicking the active tab opens `ovFx` (effect
  presets, `FxPresets.hpp`, shared); FILTER knob 4 = ON pad (`mfOnPad`); MORPH tab = `drawMorphTab` / `morphTabClick`
  (scene pads, fader `dkMFader`, lane `dkLane`); send knobs = `sendG(k)` (FX page); LFO rings = `lfoHits` /
  `drawLfoRing` (UI-side LFO simulation `lfoUiValue`, repaint at 30 fps while visible). Presets window: TO SCENE A/B
  (`presetToScene`), LOAD PATTERNS (`loadPresetPatterns`); sound loads bring the preset's scenes.
- Factory presets: `PresetDef::expReso` = voiced with the exponential RESONANCE (skips the old-curve conversion);
  builder helpers `scene()`, `lane()`, `speed()`, `tpi()`, `gpi()`. Tests: `tests/morph.cpp` (shared).

## Licences and demo mode

- `src/License.hpp` (identical in every Kinetik repo) verifies Ed25519-signed licence files with Monocypher
  (`src/third_party/monocypher`, BSD-2/CC0). Format: `KINETIK-LICENSE 1` + `product` (KDSP-16, KMSP-6 or BUNDLE),
  `name`, `email`, `order`, `issued`, then `signature` (hex) over the previous lines.
- Installed licences: `<user data>/Kinetik/Licenses/*.kinetik-license` (APPDATA on Windows,
  ~/Library/Application Support on macOS, ~/.config on Linux).
- **Beta: demo mode is OFF** (`lic::kDemoModeEnabled = false` in `License.hpp`); set it to true before selling.
- Without a valid licence (and demo mode on) the DSP mutes the outputs for 3 s every 45 s (`demoGate` in the plugin glue). The UI
  shows "DEMO - CLICK TO REGISTER" under the logo; the licence window loads a file (DPF file browser) or pastes
  text (button or Ctrl/Cmd+V), installs it and sends `cmd` "lic" so the DSP re-checks.
- Licences are made with `tools/make_license.py` and the PRIVATE key, which the owner keeps outside every
  repository. Never commit it, never ask for it to be pasted into a conversation that is shared.
- `tests/license.cpp` uses fixtures signed with a fixed test key that the plugins never trust.

## Updates (`src/Updater.hpp/.cpp`, identical in kinetik-kdsp16 and kinetik-kmsp6)

- The UI owns an `upd::Checker`: at most every 20 h (setting `auto` in `<user data>/Kinetik/updates.txt`, also
  `last-<product>` and `skip-<product>`) a thread downloads `releases/latest/download/update.txt` with the
  system curl (macOS `/usr/bin/curl`, Windows `System32\curl.exe`). Manifest `KINETIK-UPDATE 1`: product, version,
  notes, page, mac / mac-sha512, win / win-sha512, then an Ed25519 `signature` by the **update key** (not the licence
  key). Installer URLs must start with `https://github.com/Kozzzmo/`; the installer is opened only if its SHA-512
  matches. Logo subtitle "UPDATE x AVAILABLE - CLICK"; logo click = update window (`ovUpdate`).
- The release job signs `dist/update.txt` with `tools/sign_update.py` when the secret `KINETIK_UPDATE_KEY` (hex
  Ed25519 seed) exists, otherwise it only warns. The private key stays with the owner, never in a repository.
- Automatic install (0.11 / 0.6, setting `autoinstall`, default on, toggle AUTO INSTALL in the update window): after a
  check finds a newer version, `doInstall(automatic)` downloads + verifies it and runs `runInstallerSilently` (Windows:
  NSIS `/S` via ShellExecuteEx, one UAC prompt; macOS: `osascript ... /usr/sbin/installer -pkg ... with administrator
  privileges`, path passed as argv). Lock file `Updates/installing-<product>.lock` (20 min stale), `installed-<product>`
  remembers it (other windows then show "UPDATED TO x - REOPEN TO USE IT"). The checker's thread only uses a
  `shared_ptr<Shared>` block and is detached if the window closes mid-install. The NSIS installer renames a loaded
  plug-in file to `<file>.<ticks>.old` (MoveAside macro) instead of failing; old files are deleted by the next install /
  uninstall; CI `windows-install-test` updates over a DLL loaded with LoadLibrary.
- `KN_VERSION` comes from `VERSION` (Makefile). Test: `tests/update.cpp` (links the two monocypher .c files).
- Copies older than KDSP-16 0.9 / KMSP-6 0.4 have no update check.
- The plug-in version (`getVersion`, bundle Info.plist) comes from `VERSION` via `KN_VER_MAJOR/MINOR/MICRO`
  (Makefile) — it was hard-coded once and macOS Installer then skipped the "unchanged" bundles. `make-pkg.sh` also
  sets `BundleIsVersionChecked false`; the NSIS installer moves a locked (loaded) plug-in file aside.
  `upd::strayCopy` warns about old copies in the per-user plug-in folders.

## Packaging and signing

- Installers are built in CI: `packaging/macos/make-pkg.sh` (productbuild .pkg into /Library/Audio/Plug-Ins,
  EULA shown, bundles not relocatable) and `packaging/windows/installer.nsi` (NSIS, built on Linux with
  `makensis -NOCD`, installs VST3 + CLAP into Common Files, uninstaller registered). The `windows-install-test`
  job installs and uninstalls the real installer on a Windows runner.
- Signing switches on by itself when the repository secrets exist (listed at the top of the workflow):
  Developer ID + notarization on macOS, `scripts/sign-windows.sh` (osslsigncode, .pfx) on Windows.
- `EULA.txt` (draft with [PLACEHOLDERS] the owner must fill) and `THIRD_PARTY_NOTICES.txt` ship in every package.

## Added in KDSP-16 0.13 / KMSP-6 0.8 (both plug-ins)

- Echo (`MasterFx.hpp`): 8 models; MODEL knob = 8 bins of 16 values (`kEchoModelOfBin`) so the pre-0.13 values
  (0, 26, 52, 77, 103 = 5 models) keep their model. MULTI-TAP (taps at d·(1 - k·spacing), output only), REVERSE (two
  triangular grains reading the previous window backwards; delay limited to esz / 2.4; the reversed signal feeds the
  loop), DIFFUSE (3 allpasses per channel inside the loop). New saved globals after `gMORPH`: `gECHO_RTIME` (default
  64, right time x0.5..x1.5), `gECHO_DRIVE`, `gECHO_DUCK` (envelope of the echo send), `gECHO_FREEZE` (0/1: the loop
  rewrites itself with whole-sample reads, no input; keeps the echo awake). ECHO tab = 12 slots, 6 per row
  (`mSlots()`, `mKnob(k)` depends on `mpage`), slot 12 = FREEZE pad. Echo presets reset the extras they do not set.
  Test: `tests/echo.cpp` (shared).
- Logo: the Kinetik "Orbit" mark (`LogoMark.hpp`, shared: `drawOrbitMark`, colours per `LogoStyle`; RISO = black
  badge) + product name in Unbounded Bold (`fnLogo`, `kFontUnbounded` in `Fonts.hpp`, ASCII subset, OFL). The star
  (`drawSparkle`) is no longer used by the logo. J-Slice and KDYN-10 draw the same mark with their toolkits.

## Added in KDSP-16 0.14

- Kick ATTACK: saved globals `gKATK + t` (default 64, KDSP-only, after the echo extras); `Voice::attack` (-1..+1) set in
  `controlTrack`, `Voice::shapeAttack` after the machine renders (kick machines only): fade-in up to 25 ms below 0,
  transient boost `1 + 1.5 a e^(-t / 3.5 ms)` above. UI: SYNTH page of a kick = 9 knobs (`slots()`, `kickAttackSlot()`,
  `knob(k)` width = 680 / slots; `pageParam` returns -1 for k >= 8; knob 9 = `sendG` -> `gKATK`). Test: `tests/kickattack.cpp`.
- `Scene.hpp`: `sceneFromLine` accepts shorter lines (scenes saved before parameters were added; the missing values get
  `globalDefault`) and fewer machine bytes. Shared helpers `tpIndex` / `tpOf` / `choSendG` / `chaSendG` in `Params.hpp`
  (trivial here; KMSP-6 keeps tracks 7-8 after the globals).

## Added in KDSP-16 0.14.1 — sample library

- `samples/` in the repository (one sub-folder per category, `.wav` / `.aif` / `.aiff`, see `samples/README.md`) ships
  with every installer: NSIS `-DSAMPLES=samples` -> `$INSTDIR\Samples` (= Program Files\Kinetik\KinetikKDSP16\Samples,
  replaced on install, removed on uninstall); macOS `make-pkg.sh` with env `SAMPLES_DIR` / `SAMPLES_DEST` ->
  /Library/Application Support/Kinetik/KDSP-16/Samples; zips carry a `Samples` folder (Linux: ~/.local/share/...).
  **Only bundle files Kinetik may redistribute** (owner's own recordings or a licence allowing it; list sources in
  THIRD_PARTY_NOTICES) — never copyrighted breaks or other makers' sample packs.
- UI: overlay `ovLib` (`openLibrary`, `scanLibrary`, `drawLibrary`, `libClick`): categories = sub-folders of
  `factorySampleDir()` then `userSampleDir()` (= `fs::exportDir("KDSP-16 Samples")`, prefixed "USER: "); files 2 x 11
  rows, wheel scroll; click = `loadSampleFile` + `aud <t>` command (audition), double-click also closes. `fs::listDirs`
  added to the shared `FsUtf8.hpp`.

- Text (0.14.1 / KMSP 0.10.1, owner's choice after a test of 4 variants): `kFontMono` in `Fonts.hpp` = IBM Plex Mono
  **SemiBold**; `textSize()` mono x1.13 x1.08; `number()` x1.15 when size <= 16 (values, steps), big displays unchanged.

## Removed in KDSP-16 0.15 / KMSP-6 0.11 — MORPH (owner's request)

- Scenes A/B, the MORPH fader / tab / step lane, TO SCENE (presets window) and the LFO destination MORPH are removed.
  Kept for compatibility only: `Scene.hpp` (old `Z` / `z` bank lines are still parsed, no longer written; `Bank::scenes`
  and `Pattern::morph` stay but nothing reads them), `gMORPH` (host parameter hidden, named "Unused", index kept),
  `kDestMorph` (in the bins so saved LFO DEST values keep their meaning; shown as "-", not offered in the menu, does
  nothing). `Engine::process` = `processBlock` (no `applyMorph`). `tests/morph.cpp` now checks that scenes are ignored /
  not written, CHAOS sends and the old scene-line parser. The 0.12 / 0.7 morph notes above are history.
- NEON: active step LEDs red `Color(236, 36, 50)`, accents `Color(255, 140, 120)` (drawSequencer, `th->logo == lgNeon`).

## Added in KDSP-16 0.16 / KMSP-6 0.12 (both plug-ins)

- Presets: `PresetDef::cat` (`PresetCat`, `kPresetCats`, browser chips `presetCatChip` / `presetCat`, list grouped by
  category, search matches the category) and `trimDb` (`applyLevelTrim`: master VOLUME first, overflow on track VOL),
  fitted to about -15 LUFS with `tests/audit.cpp` (shared; `--fit <LUFS>` prints trims, `AUDIT_TRACKS=1` per-track levels).
  KDSP-16 also `widenIfMono`.
- LFO-moved knobs: `drawKnob(..., mod01, modCol)` draws the arc / ticks to the modulated value (`lfoLive`: value +
  out x depth / 2, as the engine) in the LFO colour, pointer on the set value; `drawLfoBadge`. Colours: `lfoPal[3]`
  (index into `lfoPalColor`, 8 colours, no pink; defaults green #7dff5a / yellow / white, owner's choice; WHITE = ink on RISO), picked with the COLOR chip of the LFO page
  (`lfoColChip`, `lfoSwatch`, `lfoColorClick`), saved per user in the view file ("skin zoom c1 c2 c3"), not in projects.

## Added in KDSP-16 0.17 / KMSP-6 0.13 (both plug-ins)

- ALL (owner's design: Alt + knob, relative, plus a latch button): `allLatch` / `altHeld` (from mouse and motion `ev.mod`) /
  `allNow` (fixed when a gesture starts, also on scroll); `knobTarget(t, pi)` (ALL: every other track, synth knobs pi < 8
  only with the same machine; KMSP also the right-click group) -> `knobOthers(pi)`; sends `sendOthers(k)` (KDSP kick
  ATTACK: same machine). `multiBegin(sel, others)` records the bases + editParameter, `multiFollow(v)` moves them by the
  same delta, `multiEnd()`. Step locks / REC never use it. UI: `allBtn()` at the end of the page tab row (tabs narrowed),
  "ALL" badge on the knobs and accent frames on the track buttons while shown (`allShown()`).
- Metronome: `Click.hpp` (shared) in the Engine, added to the main bus after the master FX while `recording && playing`;
  1760 Hz on bar starts (4/4 from the song position), 1320 Hz on beats. `click.enabled` from the UI (`setClick` direct
  call or cmd `click 0/1`, sent at the first `uiIdle`); CLICK button under REC (`clickBtn()`, REC is 30 px high now),
  saved per user as the 6th value of the view file.
- NEON: lit track LEDs `Color(125, 255, 90)` (`ledOn` in drawTracks).
- KDSP-16 MIDI trig recording: `DISTRHO_PLUGIN_WANT_DIRECT_ACCESS` + `DrumShared.hpp` (`DrumDirect`: popRecorded / setRecording /
  setClick; fallback cmds `rec 0/1`, `click 0/1`). Engine: MIDI notes while `recording` and playing -> `RecEvent` queue
  (nearest step of the track's own grid, accent if velocity > 105) and `Track::recSkipK` so the new trig is not replayed
  right after the live hit; the UI's `pollRecorded()` (uiIdle) writes `f` commands (undoable). UI-auditions are not
  recorded (right-click on a track still records through `recordTrig`). Test: `tests/liverec.cpp`.
- 808 / 909 / R-8 MkII samples (owner's request, 2026-10-27): third-party packs ("breakcore is not a good way to get laid"
  TR folder; Boxed Ear R-8 MkII). The owner stated he holds **written permission to redistribute** them; they are in
  `samples/909`, `samples/808`, `samples/R-8 MkII` (extensions lower-cased) and listed in THIRD_PARTY_NOTICES.txt. Any
  other third-party sample still needs the same written permission first.
- Beat marks (0.17.1): 1 px line under step numbers with `s % 4 == 0` (drawSequencer).
- Pattern box (0.17.2): buttons 24 px high from y 424 (rows 28 apart), MIDI / COPY / PASTE / CLEAR at y 398, 22 px: everything inside the inner box {716, 394, 448, 88}.
- Track buttons (0.17.3): a SAMPLE track with a sample shows `uiSmp[t]->name` via `fitLabel(text, maxW, size)` (size down to 8 px, then cut with "..").

## Added in KDSP-16 0.18 — factory sample kits

- `PresetDef::smp` (per track, a path relative to the built-in library, "" = none). Kits "909 Kit", "808 Kit",
  "R-8 MkII Kit" (`kSmp909/808/R8`, `sampleKit()`), appended at the end of `kPresets` (indices of the others unchanged),
  listed first in the browser (sort key). `loadPreset` -> `loadKitSamples()`: `decodeSampleFile(factorySampleDir() + path)`
  (split out of `loadSampleFile`) + `smp` state per track; missing library -> message "Sample library not found". In tests
  the library is absent, so the kits are silent there (regress references = silence).
- MIDI drum map (0.18.1, owner's request): `kDrumNoteBase = 60` (C3 = track 1 ... D#4 = track 16) in the plug-in and the MIDI export; was 36 (C1).

## Added in KDSP-16 0.19 / KMSP-6 0.14 — pattern CHAIN

- `Chain.hpp` (shared by the five Kinetik sequencers: KDSP-16, KMSP-6, KFX-8, KBAS-1, KGRN-4): `Chain` in the Bank (bank line
  `C <on> <n> <pattern>:<repeats> ...`, command `C`, returns `kKitTouched`), `ChainPlayer` in the Engine: `start(chain, true)`
  at a restart (these engines also see a boundary at the first step), `peek()` for the block's look-ahead (`patFor`, live
  recording), `next()` at every master-length boundary in `fire()`. UI: CHAIN button left of MIDI in the pattern box, window
  `ovChain` (`drawChain` / `chainClick`, same code in every plug-in), the view follows the playing pattern while the chain runs
  (`uiIdle`). Test: `tests/chain.cpp` (shared).

## 0.19.1 (owner: "le volume du kick (analog) me semble bas")

- `kKickGain` (+3.5 dB, `kKickBoostDb`, Params.hpp) on the track volume of KICK-machine tracks in `controlTrack` (after the
  track DRIVE, so presets can be compensated exactly); `keepKickLevel` in `buildPreset` lowers the KICK tracks' VOL (and
  VOL locks) of every factory preset except Init by 10^(-3.5/40): regress unchanged. Raw voice: KICK K-weighted -14.3 vs
  909 sample -11.1 / 808 sample -8.6 at the same peak.

## 0.20.0 — new factory presets

- `PresetsV2.hpp` (see the source map). `applyLevelTrim(values, db, &bank)` (both plug-ins): after the master VOLUME, the
  rest scales every track VOL knob and VOL lock by the same factor, limited so the loudest reaches 127 (it used to clamp
  tracks one by one); quiet presets use a gentle master compression ("glue": CMP THR 80, RATIO 30). `tests/audit.cpp`
  (shared) passes the bank. Mix checker used for the levels: render the mix and each track solo, BS.1770 loudness.
