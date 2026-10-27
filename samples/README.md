# KDSP-16 built-in sample library

Every audio file here (WAV or AIFF) ships with KDSP-16 and appears in the SAMPLE machine's **LIBRARY** window.
One sub-folder per category (the folder name is the category shown in the window), for example:

    samples/Kicks/Deep Kick.wav
    samples/Snares/Crack.wav
    samples/Perc/Shaker.wav

Files directly in `samples/` appear under OTHER. Only `.wav`, `.aif` and `.aiff` files are installed.

Installed to:
- Windows: `C:\Program Files\Kinetik\KinetikKDSP16\Samples` (NSIS installer, replaced on every install)
- macOS: `/Library/Application Support/Kinetik/KDSP-16/Samples` (pkg installer)
- Linux: copy the `Samples` folder of the zip to `~/.local/share/Kinetik/KDSP-16/Samples`

Users can add their own in `Documents/Kinetik/KDSP-16 Samples` (shown as USER in the window).

Only add files Kinetik has the right to distribute (own recordings or a licence that allows redistribution inside a
product), and list any third-party source in `THIRD_PARTY_NOTICES.txt`.

Current content: `909/`, `808/` and `R-8 MkII/` (third-party one-shots redistributed with written permission, see
`THIRD_PARTY_NOTICES.txt`); the factory presets "909 Kit", "808 Kit" and "R-8 MkII Kit" load their tracks from them.
