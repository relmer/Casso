#include "Pch.h"

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
    TapeDeck::Snapshot  snapshot = m_machine.GetTapeDeck().GetSnapshot();
    TapeDeckView        view;



    view.transport     = snapshot.transport;
    view.isRecordArmed = snapshot.isRecordArmed;
    view.isWritable    = snapshot.isWritable;

    if (m_tapeManager != nullptr)
    {
        view.path = std::filesystem::path (m_tapeManager->GetInsertedPath()).wstring();
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
//  Once a frame: refreshes the flat recorder and places it in the drive band,
//  left of the first drive and on the same line. It is hidden on a machine
//  without cassette jacks, when the desk scene draws the recorder instead,
//  and when the band holds no drives to line up with.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SyncTapeChrome()
{
    RECT           drive   = m_driveChrome[0].GetOuterRect();
    bool           isShown = MachineHasCassettePort() && !DeskSceneActive() && !IsRectEmpty (&drive);
    UINT           dpi     = (UINT) lroundf ((float) m_scaler.GetDpi() * m_chromeSceneScale);
    int            gap     = MulDiv (s_kCompactDriveWidgetGapDp, (int) dpi, s_kBaseDpi);
    DxuiDpiScaler  scaler;
    RECT           probe   = {};
    RECT           anchor  = {};
    int            width   = 0;



    m_tapeChrome.SyncFromView (GetTapeView());

    if (!isShown)
    {
        m_tapeChrome.Hide();
        m_tapeChrome.SetVisible (false);
        return;
    }

    scaler.SetDpi (dpi);
    m_tapeChrome.Layout (RECT {}, scaler);
    probe = m_tapeChrome.GetOuterRect();
    width = probe.right - probe.left;

    anchor.left = max (gap, (int) drive.left - width - gap);
    anchor.top  = drive.top;

    m_tapeChrome.SetVisible (true);
    m_tapeChrome.Layout (anchor, scaler);
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
        case TapeDeckRegion::Name:   BrowseForTape();                                    break;
        case TapeDeckRegion::Rewind: m_tapeManager->Rewind();                            break;
        case TapeDeckRegion::Play:   m_tapeManager->Play();                              break;
        case TapeDeckRegion::Stop:   m_tapeManager->Stop();                              break;
        case TapeDeckRegion::Record: m_tapeManager->SetRecordArmed (!view.isRecordArmed); break;
        case TapeDeckRegion::Eject:  m_tapeManager->Eject();                             break;
        default:                                                                         break;
    }

    m_d3dRenderer.MarkRedrawNeeded();
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
    std::string            error;



    spec.filters = { { L"Tape recordings", L"*.wav;*.aif;*.aiff;*.mp3" }, { L"All files", L"*.*" } };

    if (m_tapeManager != nullptr && !m_tapeManager->GetInsertedPath().empty())
    {
        spec.initialFolder = std::filesystem::path (m_tapeManager->GetInsertedPath()).parent_path();
    }

    m_host->BeginModalKeepAlive();
    hr = m_hostDialogs.PickFileToOpen (m_hwnd, spec, picked, isPicked);
    m_host->EndModalKeepAlive();

    CHR (hr);
    BAIL_OUT_IF (!isPicked, S_OK);

    hr = m_tapeManager->Insert (picked.string(), error);
    CHRF (hr, ShowNotice (L"Error: unreadable tape\n" + std::filesystem::path (error).wstring()));

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
    bool                   isPicked  = false;
    std::string            error;



    spec.filters          = { { L"WAV recordings", L"*.wav" } };
    spec.defaultExtension = L"wav";
    spec.defaultFileName  = L"New tape.wav";

    m_host->BeginModalKeepAlive();
    hr = m_hostDialogs.PickFileToSave (m_hwnd, spec, picked, isPicked);
    m_host->EndModalKeepAlive();

    CHR (hr);
    BAIL_OUT_IF (!isPicked || m_tapeManager == nullptr, S_OK);

    hr = m_tapeManager->CreateBlank (picked.string(), error);
    CHRF (hr, ShowNotice (L"Error: tape not created\n" + std::filesystem::path (error).wstring()));

Error:
    return;
}
