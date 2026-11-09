#include "MainComponent.h"

#include "PdfReader.h"
#include "Theme.h"

namespace ans
{

namespace
{
juce::String verdictText (const Job& j)
{
    if (! j.done) return "Reading" + juce::String::fromUTF8 ("\xe2\x80\xa6");
    switch (j.result.verdict)
    {
        case Conversion::Clean: return "Every bar checked out";
        case Conversion::WithIssues:
            return j.result.read.barsWithIssues == 1 ? juce::String ("1 bar needs a look")
                                                     : juce::String (j.result.read.barsWithIssues) + " bars need a look";
        case Conversion::Rejected: break;
    }
    bool scan = false;
    for (const auto& p : j.result.read.pages) scan = scan || p.kind == PageVerdict::Scanned;
    return scan ? "A scan: not converted" : "Not converted";
}

juce::Colour verdictColour (const Job& j)
{
    if (! j.done) return theme::textDim;
    switch (j.result.verdict)
    {
        case Conversion::Clean: return theme::good;
        case Conversion::WithIssues: return theme::warnText;
        case Conversion::Rejected: break;
    }
    return theme::textOff;
}

juce::Font font (float size, bool bold = false)
{
    juce::Font f { juce::FontOptions (size) };
    return bold ? f.boldened() : f;
}
} // namespace

// ===========================================================================

void PagePreview::show (Job* j, int p, int sel)
{
    if (j != job || p != page) image = {};
    job = j; page = p; selected = sel;
    repaint();
}

int PagePreview::preferredHeight (int width) const
{
    if (job == nullptr || ! job->done || job->result.read.pages.empty()) return 400;
    // Page sizes are not kept; A4 and Letter are both close to 1.3.
    return static_cast<int> (width * 1.35);
}

void PagePreview::render()
{
    image = {};
    if (job == nullptr || ! job->done || getWidth() < 10) return;
    const float pixels = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() != nullptr
                       ? static_cast<float> (juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->scale) : 1.0f;
    // First a cheap render to learn the page's width.
    std::string err;
    const auto& pages = job->result.read.pages;
    const int turns = page >= 0 && static_cast<size_t> (page) < pages.size() ? pages[static_cast<size_t> (page)].quarterTurns : 0;
    auto probe = renderPage (job->pdf.getFullPathName().toStdString(), page, 0.1, &err, turns);
    if (probe.width <= 0) return;
    const double pageWidth = probe.width / 0.1;
    scale = (getWidth() - 24) / pageWidth;
    auto img = renderPage (job->pdf.getFullPathName().toStdString(), page, scale * pixels, &err, turns);
    if (img.width <= 0) return;
    juce::Image out (juce::Image::RGB, img.width, img.height, false);
    juce::Image::BitmapData bd (out, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < img.height; ++y)
        for (int x = 0; x < img.width; ++x)
        {
            const auto* s = &img.rgb[(static_cast<size_t> (y) * static_cast<size_t> (img.width) + static_cast<size_t> (x)) * 3];
            bd.setPixelColour (x, y, juce::Colour (s[0], s[1], s[2]));
        }
    image = out;
}

void PagePreview::paint (juce::Graphics& g)
{
    g.fillAll (theme::sunken);
    if (job == nullptr) return;
    if (! job->done)
    {
        g.setColour (theme::textDim);
        g.setFont (font (16));
        g.drawText ("Reading the score" + juce::String::fromUTF8 ("\xe2\x80\xa6"), getLocalBounds(), juce::Justification::centred);
        return;
    }
    if (image.isNull()) render();
    if (image.isNull()) return;

    const float pixels = static_cast<float> (image.getWidth()) / static_cast<float> (getWidth() - 24);
    const juce::Rectangle<float> area (12.0f, 12.0f, image.getWidth() / pixels, image.getHeight() / pixels);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRect (area.translated (2, 3));
    g.setOpacity (1.0f);
    g.drawImage (image, area);

    // The bar being played.
    for (const auto& b : playing)
    {
        const juce::Rectangle<float> r (area.getX() + static_cast<float> (b.x0 * scale), area.getY() + static_cast<float> (b.y0 * scale),
                                        static_cast<float> (b.width() * scale), static_cast<float> (b.height() * scale));
        g.setColour (theme::accent.withAlpha (0.28f));
        g.fillRect (r);
    }

    // The chords each bar outlines, above its first staff.
    if (chords && ! job->result.read.score.parts.empty())
    {
        const auto& score = job->result.read.score;
        g.setFont (font (13, true));
        for (const auto& ch : job->chords)
        {
            const auto& m = score.parts.front().measures[static_cast<size_t> (ch.bar)];
            // the first part shown on this page in this bar holds the place
            const PlaceOnPage* place = nullptr;
            for (const auto& part : score.parts)
            {
                if (static_cast<size_t> (ch.bar) >= part.measures.size()) continue;
                for (const auto& pl : part.measures[static_cast<size_t> (ch.bar)].places)
                    if (pl.page == page && (place == nullptr || pl.box.y0 < place->box.y0)) place = &pl;
            }
            if (place == nullptr) continue;
            Fraction length;
            for (const auto& part : score.parts)
                if (static_cast<size_t> (ch.bar) < part.measures.size())
                    for (const auto& e : part.measures[static_cast<size_t> (ch.bar)].events)
                        if (! e.grace && e.offset + e.duration > length) length = e.offset + e.duration;
            if (length <= Fraction (0) && m.time) length = m.time->length();
            const double share = length > Fraction (0) ? (ch.offset / length).value() : 0.0;
            const auto text = juce::String::fromUTF8 (ch.name.c_str());
            const float x = area.getX() + static_cast<float> ((place->box.x0 + share * place->box.width()) * scale) + 3.0f;
            const float y = area.getY() + static_cast<float> (place->box.y0 * scale) - 18.0f;
            const float w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 8.0f;
            g.setColour (theme::ink.withAlpha (0.88f));
            g.fillRoundedRectangle (x, y, w, 17.0f, 4.0f);
            g.setColour (theme::text);
            g.drawText (text, juce::Rectangle<float> (x, y, w, 17.0f), juce::Justification::centred, false);
        }
    }

    // The bars that need a look.
    const auto& issues = job->result.read.issues;
    for (size_t i = 0; i < issues.size(); ++i)
    {
        const auto& is = issues[i];
        if (is.page != page || is.box.empty() || is.level == Issue::Info) continue;
        if (is.box.width() > 400 && is.box.height() > 400) continue;   // a whole page
        const juce::Rectangle<float> r (area.getX() + static_cast<float> (is.box.x0 * scale), area.getY() + static_cast<float> (is.box.y0 * scale),
                                        static_cast<float> (is.box.width() * scale), static_cast<float> (is.box.height() * scale));
        const bool sel = static_cast<int> (i) == selected;
        g.setColour ((sel ? theme::accent : theme::warn).withAlpha (0.18f));
        g.fillRect (r);
        g.setColour (sel ? juce::Colour (0xffB8A800) : theme::warn);
        g.drawRect (r, sel ? 2.5f : 1.5f);
    }
}

void PagePreview::mouseUp (const juce::MouseEvent& e)
{
    if (job == nullptr || ! job->done || ! onIssueClicked) return;
    const auto& issues = job->result.read.issues;
    for (size_t i = 0; i < issues.size(); ++i)
    {
        const auto& is = issues[i];
        if (is.page != page || is.box.empty()) continue;
        const juce::Rectangle<float> r (12.0f + static_cast<float> (is.box.x0 * scale), 12.0f + static_cast<float> (is.box.y0 * scale),
                                        static_cast<float> (is.box.width() * scale), static_cast<float> (is.box.height() * scale));
        if (r.contains (e.position)) { onIssueClicked (static_cast<int> (i)); return; }
    }
}

// ===========================================================================

MainComponent::MainComponent()
{
    for (auto* b : { &openButton, &saveButton, &midiButton, &saveAllButton, &revealButton, &noteratorButton, &playButton, &prevPage, &nextPage })
        addAndMakeVisible (b);
    playButton.setTooltip ("Play the score through the Mac's built-in instruments (Space)");
    midiButton.setTooltip ("Save the chosen score as a MIDI file");
    playButton.onClick = [this] { togglePlay(); };
    chordsButton.setClickingTogglesState (true);
    chordsButton.setTooltip ("Name the chords each bar outlines, above the bar (an analysis of the notes, not read from the page)");
    chordsButton.onClick = [this] { preview.showChords (chordsButton.getToggleState()); };
    addAndMakeVisible (chordsButton);
    midiButton.onClick = [this] { exportMidi(); };
    addAndMakeVisible (fileList);
    addAndMakeVisible (issueList);
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&preview, false);
    viewport.setScrollBarsShown (true, false);
    fileList.setRowHeight (54);
    issueList.setRowHeight (24);
    fileList.setColour (juce::ListBox::outlineColourId, theme::rule);

    openButton.setTooltip ("Choose PDF scores to convert");
    saveButton.setTooltip ("Save the chosen score as MusicXML");
    saveAllButton.setTooltip ("Save every converted score as MusicXML, next to its PDF");
    revealButton.setTooltip ("Show the saved MusicXML (or the PDF) in the Finder");
    noteratorButton.setTooltip ("Open the MusicXML in Noterator");

    openButton.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser> ("Choose PDF scores", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.pdf");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this] (const juce::FileChooser& fc) {
                                  juce::StringArray paths;
                                  for (const auto& f : fc.getResults()) paths.add (f.getFullPathName());
                                  addFiles (paths);
                              });
    };
    saveButton.onClick = [this] { saveCurrent(); };
    saveAllButton.onClick = [this] { saveAll(); };
    revealButton.onClick = [this] {
        if (auto* j = job()) (j->saved.existsAsFile() ? j->saved : j->pdf).revealToUser();
    };
    noteratorButton.onClick = [this] { openInNoterator(); };
    prevPage.onClick = [this] { if (currentPage > 0) { --currentPage; currentIssue = -1; refresh(); } };
    nextPage.onClick = [this] { if (auto* j = job()) if (currentPage + 1 < j->pages) { ++currentPage; currentIssue = -1; refresh(); } };
    preview.onIssueClicked = [this] (int i) { showIssue (i); };

    setWantsKeyboardFocus (true);
    setSize (1180, 820);
    refresh();
}

MainComponent::~MainComponent()
{
    stopTimer();
    player.reset();
    pool.removeAllJobs (true, 10000);
}

void MainComponent::addFiles (const juce::StringArray& paths)
{
    juce::Array<juce::File> files;
    for (const auto& p : paths)
    {
        juce::File f (p);
        if (f.isDirectory())
            for (const auto& entry : juce::RangedDirectoryIterator (f, true, "*.pdf;*.PDF"))
                files.add (entry.getFile());
        else if (f.hasFileExtension ("pdf"))
            files.add (f);
    }
    for (const auto& f : files)
    {
        auto* j = jobs.add (new Job());
        j->pdf = f;
        juce::Component::SafePointer<MainComponent> safe (this);
        pool.addJob ([j, safe] {
            j->result = convertPdf (j->pdf.getFullPathName().toStdString());
            if (j->result.verdict != Conversion::Rejected) j->chords = outlineChords (j->result.read.score);
            j->pages = pageCount (j->pdf.getFullPathName().toStdString());
            j->done = true;
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->refresh(); });
        });
    }
    fileList.updateContent();
    if (current < 0 && ! jobs.isEmpty()) selectFile (jobs.size() - files.size());
    refresh();
}

bool MainComponent::allDone() const
{
    for (auto* j : jobs) if (! j->done) return false;
    return true;
}

void MainComponent::selectFile (int index)
{
    if (player != nullptr && player->isPlaying() && index != current) togglePlay();
    current = index;
    fileList.selectRow (index);   // calls back here; the row is already current
    currentPage = 0;
    currentIssue = -1;
    refresh();
    // open on the first bar that needs a look
    if (auto* j = job(); j != nullptr && j->done)
        for (size_t i = 0; i < j->result.read.issues.size(); ++i)
        {
            const auto& is = j->result.read.issues[i];
            if (is.page >= 0 && is.level != Issue::Info && ! is.box.empty()) { showIssue (static_cast<int> (i)); break; }
        }
}

void MainComponent::refresh()
{
    fileList.updateContent();
    fileList.repaint();
    issueList.updateContent();
    issueList.repaint();
    auto* j = job();
    const bool ready = j != nullptr && j->done && j->result.verdict != Conversion::Rejected;
    saveButton.setEnabled (ready);
    midiButton.setEnabled (ready);
    playButton.setEnabled (ready || (player != nullptr && player->isPlaying()));
    revealButton.setEnabled (j != nullptr);
    bool anyReady = false;
    for (auto* o : jobs) anyReady = anyReady || (o->done && o->result.verdict != Conversion::Rejected);
    saveAllButton.setEnabled (anyReady);
    noteratorButton.setVisible (noteratorApp().exists());
    noteratorButton.setEnabled (ready);
    prevPage.setEnabled (j != nullptr && currentPage > 0);
    nextPage.setEnabled (j != nullptr && currentPage + 1 < j->pages);
    preview.show (j, currentPage, currentIssue);
    resized();
    repaint();
}

void MainComponent::showIssue (int issue)
{
    auto* j = job();
    if (j == nullptr || issue < 0 || issue >= static_cast<int> (j->result.read.issues.size())) return;
    currentIssue = issue;
    const auto& is = j->result.read.issues[static_cast<size_t> (issue)];
    if (is.page >= 0) currentPage = is.page;
    issueList.selectRow (issue, true, true);
    refresh();
    // scroll the bar into view
    const int y = static_cast<int> (is.box.y0 * (preview.getWidth() - 24) / 595.0);
    viewport.setViewPosition (0, juce::jmax (0, y - viewport.getHeight() / 3));
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme::ground);
    auto bounds = getLocalBounds();
    auto top = bounds.removeFromTop (56);
    g.setColour (theme::rule);
    g.fillRect (top.removeFromBottom (1));

    auto right = bounds.withTrimmedLeft (320);
    if (auto* j = job())
    {
        auto header = right.removeFromTop (64).reduced (16, 8);
        g.setColour (theme::text);
        g.setFont (font (17, true));
        g.drawText (j->pdf.getFileName(), header.removeFromTop (24), juce::Justification::centredLeft);
        g.setColour (j->done && j->result.verdict == Conversion::Rejected ? theme::warnText : theme::textDim);
        g.setFont (font (14));
        g.drawFittedText (j->done ? juce::String::fromUTF8 (j->result.summary().c_str()) : verdictText (*j),
                          header.withTrimmedRight (110), juce::Justification::topLeft, 2, 1.0f);
        if (j->pages > 0)
        {
            g.setColour (theme::textDim);
            g.drawText ("Page " + juce::String (currentPage + 1) + " of " + juce::String (j->pages),
                        header.withTrimmedRight (68).removeFromRight (120).withY (header.getY() - 24).withHeight (24),
                        juce::Justification::centredRight);
        }
    }

    if (jobs.isEmpty() || dragging)
    {
        auto area = getLocalBounds().withTrimmedTop (56).reduced (40);
        g.setColour (dragging ? theme::accent : theme::rule);
        g.drawRoundedRectangle (area.toFloat(), 12.0f, dragging ? 3.0f : 2.0f);
        g.setColour (theme::text);
        g.setFont (font (26, true));
        g.drawText ("Drop PDF scores here", area.withTrimmedBottom (area.getHeight() / 2), juce::Justification::centredBottom);
        g.setColour (theme::textDim);
        g.setFont (font (15));
        g.drawFittedText (juce::String::fromUTF8 (
                              "They become MusicXML for Dorico, Sibelius, MuseScore, Finale and Noterator.\n"
                              "Scores made on a computer \u2014 exported from a notation program \u2014 are read exactly, "
                              "and every bar is checked. Scans and handwriting are not read: that would mean guessing."),
                          area.withTrimmedTop (area.getHeight() / 2 + 12).reduced (60, 0), juce::Justification::centredTop, 4);
    }
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();
    auto top = bounds.removeFromTop (56).reduced (12, 10);
    openButton.setBounds (top.removeFromLeft (100));
    top.removeFromLeft (8);
    saveButton.setBounds (top.removeFromLeft (150));
    top.removeFromLeft (8);
    midiButton.setBounds (top.removeFromLeft (120));
    top.removeFromLeft (8);
    saveAllButton.setBounds (top.removeFromLeft (100));
    top.removeFromLeft (8);
    revealButton.setBounds (top.removeFromLeft (130));
    top.removeFromLeft (8);
    if (noteratorButton.isVisible()) noteratorButton.setBounds (top.removeFromLeft (150));
    playButton.setBounds (top.removeFromRight (90));
    top.removeFromRight (8);
    chordsButton.setBounds (top.removeFromRight (90));

    const bool empty = jobs.isEmpty();
    fileList.setVisible (! empty);
    issueList.setVisible (! empty);
    viewport.setVisible (! empty);
    prevPage.setVisible (! empty);
    nextPage.setVisible (! empty);
    if (empty) return;

    auto left = bounds.removeFromLeft (320);
    fileList.setBounds (left.reduced (10));

    auto header = bounds.removeFromTop (64);
    auto nav = header.reduced (16, 8).removeFromTop (24).removeFromRight (56);
    prevPage.setBounds (nav.removeFromLeft (26));
    nav.removeFromLeft (4);
    nextPage.setBounds (nav.removeFromLeft (26));

    auto* j = job();
    const int issues = j != nullptr && j->done ? static_cast<int> (j->result.read.issues.size()) : 0;
    const int issueHeight = issues == 0 ? 0 : juce::jmin (180, 8 + issues * issueList.getRowHeight());
    issueList.setBounds (bounds.removeFromBottom (issueHeight).reduced (10, 4));
    issueList.setVisible (issues > 0);
    viewport.setBounds (bounds.reduced (10, 4));
    const int w = viewport.getWidth() - viewport.getScrollBarThickness();
    preview.setSize (w, preview.preferredHeight (w));
}

bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (juce::File (f).hasFileExtension ("pdf") || juce::File (f).isDirectory()) return true;
    return false;
}

void MainComponent::filesDropped (const juce::StringArray& files, int, int)
{
    dragging = false;
    addFiles (files);
}

void MainComponent::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= jobs.size()) return;
    const auto& j = *jobs[row];
    if (selected)
    {
        g.setColour (theme::rule);
        g.fillRect (0, 0, w, h);
        g.setColour (theme::accent);
        g.fillRect (0, 0, 4, h);
    }
    g.setColour (theme::text);
    g.setFont (font (15, true));
    g.drawText (j.pdf.getFileNameWithoutExtension(), 14, 6, w - 20, 20, juce::Justification::centredLeft, true);
    g.setColour (verdictColour (j));
    g.setFont (font (13));
    g.drawText (verdictText (j), 14, 28, w - 20, 18, juce::Justification::centredLeft, true);
}

void MainComponent::selectedRowsChanged (int row)
{
    if (row != current && row >= 0) selectFile (row);
}

int MainComponent::IssueModel::getNumRows()
{
    auto* j = owner.job();
    return j != nullptr && j->done ? static_cast<int> (j->result.read.issues.size()) : 0;
}

void MainComponent::IssueModel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    auto* j = owner.job();
    if (j == nullptr || row >= static_cast<int> (j->result.read.issues.size())) return;
    const auto& is = j->result.read.issues[static_cast<size_t> (row)];
    if (selected) { g.setColour (theme::rule); g.fillRect (0, 0, w, h); }
    juce::String where;
    if (is.measureIndex >= 0)
    {
        where = "Bar " + juce::String (is.measureIndex + 1);
        const auto& parts = j->result.read.score.parts;
        if (is.part >= 0 && is.part < static_cast<int> (parts.size()))
            where += ", " + (parts[static_cast<size_t> (is.part)].name.empty() ? "Part " + juce::String (is.part + 1)
                                                                                 : juce::String::fromUTF8 (parts[static_cast<size_t> (is.part)].name.c_str()));
    }
    else if (is.page >= 0)
        where = "Page " + juce::String (is.page + 1);
    g.setColour (is.level == Issue::Error ? theme::warnText : theme::textDim);
    g.setFont (font (13, true));
    g.drawText (where, 8, 0, 170, h, juce::Justification::centredLeft, true);
    g.setColour (theme::text);
    g.setFont (font (13));
    g.drawText (juce::String::fromUTF8 (is.message.c_str()), 182, 0, w - 190, h, juce::Justification::centredLeft, true);
}

void MainComponent::IssueModel::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    owner.showIssue (row);
}

void MainComponent::saveCurrent()
{
    auto* j = job();
    if (j == nullptr || ! j->done || j->result.musicXml.empty()) return;
    chooser = std::make_unique<juce::FileChooser> ("Save MusicXML", j->pdf.withFileExtension ("musicxml"), "*.musicxml");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, j] (const juce::FileChooser& fc) {
                              const auto f = fc.getResult();
                              if (f == juce::File()) return;
                              if (f.replaceWithText (juce::String::fromUTF8 (j->result.musicXml.c_str()), false, false, "\n"))
                                  j->saved = f;
                              else
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                      juce::String::fromUTF8 ("\xe2\x80\x9c") + f.getFileName() + juce::String::fromUTF8 ("\xe2\x80\x9d couldn\xe2\x80\x99t be saved."),
                                      "Check that the folder can be written to, or choose another.");
                              refresh();
                          });
}

void MainComponent::saveAll()
{
    int count = 0;
    for (auto* j : jobs)
        if (j->done && ! j->result.musicXml.empty()) ++count;
    if (count == 0) return;
    auto options = juce::MessageBoxOptions()
                       .withIconType (juce::MessageBoxIconType::QuestionIcon)
                       .withTitle ("Save " + juce::String (count) + (count == 1 ? " MusicXML file" : " MusicXML files") + " next to the PDFs?")
                       .withMessage ("Each is saved in its PDF's folder, with the same name. A file already there with that name is replaced.")
                       .withButton ("Save")
                       .withButton ("Cancel");
    juce::AlertWindow::showAsync (options, [this] (int result) {
        if (result != 1) return;
        int failed = 0;
        for (auto* j : jobs)
        {
            if (! j->done || j->result.musicXml.empty()) continue;
            const auto f = j->pdf.withFileExtension ("musicxml");
            if (f.replaceWithText (juce::String::fromUTF8 (j->result.musicXml.c_str()), false, false, "\n")) j->saved = f;
            else ++failed;
        }
        if (failed > 0)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                    juce::String (failed) + (failed == 1 ? " file couldn't be saved." : " files couldn't be saved."),
                                                    "Check that their folders can be written to, or save them one at a time.");
        refresh();
    });
}

juce::File MainComponent::noteratorApp() const
{
#if JUCE_MAC
    for (auto f : { juce::File ("/Applications/Noterator.app"),
                    juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Applications/Noterator.app") })
        if (f.exists()) return f;
#endif
    return {};
}

void MainComponent::openInNoterator()
{
    auto* j = job();
    const auto app = noteratorApp();
    if (j == nullptr || ! app.exists() || j->result.musicXml.empty()) return;
    auto f = j->saved;
    if (! f.existsAsFile())
    {
        f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (j->pdf.getFileNameWithoutExtension() + ".musicxml");
        f.replaceWithText (juce::String::fromUTF8 (j->result.musicXml.c_str()), false, false, "\n");
    }
    juce::ChildProcess open;
    open.start (juce::StringArray { "/usr/bin/open", "-a", app.getFullPathName(), f.getFullPathName() });
    open.waitForProcessToFinish (5000);
}

} // namespace ans

namespace ans
{

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey) { togglePlay(); return true; }
    return false;
}

void MainComponent::togglePlay()
{
    if (player != nullptr && player->isPlaying())
    {
        player->stop();
        stopTimer();
        playButton.setButtonText ("Play");
        playingBar = -1;
        preview.showPlaying ({});
        return;
    }
    auto* j = job();
    if (j == nullptr || ! j->done || j->result.verdict == Conversion::Rejected) return;
    if (player == nullptr) player = std::make_unique<Player>();
    performance = perform (j->result.read.score);
    const double from = 0;
    player->play (performance, from);
    playButton.setButtonText ("Stop");
    playButton.setTooltip ("Playing through " + player->soundName() + ". Click to stop.");
    startTimerHz (30);
}

void MainComponent::timerCallback()
{
    auto* j = job();
    if (player == nullptr || j == nullptr || ! player->isPlaying())
    {
        stopTimer();
        playButton.setButtonText ("Play");
        playingBar = -1;
        preview.showPlaying ({});
        return;
    }
    const int bar = performance.barAt (performance.quarters (player->positionSeconds()));
    if (bar == playingBar) return;
    playingBar = bar;
    std::vector<Box> boxes;
    int page = -1;
    for (const auto& part : j->result.read.score.parts)
    {
        if (bar >= static_cast<int> (part.measures.size())) continue;
        for (const auto& place : part.measures[static_cast<size_t> (bar)].places)
        {
            if (page < 0) page = place.page;
            if (place.page == page) boxes.push_back (place.box);
        }
    }
    if (page < 0) return;
    if (page != currentPage) { currentPage = page; currentIssue = -1; refresh(); }
    preview.showPlaying (boxes);
    // keep the bar in view
    double top = 1e9;
    for (const auto& b : boxes) top = std::min (top, b.y0);
    const int y = static_cast<int> (top * preview.scaleToView());
    if (y < viewport.getViewPositionY() || y > viewport.getViewPositionY() + viewport.getHeight() * 2 / 3)
        viewport.setViewPosition (0, juce::jmax (0, y - viewport.getHeight() / 4));
}

void MainComponent::exportMidi()
{
    auto* j = job();
    if (j == nullptr || ! j->done || j->result.verdict == Conversion::Rejected) return;
    chooser = std::make_unique<juce::FileChooser> ("Export MIDI", j->pdf.withFileExtension ("mid"), "*.mid;*.midi");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, j] (const juce::FileChooser& fc) {
                              const auto f = fc.getResult();
                              if (f == juce::File()) return;
                              const auto bytes = toMidiFile (j->result.read.score, perform (j->result.read.score));
                              if (! f.replaceWithData (bytes.data(), bytes.size()))
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                      juce::String::fromUTF8 ("\xe2\x80\x9c") + f.getFileName() + juce::String::fromUTF8 ("\xe2\x80\x9d couldn\xe2\x80\x99t be saved."),
                                      "Check that the folder can be written to, or choose another.");
                          });
}

} // namespace ans
