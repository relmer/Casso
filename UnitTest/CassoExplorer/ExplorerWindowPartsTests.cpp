#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../CaptureTests/FakeHostDialogs.h"
#include "../CassoLink/FakeApple.h"
#include "../CassoLink/InMemoryCassoLinkFolder.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "FakeExplorerWindow.h"
#include "FakeFolderWatcher.h"
#include "RecordingExplorerServices.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "CassoExplorer/Window/ExplorerWindowParts.h"
#include "Core/AppMessages.h"
#include "Core/AppTimerIds.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerWindowPartsTests
//
//  The Casso Explorer window's creation, private messages and timers, run
//  through ExplorerWindowParts with no window: what the window sets up when
//  it opens, and where each of its messages goes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ExplorerWindowPartsTests)
{
public:

    struct Rig
    {
        InMemoryFileSystem                       fs;
        FakeDiskFileIo                           io;
        CassoExplorerBrowser                     browser { fs, io };
        CassoExplorerActions                     actions { browser, fs };
        CassoExplorerPrefs                       prefs;
        CassoExplorerWindowContext               app;
        FakeFolderWatcher                        watcher;
        FakeApple                                apple;
        FakeAppleFactory                         factory { apple };
        InMemoryCassoLinkFolder                  folder;
        FakeDiskFileIo                           linkIo;
        std::unique_ptr<CassoLinkConnections>    links;
        DxuiDpiScaler                            scaler;
        const DxuiTheme                        * theme   = nullptr;
        FakeExplorerWindow                       window;
        RecordingShellVerbs                    * verbs   = nullptr;
        RecordingInfoTips                      * tips    = nullptr;
        BlankShellIcons                        * icons   = nullptr;
        std::unique_ptr<ExplorerWindowParts>     parts;

        Rig()
        {
            CassoLinkSettings  settings;
            ExplorerServices   services;



            settings.name      = "Test";
            settings.transport = LinkTransport::Emulated;
            settings.address   = "127.0.0.1:6502";
            prefs.links        = { settings };
            prefs.typedPaths   = { L"C:\\Disks", L"C:\\Games" };

            links = std::make_unique<CassoLinkConnections> (factory, folder, linkIo);

            app.fs      = &fs;
            app.watcher = &watcher;
            app.links   = links.get();

            services.dialogs    = std::make_unique<FakeHostDialogs>();
            services.shellIcons = std::make_unique<BlankShellIcons>();
            services.infoTips   = std::make_unique<RecordingInfoTips>();
            services.shellVerbs = std::make_unique<RecordingShellVerbs>();

            icons = static_cast<BlankShellIcons *>     (services.shellIcons.get());
            tips  = static_cast<RecordingInfoTips *>   (services.infoTips.get());
            verbs = static_cast<RecordingShellVerbs *> (services.shellVerbs.get());

            parts = std::make_unique<ExplorerWindowParts> (window, browser, actions, prefs, app, scaler, theme, std::move (services));
        }

        ~Rig()
        {
            parts.reset();
            links.reset();
        }

        //  What the window runs as it opens, in order.
        void  Open()
        {
            parts->Prepare();
            parts->CreateControls();
            parts->Configure();
        }
    };



    TEST_METHOD (Open_ListsTheSavedConnectionsAsRoots)
    {
        Rig  rig;



        rig.Open();

        Assert::AreEqual ((size_t) 1, rig.browser.GetTreeModel().GetLinks().size());
        Assert::AreEqual (std::wstring (L"Test"), rig.browser.GetTreeModel().GetLinks()[0].name);
    }



    TEST_METHOD (Open_TakesTheTypedPathsFromThePrefs)
    {
        Rig  rig;



        rig.Open();

        Assert::IsTrue (rig.browser.GetTypedPaths().GetEntries() == rig.prefs.typedPaths);
    }



    TEST_METHOD (Open_StartsTheServicesWithTheWindowsMessages)
    {
        Rig  rig;



        rig.Open();

        Assert::AreEqual ((UINT) AppMessage::ExplorerRecycleBin, rig.verbs->recycleBinMessage);
        Assert::AreEqual ((UINT) AppMessage::ExplorerInfoTip,    rig.tips->startMessage);
        Assert::IsTrue   (rig.icons->sizePx > 0);
    }



    TEST_METHOD (Open_StartsTheFolderWatch)
    {
        Rig  rig;



        rig.Open();

        Assert::IsTrue (rig.parts->folderSync.IsWatching());
    }



    TEST_METHOD (Open_CreatesTheControlsAsChildren)
    {
        Rig  rig;



        rig.Open();

        Assert::IsTrue (rig.window.panel.GetChildCount() > 10);
    }



    TEST_METHOD (HandleAppMessage_TakesEveryExplorerMessageButALaunch)
    {
        Rig  rig;



        rig.Open();

        for (AppMessage message : { AppMessage::ExplorerFolderChanged, AppMessage::ExplorerShellListed, AppMessage::ExplorerCassoLink,
                                    AppMessage::ExplorerInfoTip, AppMessage::ExplorerContentDetails, AppMessage::ExplorerIconsLoaded,
                                    AppMessage::ExplorerNewChoices, AppMessage::ExplorerReply, AppMessage::ExplorerRecycleBin })
        {
            Assert::IsTrue (rig.parts->HandleAppMessage ((UINT) message, 0, 0));
        }

        Assert::IsFalse (rig.parts->HandleAppMessage ((UINT) AppMessage::ExplorerLaunch, 0, 0));
        Assert::IsFalse (rig.parts->HandleAppMessage (WM_APP + 0x7F, 0, 0));
    }



    TEST_METHOD (HandleAppMessage_RunCommandDispatchesTheCommand)
    {
        Rig  rig;



        rig.Open();
        rig.parts->HandleAppMessage ((UINT) AppMessage::ExplorerRunCommand, CassoExplorerCommands::kExit, 0);

        Assert::IsTrue (rig.window.closed);
    }



    TEST_METHOD (HandleTimer_TakesTheWindowsTimersButTheTick)
    {
        Rig  rig;



        rig.Open();

        for (AppTimerId timer : { AppTimerId::ExplorerFolder, AppTimerId::ExplorerRecycleBin, AppTimerId::ExplorerRefresh })
        {
            Assert::IsTrue (rig.parts->HandleTimer ((UINT_PTR) timer));
        }

        Assert::IsFalse (rig.parts->HandleTimer ((UINT_PTR) AppTimerId::ExplorerTooltip));
    }
};
