#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <tracktion_engine/tracktion_engine.h>

#include "ui/WorkspaceShellComponent.h"

namespace te = tracktion::engine;

//==============================================================================
class SetleApplication : public juce::JUCEApplication
{
public:
    SetleApplication() {}

    const juce::String getApplicationName() override
    {
        return JUCE_APPLICATION_NAME_STRING;
    }

    const juce::String getApplicationVersion() override
    {
        return JUCE_APPLICATION_VERSION_STRING;
    }

    bool moreThanOneInstanceAllowed() override { return false; }

    //==========================================================================
    void initialise (const juce::String&) override
    {
        // Tracktion Engine is the heart of SETLE.
        // The Engine object manages audio devices, plugin formats,
        // temporary files and the Edit data model.
        // One Engine per application — lives for the app lifetime.
        engine = std::make_unique<te::Engine> (getApplicationName());

        mainWindow = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        engine     = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted (const juce::String&) override {}

    //==========================================================================
    // Minimal main window — just proves the engine boots and a window opens.
    // Everything real gets built on top of this.
    struct MainWindow : public juce::DocumentWindow
    {
        MainWindow (const juce::String& name)
            : DocumentWindow (name,
                              juce::Desktop::getInstance().getDefaultLookAndFeel()
                                  .findColour (juce::ResizableWindow::backgroundColourId),
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new setle::ui::WorkspaceShellComponent(), true);
            setResizable (true, true);
            centreWithSize (1280, 800);
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    };

private:
    std::unique_ptr<te::Engine>    engine;
    std::unique_ptr<MainWindow>    mainWindow;
};

//==============================================================================
START_JUCE_APPLICATION (SetleApplication)
