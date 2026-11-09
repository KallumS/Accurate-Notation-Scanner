#include "MainComponent.h"

#include "Instruments.h"
#include "PdfReader.h"
#include "SystemSettings.h"
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
    return theme::textDim;
}

enum MenuIds
{
    menuOpen = 1, menuSave, menuExportMidi, menuSaveAll, menuReveal, menuNoterator,
    menuChords, menuPrevPage, menuNextPage, menuSizeBigger, menuSizeSmaller, menuSizeActual,
    menuPlay,
    menuScaleBase = 100
};

// "Cmd+O" as the Mac writes it in a menu.
juce::String keyText (const juce::String& s)
{
   #if JUCE_MAC
    return s.replace ("Cmd+", juce::String::fromUTF8 ("\xe2\x8c\x98")).replace ("Shift+", juce::String::fromUTF8 ("\xe2\x87\xa7"));
   #else
    return s.replace ("Cmd+", "Ctrl+");
   #endif
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
        std::map<int, float> rightEdge;   // per line of labels: where the last one ended, so none covers another
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
            float x = area.getX() + static_cast<float> ((place->box.x0 + share * place->box.width()) * scale) + 3.0f;
            const float y = area.getY() + static_cast<float> (place->box.y0 * scale) - 18.0f;
            const float w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 8.0f;
            auto& edge = rightEdge[juce::roundToInt (y)];
            x = std::max (x, edge + 3.0f);
            edge = x + w;
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
    playButton.setTooltip (ui ("Play the score through the Mac\xe2\x80\x99s built-in instruments (Space)"));
    midiButton.setTooltip ("Save the chosen score as a MIDI file");
    playButton.onClick = [this] { togglePlay(); };
    chordsButton.setClickingTogglesState (true);
    chordsButton.setTooltip ("Name the chords each bar outlines, above the bar (an analysis of the notes, not read from the page)");
    chordsButton.onClick = [this] { setChords (chordsButton.getToggleState()); };
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

    // Names for VoiceOver where the face is not a word (decision 0014)
    prevPage.setTitle ("Previous Page");
    nextPage.setTitle ("Next Page");
    prevPage.setTooltip ("Previous page");
    nextPage.setTooltip ("Next page");
    preview.setTitle ("Page");
    fileList.setTitle ("Scores");
    issueList.setTitle ("Bars that need a look");

    openButton.onClick = [this] { openFiles(); };
    saveButton.onClick = [this] { saveCurrent(); };
    saveAllButton.onClick = [this] { saveAll(); };
    revealButton.onClick = [this] {
        if (auto* j = job()) (j->saved.existsAsFile() ? j->saved : j->pdf).revealToUser();
    };
    noteratorButton.onClick = [this] { openInNoterator(); };
    prevPage.onClick = [this] { if (currentPage > 0) { --currentPage; currentIssue = -1; refresh(); } };
    nextPage.onClick = [this] { if (auto* j = job()) if (currentPage + 1 < j->pages) { ++currentPage; currentIssue = -1; refresh(); } };
    preview.onIssueClicked = [this] (int i) { showIssue (i); };

    juce::PropertiesFile::Options prefs;
    prefs.applicationName = "Accurate Notation Scanner";
    prefs.filenameSuffix = "settings";
    prefs.osxLibrarySubFolder = "Application Support";
    settings.setStorageParameters (prefs);
    if (auto* p = settings.getUserSettings())
    {
        const auto scale = static_cast<float> (p->getDoubleValue ("interfaceScale", 1.0));
        for (auto s : interfaceScales())
            if (std::abs (s - scale) < 0.01f) juce::Desktop::getInstance().setGlobalScaleFactor (s);
        setChords (p->getBoolValue ("chords", false));
    }
    readSystemSettings();
    settingsWatch.startTimer (1500);

    setWantsKeyboardFocus (true);
    // Clicking a button leaves the keyboard with the window, so Space still
    // plays; Tab reaches every control (decision 0014).
    theme::clicksKeepFocus (*this);
    const float scale = juce::Desktop::getInstance().getGlobalScaleFactor();
    setSize (juce::roundToInt (1180.0f / scale), juce::roundToInt (820.0f / scale));
   #if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu (this);
   #endif
    refresh();
}

void MainComponent::openFiles()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose PDF scores", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.pdf");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
                          [this] (const juce::FileChooser& fc) {
                              juce::StringArray paths;
                              for (const auto& f : fc.getResults()) paths.add (f.getFullPathName());
                              addFiles (paths);
                          });
}

void MainComponent::setChords (bool on)
{
    chordsButton.setToggleState (on, juce::dontSendNotification);
    chordsButton.setTooltip (on ? "The chords each bar outlines are shown above the bars (an analysis of the notes, not read from the page). Click to hide them."
                                : "Show the chords each bar outlines above the bars (an analysis of the notes, not read from the page)");
    preview.showChords (on);
    if (auto* p = settings.getUserSettings()) p->setValue ("chords", on);
    menuItemsChanged();
}

const std::vector<float>& MainComponent::interfaceScales()
{
    static const std::vector<float> s { 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };
    return s;
}

// Noterator's windowReady and setInterfaceScale: the smallest window shrinks
// as the interface grows, so 200% still fits a laptop.
void MainComponent::windowReady()
{
    auto* w = findParentComponentOfClass<juce::ResizableWindow>();
    if (w == nullptr) return;
    const float scale = juce::Desktop::getInstance().getGlobalScaleFactor();
    w->setResizeLimits (juce::roundToInt (820.0f / scale), juce::roundToInt (560.0f / scale), 10000, 10000);
    if (const auto* d = juce::Desktop::getInstance().getDisplays().getDisplayForRect (w->getScreenBounds()))
    {
        const auto area = d->userBounds.toNearestInt();
        auto b = w->getBounds();
        b.setSize (std::min (b.getWidth(), area.getWidth()), std::min (b.getHeight(), area.getHeight()));
        w->setBounds (b.constrainedWithin (area));
    }
}

void MainComponent::setInterfaceScale (float scale)
{
    auto& desktop = juce::Desktop::getInstance();
    const float old = desktop.getGlobalScaleFactor();
    if (std::abs (old - scale) < 0.01f) return;
    auto* w = findParentComponentOfClass<juce::ResizableWindow>();
    const auto before = w != nullptr ? w->getBounds().toFloat() * old : juce::Rectangle<float>();
    desktop.setGlobalScaleFactor (scale);
    if (auto* p = settings.getUserSettings()) p->setValue ("interfaceScale", static_cast<double> (scale));
    if (w != nullptr)
    {
        w->setResizeLimits (100, 100, 10000, 10000);
        w->setBounds ((before / scale).toNearestInt());
        windowReady();
    }
    menuItemsChanged();
}

void MainComponent::readSystemSettings()
{
    const bool contrast = system::increaseContrast();
    if (contrast == theme::increasedContrast) return;
    theme::useIncreasedContrast (contrast);
    repaint();
    fileList.repaint();
    issueList.repaint();
}

void MainComponent::updateTitle()
{
    auto* w = findParentComponentOfClass<juce::DocumentWindow>();
    if (w == nullptr) return;
    // As a Mac document window: the score's name, which VoiceOver reads too
    auto* j = job();
    w->setName (j != nullptr ? j->pdf.getFileName() : juce::String (JUCE_APPLICATION_NAME_STRING));
}

juce::StringArray MainComponent::getMenuBarNames() { return { "File", "View", "Play" }; }

juce::PopupMenu MainComponent::getMenuForIndex (int index, const juce::String&)
{
    juce::PopupMenu m;
    auto item = [&m] (int id, const juce::String& text, const juce::String& shortcut = {}, bool enabled = true, bool ticked = false)
    {
        juce::PopupMenu::Item i (text);
        i.itemID = id;
        i.isEnabled = enabled;
        i.isTicked = ticked;
        if (shortcut.isNotEmpty()) i.shortcutKeyDescription = keyText (shortcut);
        m.addItem (i);
    };
    auto* j = job();
    const bool ready = j != nullptr && j->done && j->result.verdict != Conversion::Rejected;
    if (index == 0)
    {
        item (menuOpen, ui ("Open\xe2\x80\xa6"), "Cmd+O");
        m.addSeparator();
        item (menuSave, ui ("Save MusicXML\xe2\x80\xa6"), "Cmd+S", ready);
        item (menuExportMidi, ui ("Export MIDI\xe2\x80\xa6"), "Cmd+E", ready);
        item (menuSaveAll, ui ("Save All\xe2\x80\xa6"), "Cmd+Shift+S", saveAllButton.isEnabled());
        m.addSeparator();
        item (menuReveal, "Show in Finder", {}, j != nullptr);
        if (noteratorApp().exists()) item (menuNoterator, "Open in Noterator", {}, ready);
    }
    else if (index == 1)
    {
        item (menuChords, "Show Chords", "Cmd+K", true, chordsButton.getToggleState());
        m.addSeparator();
        item (menuPrevPage, "Previous Page", "Cmd+[", prevPage.isEnabled());
        item (menuNextPage, "Next Page", "Cmd+]", nextPage.isEnabled());
        m.addSeparator();
        juce::PopupMenu sizes;
        const float now = juce::Desktop::getInstance().getGlobalScaleFactor();
        for (size_t i = 0; i < interfaceScales().size(); ++i)
            sizes.addItem (menuScaleBase + static_cast<int> (i), juce::String (juce::roundToInt (interfaceScales()[i] * 100.0f)) + "%", true,
                           std::abs (interfaceScales()[i] - now) < 0.01f);
        sizes.addSeparator();
        juce::PopupMenu::Item bigger ("Bigger"); bigger.itemID = menuSizeBigger; bigger.shortcutKeyDescription = keyText ("Cmd+="); sizes.addItem (bigger);
        juce::PopupMenu::Item smaller ("Smaller"); smaller.itemID = menuSizeSmaller; smaller.shortcutKeyDescription = keyText ("Cmd+-"); sizes.addItem (smaller);
        juce::PopupMenu::Item actual ("Actual Size"); actual.itemID = menuSizeActual; actual.shortcutKeyDescription = keyText ("Cmd+0"); sizes.addItem (actual);
        m.addSubMenu ("Interface Size", sizes);
    }
    else
    {
        const bool playing = player != nullptr && player->isPlaying();
        item (menuPlay, playing ? "Stop" : "Play", "Space", ready || playing);
    }
    return m;
}

void MainComponent::menuItemSelected (int id, int)
{
    const auto& scales = interfaceScales();
    const float now = juce::Desktop::getInstance().getGlobalScaleFactor();
    size_t at = 0;
    for (size_t i = 0; i < scales.size(); ++i) if (std::abs (scales[i] - now) < 0.01f) at = i;
    if (id >= menuScaleBase && id < menuScaleBase + static_cast<int> (scales.size())) { setInterfaceScale (scales[static_cast<size_t> (id - menuScaleBase)]); return; }
    switch (id)
    {
        case menuOpen: openFiles(); break;
        case menuSave: saveCurrent(); break;
        case menuExportMidi: exportMidi(); break;
        case menuSaveAll: saveAll(); break;
        case menuReveal: revealButton.triggerClick(); break;
        case menuNoterator: openInNoterator(); break;
        case menuChords: setChords (! chordsButton.getToggleState()); break;
        case menuPrevPage: prevPage.triggerClick(); break;
        case menuNextPage: nextPage.triggerClick(); break;
        case menuSizeBigger: setInterfaceScale (scales[std::min (at + 1, scales.size() - 1)]); break;
        case menuSizeSmaller: setInterfaceScale (scales[at > 0 ? at - 1 : 0]); break;
        case menuSizeActual: setInterfaceScale (1.0f); break;
        case menuPlay: togglePlay(); break;
        default: break;
    }
}

MainComponent::~MainComponent()
{
   #if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu (nullptr);
   #endif
    settingsWatch.stopTimer();
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
    preview.setDescription (j == nullptr ? juce::String() : j->pdf.getFileName() + ", page " + juce::String (currentPage + 1) + " of " + juce::String (j->pages)
                                                             + (j->done ? ". " + juce::String::fromUTF8 (j->result.summary().c_str()) : juce::String()));
    updateTitle();
    menuItemsChanged();
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
    g.setColour (theme::rule);
    g.fillRect (0, toolbarHeight - 1, getWidth(), 1);

    if (auto* j = job())
    {
        auto header = headerTextArea();
        g.setColour (theme::text);
        g.setFont (font (17, true));
        g.drawText (j->pdf.getFileName(), header.removeFromTop (24), juce::Justification::centredLeft, true);
        g.setColour (j->done && j->result.verdict == Conversion::Rejected ? theme::warnText : theme::textDim);
        g.setFont (font (14));
        g.drawFittedText (j->done ? juce::String::fromUTF8 (j->result.summary().c_str()) : verdictText (*j),
                          header.removeFromTop (18 * summaryLines), juce::Justification::topLeft, summaryLines, 1.0f);
        // who plays: each part's instrument, and how a transposing one is written
        if (j->done && j->result.verdict != Conversion::Rejected)
        {
            g.setFont (font (13));
            g.setColour (theme::textDim);
            g.drawFittedText (whoPlays (*j), header.withTrimmedTop (2).removeFromTop (17 * whoLines), juce::Justification::topLeft, whoLines, 1.0f);
        }
        if (j->pages > 0)
        {
            g.setColour (theme::textDim);
            g.setFont (font (13));
            const auto nav = prevPage.getBounds();
            g.drawText ("Page " + juce::String (currentPage + 1) + " of " + juce::String (j->pages),
                        juce::Rectangle<int> (nav.getX() - 112, nav.getY(), 104, nav.getHeight()), juce::Justification::centredRight);
        }
    }

    if (jobs.isEmpty() || dragging)
    {
        auto area = getLocalBounds().withTrimmedTop (toolbarHeight).reduced (40);
        g.setColour (dragging ? theme::accent : theme::rule);
        g.drawRoundedRectangle (area.toFloat(), 12.0f, dragging ? 3.0f : 2.0f);
        g.setColour (theme::text);
        g.setFont (font (26, true));
        g.drawText (ui ("Drop PDF scores here, or choose Open\xe2\x80\xa6"), area.withTrimmedBottom (area.getHeight() / 2), juce::Justification::centredBottom);
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
    // The toolbar: buttons as wide as their words, 12 apart (Apple's spacing),
    // folding into a second row when the window is narrow or the interface
    // large, rather than running off the edge (decision 0014).
    {
        const auto buttonFont = font (15);
        auto widthOf = [&] (juce::TextButton& b) {
            return std::max (90, juce::roundToInt (juce::GlyphArrangement::getStringWidth (buttonFont, b.getButtonText())) + 36);
        };
        std::vector<juce::TextButton*> left { &openButton, &saveButton, &midiButton, &saveAllButton, &revealButton };
        if (noteratorButton.isVisible()) left.push_back (&noteratorButton);
        const std::vector<juce::TextButton*> right { &chordsButton, &playButton };
        const int gap = 12, h = 36, margin = 12, avail = getWidth() - 2 * margin;
        int rightWidth = 0;
        for (auto* b : right) rightWidth += widthOf (*b) + gap;
        int x = margin, y = 10;
        for (auto* b : left)
        {
            const int w = widthOf (*b);
            if (x > margin && x + w > margin + avail - (y == 10 ? rightWidth : 0)) { x = margin; y += h + gap; }
            b->setBounds (x, y, w, h);
            x += w + gap;
        }
        // Chords and Play at the right of the first row, or of the next one if they no longer fit
        int rx = getWidth() - margin, ry = 10;
        if (y == 10 && x + rightWidth > getWidth() - margin) ry = y + h + gap;
        else if (y > 10 && x + rightWidth <= getWidth() - margin) ry = y;
        else if (y > 10) ry = y + h + gap;
        for (auto it = right.rbegin(); it != right.rend(); ++it)
        {
            const int w = widthOf (**it);
            rx -= w;
            (*it)->setBounds (rx, ry, w, h);
            rx -= gap;
        }
        toolbarHeight = std::max (y, ry) + h + 10;
    }

    auto bounds = getLocalBounds().withTrimmedTop (toolbarHeight);
    listWidth = juce::jlimit (150, 320, getWidth() / 4);

    // The header grows with its text instead of cutting it short.
    if (auto* j = job())
    {
        const int w = std::max (100, getWidth() - listWidth - 32 - 190);
        auto lines = [w] (const juce::String& t, const juce::Font& f, int most) {
            const float width = juce::GlyphArrangement::getStringWidth (f, t);
            return juce::jlimit (1, most, static_cast<int> (std::ceil (width / static_cast<float> (w))));
        };
        summaryLines = lines (j->done ? juce::String::fromUTF8 (j->result.summary().c_str()) : verdictText (*j), font (14), 3);
        whoLines = j->done && j->result.verdict != Conversion::Rejected ? lines (whoPlays (*j), font (13), 4) : 0;
        headerHeight = 16 + 24 + 18 * summaryLines + (whoLines > 0 ? 2 + 17 * whoLines : 0);
    }
    const bool empty = jobs.isEmpty();
    fileList.setVisible (! empty);
    issueList.setVisible (! empty);
    viewport.setVisible (! empty);
    prevPage.setVisible (! empty);
    nextPage.setVisible (! empty);
    if (empty) return;

    auto left = bounds.removeFromLeft (listWidth);
    fileList.setBounds (left.reduced (10));

    auto header = bounds.removeFromTop (headerHeight);
    auto nav = header.reduced (16, 6).removeFromTop (28).removeFromRight (64);
    prevPage.setBounds (nav.removeFromLeft (28));
    nav.removeFromLeft (8);
    nextPage.setBounds (nav.removeFromLeft (28));

    auto* j = job();
    const int issues = j != nullptr && j->done ? static_cast<int> (j->result.read.issues.size()) : 0;
    const int issueHeight = issues == 0 ? 0 : juce::jmin (180, 8 + issues * issueList.getRowHeight());
    // the bars that need a look above the page, not along the window's
    // bottom edge, which is often below the screen (decision 0014)
    issueList.setBounds (bounds.removeFromTop (issueHeight).reduced (10, 4));
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
    const auto where = owner.issueWhere (*j, is);
    g.setColour (is.level == Issue::Error ? theme::warnText : theme::textDim);
    g.setFont (font (13, true));
    g.drawText (where, 8, 0, 170, h, juce::Justification::centredLeft, true);
    g.setColour (theme::text);
    g.setFont (font (13));
    g.drawText (juce::String::fromUTF8 (is.message.c_str()), 182, 0, w - 190, h, juce::Justification::centredLeft, true);
}

juce::String MainComponent::partLabel (const Job& j, int p) const
{
    const auto& parts = j.result.read.score.parts;
    if (p < 0 || p >= static_cast<int> (parts.size())) return {};
    const auto& part = parts[static_cast<size_t> (p)];
    if (! part.name.empty()) return juce::String::fromUTF8 (part.name.c_str());
    if (const auto* inst = instruments::byId (part.instrument)) return juce::String::fromUTF8 (inst->name.c_str());
    return "Part " + juce::String (p + 1);
}

juce::String MainComponent::whoPlays (const Job& j) const
{
    juce::StringArray who;
    const auto& parts = j.result.read.score.parts;
    for (size_t p = 0; p < parts.size(); ++p)
    {
        const auto& part = parts[p];
        const auto* inst = instruments::byId (part.instrument);
        auto s = partLabel (j, static_cast<int> (p));
        // the instrument where the printed name is not plainly it ("Corni in F (horn)")
        if (inst != nullptr && ! s.containsIgnoreCase (juce::String (inst->name).upToFirstOccurrenceOf (" ", false, false)))
            s << " (" << juce::String::fromUTF8 (inst->name.c_str()).toLowerCase() << ")";
        if (part.transpose % 12 != 0) s << ui (" \xe2\x80\x93 played at concert pitch");
        else if (part.instrumentNote.find ("concert pitch") != std::string::npos) s << ui (" \xe2\x80\x93 written at concert pitch");
        else if (part.instrumentNote.find ("could not be told") != std::string::npos) s << ui (" \xe2\x80\x93 played as written");
        who.add (s);
    }
    return who.joinIntoString (ui ("  \xc2\xb7  "));
}

juce::Rectangle<int> MainComponent::headerTextArea() const
{
    // left of the page count and arrows
    return juce::Rectangle<int> (listWidth, toolbarHeight, getWidth() - listWidth, headerHeight).reduced (16, 8).withTrimmedRight (190);
}

juce::String MainComponent::issueWhere (const Job& j, const Issue& is) const
{
    juce::String where;
    if (is.measureIndex >= 0)
    {
        where = "Bar " + juce::String (is.measureIndex + 1);
        if (is.part >= 0 && is.part < static_cast<int> (j.result.read.score.parts.size()))
            where += ", " + partLabel (j, is.part);
    }
    else if (is.page >= 0)
        where = "Page " + juce::String (is.page + 1);
    return where;
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
                                                    juce::String (failed) + ui (failed == 1 ? " file couldn\xe2\x80\x99t be saved." : " files couldn\xe2\x80\x99t be saved."),
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
    const auto mods = key.getModifiers();
    const int code = key.getKeyCode();
    if (key == juce::KeyPress::spaceKey) { togglePlay(); return true; }
    // Esc hands the keyboard back to the window from a control or a list
    if (key == juce::KeyPress::escapeKey) { grabKeyboardFocus(); return true; }
    if (mods.isCommandDown())
    {
        const int lower = juce::CharacterFunctions::toLowerCase (static_cast<juce::juce_wchar> (code));
        if (lower == 'o') { menuItemSelected (menuOpen, 0); return true; }
        if (lower == 's') { menuItemSelected (mods.isShiftDown() ? menuSaveAll : menuSave, 0); return true; }
        if (lower == 'e') { menuItemSelected (menuExportMidi, 0); return true; }
        if (lower == 'k') { menuItemSelected (menuChords, 0); return true; }
        if (code == '[') { menuItemSelected (menuPrevPage, 0); return true; }
        if (code == ']') { menuItemSelected (menuNextPage, 0); return true; }
        if (code == '=' || code == '+') { menuItemSelected (menuSizeBigger, 0); return true; }
        if (code == '-') { menuItemSelected (menuSizeSmaller, 0); return true; }
        if (code == '0') { menuItemSelected (menuSizeActual, 0); return true; }
    }
    return false;
}

juce::String MainComponent::getNameForRow (int row)
{
    if (row < 0 || row >= jobs.size()) return {};
    return jobs[row]->pdf.getFileNameWithoutExtension() + ": " + verdictText (*jobs[row]);
}

juce::String MainComponent::IssueModel::getNameForRow (int row)
{
    auto* j = owner.job();
    if (j == nullptr || row < 0 || row >= static_cast<int> (j->result.read.issues.size())) return {};
    const auto& is = j->result.read.issues[static_cast<size_t> (row)];
    return owner.issueWhere (*j, is) + ": " + juce::String::fromUTF8 (is.message.c_str());
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
    playButton.setTooltip ("Playing through " + player->soundName() + ". Click to stop (Space).");
    menuItemsChanged();
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
