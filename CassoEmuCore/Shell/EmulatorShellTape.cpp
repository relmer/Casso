#include "Pch.h"

#include "Devices/Tape/TapeImageLoader.h"
#include "Ui/Dialogs/TapePositionDialog.h"
#include "Machines/MachineDefinitions.h"
#include "Shell/EmulatorShell.h"
#include "Shell/EmulatorShellInternal.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHasCassettePort
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::MachineHasCassettePort() const
{
    const MachineDefinition  * definition = MachineDefinitions::Find (m_machine.GetConfig().machineId);



    return definition != nullptr && definition->hasCassettePort;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTapeView
//
//  What the tape widget shows this frame, from the deck's thread-safe
//  snapshot and the path the UI thread inserted.
//
////////////////////////////////////////////////////////////////////////////////

TapeDeckView EmulatorShell::GetTapeView() const
{
    constexpr int64_t   kLoadingShowMs = 150;
    TapeDeck::Snapshot  snapshot       = m_machine.GetTapeDeck().GetSnapshot();
    TapeDeckView        view;
    std::string         loading;
    int64_t             loadingMs      = 0;



    view.transport     = snapshot.transport;
    view.isRecordArmed = snapshot.isRecordArmed;
    view.isWritable    = snapshot.isWritable;

    if (m_tapeManager != nullptr)
    {
        view.path = std::filesystem::path (m_tapeManager->GetInsertedPath()).wstring();
        loading   = m_tapeManager->GetLoadingPath (loadingMs);

        // A quick load shows nothing, so a WAV that decodes at once does not
        // flicker a message.
        if (!loading.empty() && loadingMs >= kLoadingShowMs)
        {
            view.loadingPath = std::filesystem::path (loading).wstring();
        }
    }

    if (snapshot.sampleRate != 0)
    {
        view.positionSeconds = snapshot.positionSamples / snapshot.sampleRate;
        view.lengthSeconds   = (double) snapshot.lengthSamples / snapshot.sampleRate;
    }

    return view;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncTapeChrome
//
//  Once a frame: refreshes the flat recorder and lays it out where the drive
//  row put it, right of the drives. It is hidden on a machine without
//  cassette jacks, when the desk scene draws the recorder instead, and when
//  the band holds no drives to line up with.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SyncTapeChrome()
{
    RECT           drive   = m_driveChrome[0].GetOuterRect();
    bool           isShown = MachineHasCassettePort() && !DeskSceneActive() && !IsRectEmpty (&drive) &&
                             m_tapeAnchorDpi != 0;
    DxuiDpiScaler  scaler;
    TapeDeckView   view    = GetTapeView();



    m_tapeChrome.SyncFromView (view);
    SyncSceneTapeLabel();

    // A STATIC GUEST SCREEN PRESENTS NO FRAMES, and a program loading from
    // tape is exactly that: so while the tape moves, and while a long name may
    // be scrolling under the pointer, ask for one every UI frame or the
    // counter stands still.
    // A transport change repaints too -- above all a STOP, after which nothing
    // moves to ask for a frame and the desk's PLAY key would stay drawn down.
    if (view.transport != m_shownTapeTransport)
    {
        m_shownTapeTransport = view.transport;
        m_d3dRenderer.MarkRedrawNeeded();
    }

    if ((view.transport != TapeTransport::Empty && view.transport != TapeTransport::Stopped) ||
        (isShown && m_tapeChrome.GetHover() == TapeDeckRegion::Name) ||
        (isShown && m_tapeChrome.IsMagnifying()))
    {
        m_d3dRenderer.MarkRedrawNeeded();
    }

    if (!isShown)
    {
        m_tapeChrome.Hide();
        m_tapeChrome.SetVisible (false);
        return;
    }

    scaler.SetDpi (m_tapeAnchorDpi);
    m_tapeChrome.SetVisible (true);
    m_tapeChrome.Layout (m_tapeAnchor, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterTapeDropTarget
//
//  Adds the recorder to the drop targets the drives already registered: the
//  3D recorder's projected box in the desk scene, the flat widget otherwise.
//  The flat widget is laid out here first because it is placed off the
//  drive's rect, which is only final once the drives have been laid out.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RegisterTapeDropTarget()
{
    HRESULT  hr   = S_OK;
    RECT     rect = {};



    BAIL_OUT_IF (!MachineHasCassettePort(), S_OK);

    if (DeskSceneActive())
    {
        rect = m_deskScene.Composition().recorderRectPx;
    }
    else
    {
        SyncTapeChrome();
        rect = m_tapeChrome.IsHidden() ? RECT {} : m_tapeChrome.GetOuterRect();
    }

    BAIL_OUT_IF (IsRectEmpty (&rect), S_OK);

    m_uiShell.GetHitTester().Register (DxuiHitRect { rect, DxuiHitSlot::Custom, s_kTapeDropTag });

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnFileDropped
//
//  A file dropped on a drive mounts there and one dropped on the recorder is
//  inserted into it, but only the kind each takes: a tape dropped on a drive,
//  or a disk on the recorder, is ignored rather than mounted somewhere it
//  cannot be read.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnFileDropped (int tag, const std::wstring & path)
{
    if (tag == s_kTapeDropTag)
    {
        if (TapeImageLoader::IsTapeFileExtension (path))
        {
            InsertTape (path);
        }
    }
    else if (IsSupportedDiskImageExtension (path))
    {
        Mount (6, tag, path);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleTapeClick
//
//  A control that cannot act in the deck's current state does nothing, as a
//  key on a real deck would not go down.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::HandleTapeClick (TapeDeckRegion region)
{
    TapeDeckView  view = GetTapeView();



    if (!TapeDeckWidget::IsRegionEnabled (region, view) || m_tapeManager == nullptr)
    {
        return;
    }

    switch (region)
    {
        case TapeDeckRegion::Name:        PickTape();                                                                 break;
        case TapeDeckRegion::Rewind:      m_tapeManager->Rewind();                                                    break;
        case TapeDeckRegion::FastForward: m_tapeManager->FastForward();                                               break;
        case TapeDeckRegion::Play:        m_tapeManager->Play();                                                      break;
        case TapeDeckRegion::Stop:        m_tapeManager->Stop();                                                      break;
        case TapeDeckRegion::Record:      m_tapeManager->SetRecordArmed (view.transport != TapeTransport::Recording); break;
        case TapeDeckRegion::Eject:       m_tapeManager->Eject();                                                     break;
        case TapeDeckRegion::Counter:     PromptTapePosition();                                                       break;
        default:                                                                                                      break;
    }

    m_d3dRenderer.MarkRedrawNeeded();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PromptTapePosition
//
//  Asks where to wind the tape, starting from where it is, and winds there.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PromptTapePosition()
{
    HRESULT                   hr     = S_OK;
    TapeDeckView              view   = GetTapeView();
    TapePositionDialog        dialog;
    DxuiWindow::CreateParams  params;



    dialog.Configure (&m_chromeTheme, view.positionSeconds, view.lengthSeconds);

    params.title                    = L"Tape position";
    params.hInstance                = GetModuleHandle (nullptr);
    params.ownerHwnd                = m_hwnd;
    params.initialSizeDip           = { 360, 170 };
    params.minSizeDip               = { 360, 170 };
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);
    CHRA (hr);

    dialog.SetTheme (&m_chromeTheme);
    dialog.ShowModalDialog (IDOK);

    BAIL_OUT_IF (!dialog.GetOutcome().confirmed || m_tapeManager == nullptr, S_OK);

    m_tapeManager->Seek (dialog.GetOutcome().seconds);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowseForTape
//
//  Opens the file picker on the folder of the tape in the deck, if any. A
//  tape that cannot be read is reported and the deck keeps what it had.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::BrowseForTape()
{
    HRESULT                hr        = S_OK;
    FileDialogSpec         spec;
    std::filesystem::path  picked;
    bool                   isPicked  = false;



    spec.filters = { { L"Tape recordings", L"*.wav;*.aif;*.aiff;*.aifc;*.mp3;*.flac" }, { L"All files", L"*.*" } };

    spec.initialFolder = m_windowCommandManager->GetDiskCreateFolder();

    m_host->BeginModalKeepAlive();
    hr = m_hostDialogs.PickFileToOpen (m_hwnd, spec, picked, isPicked);
    m_host->EndModalKeepAlive();

    CHR (hr);
    BAIL_OUT_IF (!isPicked, S_OK);

    InsertTape (picked.wstring());

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateBlankTape
//
//  Asks where the new tape goes, writes it empty and inserts it, ready to
//  record on.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::CreateBlankTape()
{
    HRESULT                hr        = S_OK;
    FileDialogSpec         spec;
    std::filesystem::path  picked;
    std::u8string          folderUtf8;
    bool                   isPicked  = false;



    spec.filters          = { { L"WAV recordings", L"*.wav" } };
    spec.defaultExtension = L"wav";
    spec.defaultFileName  = L"New tape.wav";
    spec.initialFolder    = m_windowCommandManager->GetDiskCreateFolder();

    m_host->BeginModalKeepAlive();
    hr = m_hostDialogs.PickFileToSave (m_hwnd, spec, picked, isPicked);
    m_host->EndModalKeepAlive();

    CHR (hr);
    BAIL_OUT_IF (!isPicked || m_tapeManager == nullptr, S_OK);

    // New tapes and new disks share the create folder.
    folderUtf8 = picked.parent_path().u8string();
    m_globalPrefs.lastDiskCreateFolder.assign (folderUtf8.begin(), folderUtf8.end());
    SaveGlobalPrefs();

    m_tapeManager->CreateBlank (picked.string());
    RecordRecentDisk (picked.wstring(), S_OK);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PickTape
//
//  The disk picker, choosing a tape, anchored under the tape widget.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PickTape()
{
    HRESULT       hr       = S_OK;
    RECT          anchor   = m_tapeChrome.GetNameRect();
    const RECT  * pAnchor  = nullptr;



    if (!IsRectEmpty (&anchor))
    {
        MapWindowPoints (m_hwnd, HWND_DESKTOP, reinterpret_cast<POINT *> (&anchor), 2);
        pAnchor = &anchor;
    }

    m_host->BeginModalKeepAlive();
    hr = m_windowCommandManager->PromptInsertTapeMru (pAnchor);
    m_host->EndModalKeepAlive();

    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InsertTape
//
//  Starts the load and puts the tape at the top of the recent list, which
//  disks and tapes share.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::InsertTape (const std::wstring & path)
{
    if (m_tapeManager == nullptr)
    {
        return;
    }

    m_tapeManager->Insert (std::filesystem::path (path).string());
    RecordRecentDisk (path, S_OK);
}




