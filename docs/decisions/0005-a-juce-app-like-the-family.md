# 0005 - A JUCE app, in the family's look

- **Date:** 2026-11-08
- **Status:** Accepted

## Context

The family's Mac apps (Noterator, Miderator) are JUCE, built the same way,
with one colour scheme. A SwiftUI app could not be compiled or looked at in
the Linux environment the work is done in; a JUCE app can be built and
screenshotted there (0007).

## Decision

JUCE 8.0.15, the house colours (Noterator's `Theme.h`), Apple's writing
style for labels. One window: files, the page with marked bars, the list of
issues, Save / Save All / Show in Finder / Open in Noterator (shown when
Noterator is installed).
