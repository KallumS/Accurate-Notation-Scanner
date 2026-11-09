/*
    Accurate Notation Scanner - the application: one window, the house look,
    and PDFs handed over by the Finder (dropped on the Dock icon, or "Open
    With") or on the command line.

    --screenshot out.png [--select N] [--play seconds] file.pdf... opens the
    files, waits for them to be
    read, draws the window into a PNG and quits: how the window is checked
    without a screen (decision 0007).
*/

#include "MainComponent.h"
#include "Theme.h"

namespace ans
{

class MainWindow : public juce::DocumentWindow
{
public:
    explicit MainWindow (const juce::String& name)
        : juce::DocumentWindow (name, theme::ground, juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        content = new MainComponent();
        setContentOwned (content, true);
        setResizable (true, true);
        setResizeLimits (820, 560, 4000, 4000);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

    MainComponent* content = nullptr;
};

class Application : public juce::JUCEApplication, private juce::Timer
{
public:
    const juce::String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String& commandLine) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        tooltips = std::make_unique<juce::TooltipWindow> (nullptr, 600);
        window = std::make_unique<MainWindow> (getApplicationName());

        auto args = juce::StringArray::fromTokens (commandLine, true);
        args.trim();
        args.removeEmptyStrings();
        for (auto& a : args) a = a.unquoted();
        const int shot = args.indexOf ("--screenshot");
        if (shot >= 0 && shot + 1 < args.size())
        {
            screenshot = juce::File::getCurrentWorkingDirectory().getChildFile (args[shot + 1]);
            if (args.indexOf ("--play") >= 0) { playFor = args[args.indexOf ("--play") + 1].getIntValue(); args.removeRange (args.indexOf ("--play"), 2); }
            const int select = args.indexOf ("--select");
            if (select >= 0 && select + 1 < args.size()) selectAfter = args[select + 1].getIntValue();
            args.removeRange (shot, 2);
            if (args.indexOf ("--select") >= 0) args.removeRange (args.indexOf ("--select"), 2);
            startTimer (300);
        }
        juce::StringArray files;
        for (const auto& a : args)
            if (! a.startsWith ("-")) files.add (juce::File::getCurrentWorkingDirectory().getChildFile (a).getFullPathName());
        if (! files.isEmpty()) window->content->addFiles (files);
    }

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        auto args = juce::StringArray::fromTokens (commandLine, true);
        juce::StringArray files;
        for (auto a : args)
        {
            a = a.unquoted();
            if (a.isNotEmpty() && ! a.startsWith ("-")) files.add (a);
        }
        if (window != nullptr && ! files.isEmpty()) window->content->addFiles (files);
    }

    void shutdown() override
    {
        window = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override { quit(); }

private:
    theme::LookAndFeel lookAndFeel;
    std::unique_ptr<juce::TooltipWindow> tooltips;
    std::unique_ptr<MainWindow> window;
    juce::File screenshot;
    int selectAfter = -1;
    int playFor = 0;     // seconds of playback before the picture
    int waited = 0;

    void timerCallback() override
    {
        if (! window->content->allDone() && ++waited < 400) return;
        if (selectAfter >= 0 && waited < 1000)
        {
            window->content->selectFile (selectAfter);
            selectAfter = -1;
            waited = 1000;
            if (playFor > 0) window->content->play();
            return;
        }
        if (playFor > 0 && waited < 1000 + playFor * 1000 / 300) { ++waited; return; }
        stopTimer();
        auto image = window->content->createComponentSnapshot (window->content->getLocalBounds(), true, 1.0f);
        juce::FileOutputStream out (screenshot);
        if (out.openedOk())
        {
            out.setPosition (0);
            out.truncate();
            juce::PNGImageFormat().writeImageToStream (image, out);
        }
        quit();
    }
};

} // namespace ans

START_JUCE_APPLICATION (ans::Application)
