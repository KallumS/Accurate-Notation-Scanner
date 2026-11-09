# 0014 - The window follows Apple's Human Interface Guidelines, as the family does

- **Date:** 2026-11-09
- **Status:** Accepted

## Context

The user asked for the window to follow Apple's HIG as set out in the Good
Idea repo (its review and decisions 0030-0031, the family's shared reading
of the nineteen Foundations pages). Noterator, the family's other JUCE app,
had already met them in JUCE (its 0048). [The review](../apple-hig-review.md)
lists each change.

## Decision

The house look stays; within it, the family's rules:

1. **Contrast**: words 7:1 or more on what they sit on; things to grab 3:1;
   the house red only marks. Increase Contrast strengthens secondary text,
   warnings and rules (Noterator's `SystemSettings` and `useIncreasedContrast`,
   copied).
2. **Interface Size** 100-200% in a View menu, remembered; the layout wraps
   (the toolbar folds into rows, the header grows) instead of cropping, and
   the smallest window shrinks with the size (Noterator's way).
3. **Nothing important along the bottom edge**: the bars that need a look
   are listed above the page.
4. **The keyboard**: clicks leave the keys with the window
   (`clicksKeepFocus`), Tab reaches every control with a ring in the text
   colour (`focusRing`), Esc hands the keys back; a menu bar with the Mac's
   shortcuts.
5. **Names for VoiceOver** on every control and list row; the title names
   the open score.
6. **"On" is not colour alone**: an ink bar under a chosen button's word.
7. **Writing** as Good Idea's 0030 and Noterator's 0054: title case and a
   real ellipsis on buttons and menus, curly quotes, messages that say what
   to do.

## Consequences

- `Tests/TestTheme.cpp` checks the colours against these ratios, read from
  `Theme.h` and `Theme.cpp`, with and without Increase Contrast.
- New controls follow the same rules: a name when the face is not a word,
  `clicksKeepFocus` for anything added later, colours from the theme.
- Not checked on a Mac: the menu bar, Increase Contrast and VoiceOver itself
  (the Mac CI job does not start); the screenshots at 100% and 200% were
  taken on Linux.
