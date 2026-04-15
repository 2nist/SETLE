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
        engine = std::make_unique<te::Engine> (getApplicationName());
        engine->getDeviceManager().initialise();
        mainWindow = std::make_unique<MainWindow> (getApplicationName(), *engine);
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
        MainWindow (const juce::String& name, te::Engine& engine)
            : DocumentWindow (name,
                              juce::Desktop::getInstance().getDefaultLookAndFeel()
                                  .findColour (juce::ResizableWindow::backgroundColourId),
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new setle::ui::WorkspaceShellComponent(engine), true);
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
