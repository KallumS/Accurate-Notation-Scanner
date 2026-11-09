# Accurate Notation Scanner

A Mac app (Apple silicon, JUCE) that reads PDF scores made by notation
programs into MusicXML - exactly, or it says where it is unsure. Scanned and
handwritten pages are refused. One of the family with Noterator, Miderator
and Starting Blocks; its MusicXML is meant to open in Noterator.

The user is not a developer. Explain in plain words, show pictures of what
changed, and make sure every change reaches them as a downloadable Mac app
(the CI's `.dmg`, or `build-mac.command`).

## Where the reasons are

- **[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)** - the shape of it on one page. Start here.
- **[`docs/decisions/`](docs/decisions/README.md)** - one record per choice that could be made the other way, by number: (0002).
- **[`docs/sessions/`](docs/sessions/README.md)** - one log per working session: the route, the mistakes, the numbers.

Write to all three when something changes.

## Working in it

```sh
# core and tests: seconds (Ubuntu: apt install libmupdf-dev and its -dev libraries, see the CI)
cmake -B build-core -G Ninja -DANS_BUILD_APP=OFF -DANS_SYSTEM_MUPDF=ON && cmake --build build-core && ./build-core/ScannerTests
# the app (downloads JUCE)
cmake -B build -G Ninja -DANS_SYSTEM_MUPDF=ON -DCMAKE_BUILD_TYPE=Release && cmake --build build --target AccurateNotationScanner
# a picture of the window, no screen needed (start Xvfb :99 first)
DISPLAY=:99 "build/AccurateNotationScanner_artefacts/Release/Accurate Notation Scanner" --screenshot out.png --select 0 a.pdf b.pdf
# the accuracy check: engrave known music, read it back, compare (needs mscore3, lilypond, musicxml2ly, pip music21)
python3 tools/corpus.py --sets bach,real,random --count 40
python3 tools/flagcheck.py corpus-out musescore    # wrong bars that were NOT marked
# why a file reads wrong
ANS_DEBUG_ITEMS=1 build-core/notation-scan x.pdf --stdout -q    # every chord and rest per bar
ANS_DEBUG_TIES=1  build-core/notation-scan x.pdf --stdout -q    # every curve and what it joined
```

- **Measure every reader change with `tools/corpus.py`, both engravers**, and
  run `tools/flagcheck.py`. A change that raises the match but adds a wrong
  bar nobody is told about is a step back (0002).
- **LilyPond's numbers include musicxml2ly's own mistakes** (empty bars, lost
  ties, wild beams): look at the PDF before blaming the reader. MuseScore's
  truth is MuseScore's own MusicXML of what it engraved, so it is exact.
- **Look at the page** (`tools`-style crop with pymupdf, or the app's
  screenshot) - a stem from the wrong head is obvious there and invisible in
  the numbers.
- **`Tests/fixtures`** are real PDFs read note by note; each one is read
  exactly today. Add one (`tools/expected.py truth.musicxml > name.notes`)
  when a fix makes a new kind of score read right.
- The Mac app is built and tested on CI (`.github/workflows/build.yml`);
  check the run after pushing. `build-mac.command` builds it on the user's
  Mac, as Noterator's does.
- Commit and push early. A session's container can restart.

## Rules that are easy to break

- **Never guess** (0002). If the ink does not say it, the bar is marked. No
  fixing durations to make a bar add up, no assuming a missing accidental.
- **Nothing reads pixels** (0001). A page without music-font glyphs is a scan
  or an outline-only PDF, and is refused with the reason.
- **Only `PdfReader.cpp` talks to MuPDF** (0003). Everything after it works
  on `Ink.h`'s plain structs, which is what keeps it testable.
- **Glyphs are SMuFL names** after `Glyphs.cpp`. New fonts are added there
  as a name table, never by guessing shapes.
- **Every issue carries its page and box**: the app outlines it, so a
  message without a place is no use to the user.
- `Xml.*` is Noterator's, copied; keep them alike.
