#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <tracktion_engine/tracktion_engine.h>

#include "ui/WorkspaceShellComponent.h"

#if JUCE_WINDOWS
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <dbghelp.h>
 #pragma comment(lib, "dbghelp.lib")

static LONG WINAPI setleCrashHandler(EXCEPTION_POINTERS* ep)
{
    // Write a simple text crash log with the exception address and module
    char path[MAX_PATH] = {};
    DWORD len = ::GetTempPathA(MAX_PATH, path);
    if (len > 0 && len < MAX_PATH - 20)
        ::lstrcatA(path, "setle_crash.txt");
    else
        ::lstrcpyA(path, "C:\\setle_crash.txt");

    HANDLE f = ::CreateFileA(path, GENERIC_WRITE, 0, nullptr,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f != INVALID_HANDLE_VALUE)
    {
        char buf[512] = {};
        DWORD written = 0;
        const auto* er = ep->ExceptionRecord;
        const ULONG_PTR exAddr = reinterpret_cast<ULONG_PTR>(er->ExceptionAddress);

        // Find which module the exception occurred in
        char modPath[MAX_PATH] = {};
        HMODULE hMod = nullptr;
        ::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             reinterpret_cast<LPCSTR>(exAddr), &hMod);
        if (hMod)
            ::GetModuleFileNameA(hMod, modPath, MAX_PATH);

        const ULONG_PTR modBase = reinterpret_cast<ULONG_PTR>(hMod);
        const ULONG_PTR offset  = exAddr - modBase;

        ::wsprintfA(buf,
            "SETLE Crash\r\n"
            "ExceptionCode: 0x%08lX\r\n"
            "ExceptionAddress: 0x%016I64X\r\n"
            "Module: %s\r\n"
            "ModuleBase: 0x%016I64X\r\n"
            "Offset: 0x%016I64X\r\n",
            static_cast<unsigned long>(er->ExceptionCode),
            static_cast<unsigned long long>(exAddr),
            modPath,
            static_cast<unsigned long long>(modBase),
            static_cast<unsigned long long>(offset));

        ::WriteFile(f, buf, ::lstrlenA(buf), &written, nullptr);

        // Also walk the stack (up to 32 frames)
        void* stack[32] = {};
        const USHORT frames = ::CaptureStackBackTrace(0, 32, stack, nullptr);
        char line[256] = {};
        for (USHORT i = 0; i < frames; ++i)
        {
            const ULONG_PTR frameAddr = reinterpret_cast<ULONG_PTR>(stack[i]);
            HMODULE hFrameMod = nullptr;
            char frameMod[64] = "(unknown)";
            ::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                 GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                 reinterpret_cast<LPCSTR>(frameAddr), &hFrameMod);
            if (hFrameMod)
            {
                char tmp[MAX_PATH] = {};
                ::GetModuleFileNameA(hFrameMod, tmp, MAX_PATH);
                // extract just the filename
                const char* slash = tmp;
                for (const char* c = tmp; *c; ++c)
                    if (*c == '\\' || *c == '/') slash = c + 1;
                ::lstrcpynA(frameMod, slash, 63);
            }
            const ULONG_PTR fBase = reinterpret_cast<ULONG_PTR>(hFrameMod);
            ::wsprintfA(line, "  [%2u] %s + 0x%I64X\r\n",
                        (unsigned)i, frameMod,
                        static_cast<unsigned long long>(frameAddr - fBase));
            ::WriteFile(f, line, ::lstrlenA(line), &written, nullptr);
        }

        ::CloseHandle(f);
    }

    return EXCEPTION_CONTINUE_SEARCH;  // let Windows handle it normally
}
#endif

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
        const auto logFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                 .getChildFile("SETLE").getChildFile("startup.log");
        fileLogger = std::make_unique<juce::FileLogger>(logFile, "SETLE startup log");
        juce::Logger::setCurrentLogger(fileLogger.get());
        juce::Logger::writeToLog("=== SETLE initialise() begin ===");

        // Tracktion Engine is the heart of SETLE.
        // The Engine object manages audio devices, plugin formats,
        // temporary files and the Edit data model.
        // One Engine per application — lives for the app lifetime.
        juce::Logger::writeToLog("Creating Engine...");
        engine = std::make_unique<te::Engine> (getApplicationName());
        juce::Logger::writeToLog("Engine created. Initialising device manager...");
        engine->getDeviceManager().initialise();
        juce::Logger::writeToLog("Device manager initialised. Creating MainWindow...");

        mainWindow = std::make_unique<MainWindow> (getApplicationName(), *engine);
        juce::Logger::writeToLog("=== SETLE initialise() complete ===");
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        engine     = nullptr;
        juce::Logger::setCurrentLogger(nullptr);
        fileLogger.reset();
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
    std::unique_ptr<juce::FileLogger> fileLogger;
    std::unique_ptr<te::Engine>    engine;
    std::unique_ptr<MainWindow>    mainWindow;
};

//==============================================================================
#if JUCE_WINDOWS
// Install the crash handler as early as possible — via a global constructor.
struct SetleCrashHandlerInstaller
{
    SetleCrashHandlerInstaller()
    {
        ::SetUnhandledExceptionFilter(setleCrashHandler);
    }
};
static SetleCrashHandlerInstaller gCrashHandlerInstaller;
#endif

START_JUCE_APPLICATION (SetleApplication)
