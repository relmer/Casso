#pragma once

#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../CaptureTests/FakeHostDialogs.h"
#include "../CassoLink/FakeApple.h"
#include "../CassoLink/InMemoryCassoLinkFolder.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../EmuTests/FixtureProvider.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "FakeDiskPictures.h"
#include "FakeExplorerWindow.h"
#include "FakeFolderWatcher.h"
#include "RecordingExplorerServices.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Picker/PickerRequest.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "CassoExplorer/Model/DiskImagePictures.h"
#include "CassoExplorer/Window/ExplorerWindowParts.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewManager.h"
#include "CassoExplorer/Window/Panes/Preview/PreviewDiskThumbnail.h"
#include "CassoLink/FullSend/FullSendManifest.h"
#include "Core/AppMessages.h"
#include "Core/AppTimerIds.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerWindowPartsRig
//
//  The Casso Explorer window's parts over an in-memory host, a fake window
//  and recording services, for tests of the window without a window.
//
////////////////////////////////////////////////////////////////////////////////

struct ExplorerWindowPartsRig
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
    FakeHostDialogs                        * dialogs = nullptr;
    std::unique_ptr<ExplorerWindowParts>     parts;

    ExplorerWindowPartsRig()
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

        icons   = static_cast<BlankShellIcons *>     (services.shellIcons.get());
        tips    = static_cast<RecordingInfoTips *>   (services.infoTips.get());
        verbs   = static_cast<RecordingShellVerbs *> (services.shellVerbs.get());
        dialogs = static_cast<FakeHostDialogs *>     (services.dialogs.get());

        parts = std::make_unique<ExplorerWindowParts> (window, browser, actions, prefs, app, scaler, theme, std::move (services));
    }

    ~ExplorerWindowPartsRig()
    {
        parts.reset();
        links.reset();
    }

    //  A fixture image at a path on the in-memory host.
    void  Seed (const char * fixture, const wchar_t * path)
    {
        FixtureProvider    fixtures;
        std::vector<Byte>  bytes;
        std::string        narrow;



        AssertSucceeded (fixtures.OpenFixture (fixture, bytes));
        AssertSucceeded (fs.WriteAllText (path, std::string (bytes.begin(), bytes.end())));

        for (const wchar_t * p = path; *p != L'\0'; p++)
        {
            narrow.push_back ((char) *p);
        }

        io.files[narrow]  = bytes;
        io.stamps[narrow] = FileStamp { bytes.size(), 100 };
    }

    //  A left click at the middle of the bounds, through the window's
    //  input, as a real click arrives: a move, a press and a release.
    void  Click (const RECT & bounds)
    {
        DxuiMouseEvent  mouse;



        mouse.button      = DxuiMouseButton::Left;
        mouse.positionDip = { (bounds.left + bounds.right) / 2, (bounds.top + bounds.bottom) / 2 };

        for (DxuiMouseEventKind kind : { DxuiMouseEventKind::Move, DxuiMouseEventKind::Down, DxuiMouseEventKind::Up })
        {
            mouse.kind = kind;
            parts->input.HandleMouse (mouse);
        }
    }

    //  What the window runs as it opens, in order, then its first layout.
    void  Open()
    {
        parts->Prepare();
        parts->CreateControls();
        parts->Configure();
        parts->layout.SetClient (RECT { 0, 0, 1280, 800 });
        parts->layout.RecomputeLayout();
    }
};
