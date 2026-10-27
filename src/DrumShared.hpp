// Kinetik KDSP-16 — direct UI <-> DSP access (same process, DISTRHO_PLUGIN_WANT_DIRECT_ACCESS)
#pragma once
#include <cstdint>

namespace kn {

struct DrumDirect {
    virtual ~DrumDirect() {}
    // trigs played on a MIDI keyboard while REC is armed and the sequencer plays
    virtual bool popRecorded(int& pattern, int& track, int& step, bool& accent) = 0;
    virtual void setRecording(bool on) = 0;
    virtual void setClick(bool on) = 0;     // metronome while recording
};

} // namespace kn
