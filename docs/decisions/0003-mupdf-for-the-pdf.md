# 0003 - MuPDF reads the PDF, built from source into the app

- **Date:** 2026-11-08
- **Status:** Accepted

## Context

Reading real-world PDFs means compressed and object streams, broken
cross-reference tables, empty-password encryption, embedded CFF, TrueType
and Type 1 fonts with subset names. Apple's PDFKit is Mac-only, so the core
could not be tested on Linux where the work is done.

## Decision

MuPDF, through a custom device (`PdfReader.cpp`, the only file that sees
it). It gives each glyph's font, glyph id, glyph name and exact transform,
and each path. Built from a pinned release (1.26.9) cloned from GitHub with
its third-party libraries, as static libraries, so the app carries it.
`-DANS_SYSTEM_MUPDF=ON` uses Ubuntu's package for quick work on Linux.

## Consequences

- MuPDF is AGPL-3.0; the app is GPL-3.0, which may be combined with it.
- The first Mac build compiles MuPDF (a few minutes).
- The Linux sandbox used for development cannot compile MuPDF from source
  (blocked), so that path is exercised by the Mac CI job and
  `build-mac.command`.
