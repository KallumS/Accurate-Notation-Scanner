# 0007 - The window is checked with a screenshot

- **Date:** 2026-11-08
- **Status:** Accepted. Noterator's 0010 in spirit.

## Decision

`--screenshot out.png [--select N] files...` opens the files, waits until
they are read, draws the window into a PNG and quits. The Linux CI job does
it with the fixtures and keeps `window.png` as an artifact; a person looks at
it after any change to the window.
