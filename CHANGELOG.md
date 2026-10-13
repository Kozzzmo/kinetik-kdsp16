# Changelog

## 0.9.2

- **Solo** button (S) next to mute on every track: when a track is soloed, only the soloed tracks play
- **Knob values always match the sound**: the plug-in also receives every change made in its window directly, so
  hosts that sent an old value back (or lost part of a randomize / undo / preset) can no longer leave the sound out of
  step with the display — randomize works on the first press
- NEON skin: the pink outlines are red
- **SAMPLE machine**: loaded and dropped samples are normalized (-1 dB peak) and a fresh SAMPLE track starts with
  neutral filter / envelope / crush settings (the previous machine's settings made samples sound odd or almost
  silent); a new sample on a SAMPLE track resets START, LENGTH and REVERSE; TUNE 64 is exactly 0 semitones

## 0.9.1

- **Updates really replace the plug-in**: the installers now always overwrite the installed files (on macOS an update
  could leave the old version in place), and the plug-in reports its real version to hosts
- The Windows installer asks you to close your music software when the plug-in is in use
- The update window warns when an older copy sits in your personal plug-in folder (hosts may load it instead)

## 0.9

- **Update check**: the plug-in tells you when a new version is out (at most once a day, nothing is sent). Click the
  logo: DOWNLOAD & INSTALL fetches the installer, checks its signature and opens it (close your DAW to finish).
  Automatic check can be turned off in the same window.

## 0.8

- **SAMPLE machine**: your own sounds, one per track. Drop a WAV or AIFF file on the window (on a track button
  = that track; Windows and macOS) or click LOAD. TUNE, START, LENGTH, ATTACK, DECAY, REVERSE, LO-FI, DRIVE.
  Samples are saved with the project and in user presets.
- **128 steps** per track (8 pages).
- **Multi-step editing**: each right-click adds a step to the selection; knobs and RATCH / COND / TIME edit all of them.
- Randomizing a sound keeps the track's level, pan and sends; "everything" keeps the SAMPLE tracks.

## 0.7.1

- Beta test: demo mode switched off (no silences, no licence needed). It comes back for the commercial release
  (`kDemoModeEnabled` in `src/License.hpp`).

## 0.7

- **Readable names**: machines (KICK, SNARE, TOM, CLAP, RIM, COWBELL, CL HAT, OP HAT, CYMBAL, SHAKER, CLAVE,
  FM ..., PH ... for the physical models, SINE, NOISE) and every parameter (TUNE, DECAY, SWEEP, CUTOFF, RESO,
  TREMOLO, DRIVE...).
- **New pages**: SYNTH, FILTER, FX, MIX, LFO 1, LFO 2, TRACK.
- **New filter**: CUTOFF, RESO and TYPE (LP 12, LP 24, BAND, HIGH, NOTCH) with its envelope. Old presets and
  banks are converted.
- **Graphic display**: each page draws what its knobs do: the waveform of one hit, the filter curve and its
  envelope, the effects, level / pan / velocity / sends, the LFO wave and its destination, routing.
- **LFOs**: destination list (click DEST, or the box in the display), SYNC (rate as a note length locked to the
  song) and PHASE.
- **Delay and reverb models**: DIGITAL, TAPE, BBD, PING-PONG, LO-FI echoes; ROOM, PLATE, HALL, SPRING, SHIMMER
  reverbs, each with a MOD knob. Levels and sends go much higher than before.

Note: projects saved by a host with 0.6 keep their old filter knob positions, which mean something else now.
