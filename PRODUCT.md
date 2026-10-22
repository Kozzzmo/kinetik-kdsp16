# Product

<!-- impeccable:product-schema 1 -->

## Platform

web

(Not a web app: a native desktop audio plug-in, VST3 / AU / CLAP, built with DPF; the whole UI is drawn with
NanoVG in `src/KinetikUI.cpp` on a 1180×720 canvas, resizable 80–200 %. No HTML, no browser.)

## Users

Music producers, in two equal situations:
- **Studio**: inside a DAW, building and editing patterns over long sessions with a mouse.
- **Live / jamming**: playing the instrument in real time, switching patterns, muting tracks, reading the
  screen quickly and from a distance.

## Product Purpose

Kinetik KDSP-16 ("Kinetik Drum Synth Program") is a 16-track drum synthesizer plug-in with a groovebox
sequencer. Success: people build grooves fast, play them live, and enjoy touching the instrument.

## Positioning

- **The groovebox workflow inside a plug-in**: 64-step sequencer per pattern, parameter locks, conditional trigs,
  ratchets, micro-timing, per-track lengths, 16 patterns.
- **Gesture and play**: RANDOMIZE (sound / sequence / pattern / everything) with UNDO, MORPH between presets,
  REC that records knob moves as locks; the instrument as a playground.
- **Visual identity**: the Kinetik look itself (logo, skins) is part of what the product is.

## Operating Context

Runs inside a host DAW window (Ableton Live, Logic, etc.), next to other plug-ins and the host's own UI; often
on a laptop screen, sometimes on stage. The host may drive transport and tempo.

## Capabilities and Constraints

- 16 tracks of original synthesis (analog-style, FM, physical models, noise generators, user SAMPLE); master
  echo / reverb / EQ / compressor; 4 stereo outputs; 21 factory presets; preset browser; MIDI export.
- Pages: SYNTH FILTER FX MIX LFO 1 LFO 2 TRACK; an upper display draws a graphic per page.
- Skin system: 4 skins (MIDNIGHT, RISO, NEON, CLASSIC). `Skins.hpp` and `Fonts.hpp` are shared with the sister
  plug-in Kinetik KMSP-6 and must stay identical in both repositories.
- Every function must be kept when the look changes.

## Brand Commitments

- Name "Kinetik", product "KDSP-16", subtitle "DRUM SYNTH PROGRAM".
- One logo for every skin: serif italic "Kinetik" + four-point star.
- NEON stays the default skin; new looks ship as additional skins.
- English UI; large readable text (label floor 9.5 px ×1.13 mono).
- Legal red lines: no Elektron names, graphics or trade dress; no hardware maker's abbreviations; no VST logo.

## Evidence on Hand

- Screenshots can be produced on Linux (Xvfb + jackd dummy, see CLAUDE.md).
- No testimonials, users, press or sales figures yet: never invent them.

## Product Principles

1. Fast to read while playing: state (playing step, mutes, pattern) is visible at a glance and from a distance.
2. Every function stays reachable; the look changes, the workflow does not.
3. Play is encouraged: bold experiments are always one UNDO away.
4. The instrument has a personality of its own, not a generic plug-in look.
