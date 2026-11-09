/*
    MainComponent - the one window.

    Left: the PDFs dropped on it, each with how it went (checked, needs a
    look, not converted). Right: the chosen PDF's page as printed, with every
    bar that needs a look outlined in red, the list of what to look at under
    it, and the buttons: save the MusicXML, show it in the Finder, open it in
    Noterator. Converting happens off the main thread, two files at a time.
*/

#pragma once

#include "Convert.h"
#include "Harmony.h"
#include "Midi.h"
#include "Player.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <atomic>
#include <map>
#include <memory>

namespace ans
{

struct Job
{
    juce::File pdf;
    std::atomic<bool> done { false };
    Conversion result;
    std::vector<OutlinedChord> chords;   // what each bar outlines: an analysis, not read from the page
    juce::File saved;          // where its MusicXML was saved, once it is
    int pages = 0;
};

class PagePreview : public juce::Component
{
public:
    void show (Job* job, int page, int selectedIssue);
    void showPlaying (std::vector<Box> boxes) { playing = std::move (boxes); repaint(); }
    void showChords (bool on) { chords = on; repaint(); }
    double scaleToView() const { return scale; }
    void paint (juce::Graphics&) override;
    void resized() override { image = {}; }
    void mouseUp (const juce::MouseEvent&) override;
    std::function<void (int issue)> onIssueClicked;
    int preferredHeight (int width) const;

private:
    Job* job = nullptr;
    int page = 0, selected = -1;
    std::vector<Box> playing;   // the bar being played, on this page
    bool chords = false;        // the chords each bar outlines, above it
    juce::Image image;
    double scale = 1.0;
    void render();
};

class MainComponent : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      public juce::MenuBarModel,
                      private juce::ListBoxModel,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    // The menu bar (decision 0014): File, View, Play.
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int index, const juce::String&) override;
    void menuItemSelected (int id, int) override;
    // Once the window is up: its smallest size, for the interface size in use.
    void windowReady();
    // For checking the window: an interface size from the command line (not saved).
    void showInterfaceSize (float scale) { juce::Desktop::getInstance().setGlobalScaleFactor (scale); windowReady(); }

    void addFiles (const juce::StringArray& paths);
    void showChords (bool on) { chordsButton.setToggleState (on, juce::dontSendNotification); preview.showChords (on); }

    void paint (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragging = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragging = false; repaint(); }

    // For checking the window without a mouse: wait for every file, then
    // draw the window into a PNG.
    bool allDone() const;
    void selectFile (int index);
    void play() { if (player == nullptr || ! player->isPlaying()) togglePlay(); }
    bool keyPressed (const juce::KeyPress&) override;

private:
    juce::OwnedArray<Job> jobs;
    juce::ThreadPool pool { 2 };
    int current = -1, currentPage = 0, currentIssue = -1;
    bool dragging = false;

    juce::TextButton openButton { ui ("Open\xe2\x80\xa6") }, saveButton { ui ("Save MusicXML\xe2\x80\xa6") },
                     saveAllButton { ui ("Save All\xe2\x80\xa6") }, revealButton { "Show in Finder" }, noteratorButton { "Open in Noterator" },
                     playButton { "Play" }, chordsButton { "Chords" }, midiButton { ui ("Export MIDI\xe2\x80\xa6") },
                     prevPage { ui ("\xe2\x80\xb9") }, nextPage { ui ("\xe2\x80\xba") };
    juce::ListBox fileList { "Files", this };

    struct IssueModel : juce::ListBoxModel
    {
        MainComponent& owner;
        explicit IssueModel (MainComponent& o) : owner (o) {}
        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;
        juce::String getNameForRow (int row) override;   // what VoiceOver reads
        juce::String getTooltipForRow (int row) override { return getNameForRow (row); }   // the whole message, where the row cuts it short
    } issueModel { *this };
    juce::ListBox issueList { "Issues", &issueModel };

    PagePreview preview;
    juce::Viewport viewport;
    std::unique_ptr<juce::FileChooser> chooser;

    Job* job() const { return current >= 0 && current < jobs.size() ? jobs[current] : nullptr; }
    void refresh();
    void showIssue (int issue);
    void saveCurrent();
    void saveAll();
    void openInNoterator();
    void togglePlay();
    void exportMidi();
    void timerCallback() override;
    std::unique_ptr<Player> player;
    Performance performance;
    int playingBar = -1;
    juce::File noteratorApp() const;
    static juce::String ui (const char* s) { return juce::String::fromUTF8 (s); }

    // Interface size (View menu), the Mac's Increase Contrast, remembered settings.
    juce::ApplicationProperties settings;
    static const std::vector<float>& interfaceScales();
    void setInterfaceScale (float scale);
    void readSystemSettings();
    struct SettingsWatch : juce::Timer
    {
        MainComponent& owner;
        explicit SettingsWatch (MainComponent& o) : owner (o) {}
        void timerCallback() override { owner.readSystemSettings(); }
    } settingsWatch { *this };
    void openFiles();
    void setChords (bool on);
    void updateTitle();
    juce::String issueWhere (const Job&, const Issue&) const;
    juce::String partLabel (const Job&, int part) const;
    juce::String whoPlays (const Job&) const;
    // The layout, worked out in resized() for the width and interface size:
    // the toolbar folds into rows and the header grows rather than cropping.
    int toolbarHeight = 56, listWidth = 320, headerHeight = 84;
    int summaryLines = 1, whoLines = 1;
    juce::Rectangle<int> headerTextArea() const;

    int getNumRows() override { return jobs.size(); }
    juce::String getNameForRow (int row) override;
    juce::String getTooltipForRow (int row) override { return getNameForRow (row); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void selectedRowsChanged (int row) override;
};

} // namespace ans
