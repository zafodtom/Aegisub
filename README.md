# SRTune

**SRTune** is a Windows subtitle editor focused on fast SRT timing, translation review and waveform-driven editing.

> Current status: Windows x64, SRT workflow, first public release preparation.

## Highlights

- Open video together with original and translated SRT subtitles
- Large audio waveform with direct subtitle timing
- Double-click heuristic selection of active audio regions
- Fast start/end adjustment directly in the waveform
- Create, delete, split, merge and duplicate subtitles
- Project-wide Undo / Redo for text, timing and structural changes
- Find / replace and subtitle QA (CPL, CPS, overlaps)
- Translation glossary / manual
- Czech spell checking in the translation editor
- Burn the current translation permanently into a new MP4 video
- FFmpeg is bundled with release builds so end users do not need to install it separately

## Keyboard workflow

- **Enter** – save the current subtitle and move to the next one; on the last subtitle, create a new empty subtitle
- **Shift+Enter** – insert a line break
- **Ctrl+Enter** – previous subtitle
- **Ctrl+Z / Ctrl+Y** – Undo / Redo
- **Ctrl+F / Ctrl+R** – Find / Replace
- **Ctrl+S** – Save
- **Ctrl+Delete** – delete selected subtitle(s)

## Waveform controls

- Left click – set subtitle start
- Right click / drag – set subtitle end
- Left drag – create a new interval
- Middle drag – move the whole subtitle while preserving its duration
- Middle click – seek / play
- Shift + drag – pan the waveform
- Double-click – automatically select a nearby active audio region

## Build

The WinUI frontend lives in:

`winui/Aegisub.WinUI/Aegisub.WinUI/`

For a distributable x64 build:

```powershell
.\tools\build-srtune-release.ps1 -Version 1.0.0
```

The script creates:

- `dist\SRTune-1.0.0-portable-x64.zip`
- `dist\SRTune-1.0.0-Setup-x64.exe` when Inno Setup is available

The release build is Windows App SDK self-contained.

## Project lineage and licensing

SRTune is derived from the open-source **Aegisub** codebase. Original Aegisub copyright notices and the repository's `LICENCE` file are retained.

Third-party components retain their own licenses. Video subtitle burning uses a separately bundled `ffmpeg.exe`; see `FFMPEG-NOTICE.txt` in release packages for FFmpeg attribution and licensing information.

SRTune is not presented as an official release of the original Aegisub project.
