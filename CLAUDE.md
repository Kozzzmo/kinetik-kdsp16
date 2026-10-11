# Kinetik KDSP-16 — project memory for Claude

Kinetik KDSP-16 ("Kinetik Drum Synth Program") is a 16-track drum synthesizer plugin (VST3 / AU / CLAP, built
with DPF and NanoVG), inspired by the workflow of classic drum grooveboxes. All synthesis is original code.
Sister product: **Kinetik KMSP-6** (mono synth), repository `Kozzzmo/kinetik-kmsp6`.

## Source map (`src/`)

| File | Role |
|---|---|
| `Params.hpp` | constants (16 tracks, 64 steps, 16 patterns, 36 lockable params per track), machines and their 8 synth params, FX/LFO params, kit fields, conditions, ratchets, echo divisions |
| `Bank.hpp` | patterns (trigs, locks, conditions, micro-timing, ratchets, per-track lengths), kit (machine, mute, kit fields); text serialization (`serializeBank` / `deserializeBank`); `applyCommand` (P M U K f l c m r T cl ct cp tr pc) |
| `Voice.hpp` | drum machines (analog-style, FM, physical models, noise generators) |
| `Engine.hpp` | sequencer + voices + master FX, 4 output buses |
| `KinetikPlugin.cpp` | DPF plugin glue: parameters, states, MIDI (notes 36–51 = tracks, or chromatic per track MIDI channel) |
| `KinetikUI.cpp` | the whole UI (≈1 700 lines, 1180×720 canvas, overlays: machine picker, presets, help, skins, save) |
| `Presets.hpp` | 21 factory presets built in code (`PB` builder) |
| `Random.hpp`, `Help.hpp`, `Skins.hpp`, `Fonts.hpp`, `Dsp.hpp` | randomizers, help texts, 4 skins, embedded fonts, DSP helpers |

User presets: `%APPDATA%\Kinetik\KDSP-16\Presets\*.kdsp` / `~/Library/Application Support/Kinetik/KDSP-16/Presets`
(header `KDSP16-PRESET 1`, `V <param> <value>` lines, then `BANK` + serialized bank).

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
  mono); one logo for every skin (serif italic "Kinetik" + four-point star); **NEON** is the default skin;
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
- Output parameters: `out_step` (absolute step counter modulo 2^20), `out_leds`, `out_pattern`.
- `paramSymbol()` tables have a `static_assert` on their size — a too-short table crashed Windows hosts once.
- Engine: per-block event list (sorted by frame then kind), swing, micro-timing, ratchets, conditions
  (probability, FILL, PRE, 1ST, A:B), pattern change at the master length, 4 stereo buses (Main + Out 2–4),
  master echo / FDN reverb / EQ / compressor / drive / safety limiter, denormals flushed (SSE and ARM64).
- `Skins.hpp`, `Fonts.hpp` (embedded OFL fonts) and `Dsp.hpp` are shared with the sister plugin
  (kinetik-kdsp16 ⇄ kinetik-kmsp6): keep them identical in both repositories.
