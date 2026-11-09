# Where the fixtures come from

Each `.pdf` was engraved from a freely licensed score; each `.notes` file is
what the engraver was given, bar by bar (`tools/expected.py truth.musicxml`).

| File | Engraver | Source | Licence |
|---|---|---|---|
| `ms_*` (except below) | MuseScore 3 | music21's corpus and random scores (`tools/randomscore.py`) | music21 corpus terms / made here |
| `ms_kinkel_op10no3` | MuseScore 3 | OpenScore Lieder: Johanna Kinkel, Op. 10 No. 3 | CC0 |
| `ly_bach_*`, `ly_monteverdi_*` | LilyPond | music21's corpus, through musicxml2ly | music21 corpus terms |
| `ly_carcassi_op60no24` | LilyPond | Mutopia Project: Carcassi, Op. 60 No. 24 (no `.notes`: its truth is MIDI; used for endings) | CC BY-SA 3.0, credited on the page |
| `ms_orchestra_transposed`, `ms_orchestra_concert` | MuseScore 3 | made here (`tools/transposingscore.py`): the same orchestra written transposed and at concert pitch | made here |
| `scanned.pdf` | - | a picture of a page, to be refused | made here |
