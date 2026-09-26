# Changelog

## 1.0.0

First SRTune release.

### Subtitle editing
- SRT-focused original / translation workflow
- Add, delete, duplicate, split and merge subtitle rows
- Insert subtitle at current video position
- Create a new subtitle automatically when pressing Enter on the last row
- Project-wide Undo / Redo for text, timing and structural changes
- Search, replace and remove empty subtitles
- Czech spell checking for translation text

### Timing and media
- Video preview and audio waveform
- Direct start/end timing from the waveform
- Drag whole subtitle timing while preserving duration
- Waveform horizontal and vertical zoom
- Timeline overview
- Automatic active-audio interval selection by double-click
- Global subtitle time shift and 1 ms gap insertion

### Translation workflow
- Original / translation side-by-side editing
- Hide original panel
- Translation manual / glossary with TSV import and export
- QA for CPL, CPS and overlapping subtitles

### Export and distribution
- SRT save
- Burn translated subtitles into MP4 video through FFmpeg
- FFmpeg bundled with release builds
- Portable x64 ZIP and x64 installer
