#include "Pch.h"

#include "Ui/DiskInspector/Components/InspectorDialogs.h"
#include "Ui/DiskInspector/Components/IInspectorComponentHost.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "Core/TextEncoding.h"
#include "Ui/DiskInspector/DecodeSettingsDialog.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DurableCommit.h"
#include "Seams/Win32DiskFileIo.h"
#include "Seams/Win32HostDialogs.h"
#include "Ui/DiskInspector/ExportDialog.h"
#include "Ui/DiskInspector/GoToDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorDialogs::InspectorDialogs
//
////////////////////////////////////////////////////////////////////////////////

InspectorDialogs::InspectorDialogs (InspectorSession & session, IInspectorComponentHost & host) :
    m_session (session),
    m_host    (host)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorDialogs::OpenDecodeSettings
//
//  The dialog opens on the settings in force for the selected track.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorDialogs::OpenDecodeSettings()
{
    HRESULT                   hr     = S_OK;
    DecodeSettingsDialog      dialog;
    DxuiWindow::CreateParams  params;



    dialog.Configure (m_session.theme, m_session.analysis.settings, m_session.model.GetQuarterTrack() / DiskImage::kQuarterTracksPerWholeTrack);

    params.title                    = L"Decode settings";
    params.hInstance                = GetModuleHandle (nullptr);
    params.ownerHwnd                = m_host.GetOwnerHwnd();
    params.initialSizeDip           = DecodeSettingsDialog::kSizeDip;
    params.minSizeDip               = DecodeSettingsDialog::kSizeDip;
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);
    CHRA (hr);

    dialog.SetTheme (m_session.theme);
    dialog.ShowModalDialog (IDOK);

    if (dialog.GetOutcome() != DecodeSettingsDialog::Outcome::Cancelled)
    {
        ApplySettings (dialog.GetSettings());
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorDialogs::ApplySettings
//
//  Every record is analyzed again under the new settings; each record's old
//  result shows until its new one arrives.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorDialogs::ApplySettings (const DecodeSettings & settings)
{
    //  Edits on sectors that no longer decode under the new settings are
    //  discarded after the analysis (FR-108).
    if (m_session.context.hasDisk && m_session.analysis.copy != nullptr && (m_session.edits.IsEmpty() ||
        m_host.Confirm (L"Decode settings", { L"Pending edits on sectors that no longer decode as editable under the new settings are discarded." }, L"Change settings")))
    {
        m_session.analysis.settings = settings;
        m_session.scheduler->Restart (m_session.analysis.copy, settings);
        m_session.comparison.ApplySettings (settings);
        m_session.comparison.MarkAChanged();
        m_session.isTablesDirty = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorDialogs::GetPrefs
//
//  The kind "Go to" last went to, so it opens on it again.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorDialogs::GetPrefs (DiskInspectorPrefs & inOut) const
{
    inOut.goToKind = static_cast<int> (m_goToKind);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorDialogs::ApplyPrefs
//
////////////////////////////////////////////////////////////////////////////////

void InspectorDialogs::ApplyPrefs (const DiskInspectorPrefs & prefs)
{
    m_goToKind = static_cast<GoToKind> (std::clamp (prefs.goToKind, 0, GoToDialog::kKindCount - 1));
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorDialogs::OpenGoTo
//
////////////////////////////////////////////////////////////////////////////////

void InspectorDialogs::OpenGoTo()
{
    HRESULT                   hr     = S_OK;
    GoToDialog                dialog;
    DxuiWindow::CreateParams  params;
    GoToTarget                target;



    dialog.Configure (m_session.theme, m_session.analysis, m_session.model.GetQuarterTrack(), m_goToKind);

    params.title                    = L"Go to";
    params.hInstance                = GetModuleHandle (nullptr);
    params.ownerHwnd                = m_host.GetOwnerHwnd();
    params.initialSizeDip           = GoToDialog::kSizeDip;
    params.minSizeDip               = GoToDialog::kSizeDip;
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);
    CHRA (hr);

    dialog.SetTheme (m_session.theme);
    dialog.ShowModalDialog (IDOK);

    m_goToKind = dialog.GetKind();
    target     = dialog.GetTarget();

    //  A file gone to is chosen as a click in the Files view's list chooses
    //  it, which selects its first sector; the view picks it up as it syncs.
    if (dialog.IsChosen() && target.file >= 0)
    {
        m_session.mapIndex = target.fileMap;
        m_session.goToFile = target.file;
        m_host.OnSelection();
    }
    else if (dialog.IsChosen() && target.firstNibble >= 0)
    {
        m_session.model.SelectNibbles (target.quarterTrack, target.firstNibble, 1);
        m_host.OnSelection();
    }
    else if (dialog.IsChosen() && target.sectorIndex >= 0)
    {
        m_session.model.SelectSector (target.quarterTrack, target.sectorIndex);
        m_host.OnSelection();
    }
    else if (dialog.IsChosen())
    {
        m_session.model.SelectQuarterTrack (target.quarterTrack);
        m_host.OnSelection();
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorDialogs::OpenExport
//
//  "Export..." (FR-058): the dialog builds the bytes; a list of what was
//  written as decoded or as zeros is shown first and can stop it; the save
//  dialog picks the file, its own prompt confirming a replace; and the
//  bytes go through DurableCommit. The image is never touched.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorDialogs::OpenExport()
{
    HRESULT                   hr        = S_OK;
    ExportDialog              dialog;
    DxuiWindow::CreateParams  params;
    Win32HostDialogs          dialogs;
    Win32DiskFileIo           fileIo;
    FileDialogSpec            spec;
    CommitPlan::Progress      progress;
    std::filesystem::path     image;
    std::filesystem::path     target;
    std::wstring              list;
    bool                      isPicked  = false;
    bool                      isGoingOn = false;



    image = (m_session.analysis.copy != nullptr) ? std::filesystem::path (TextEncoding::Utf8ToWide (m_session.analysis.copy->fileName)) : std::filesystem::path (L"Disk");
    dialog.Configure (m_session.theme, m_session.analysis, m_session.model.GetQuarterTrack(), m_session.model.GetSectorIndex(), image.stem().wstring());

    params.title                    = L"Export";
    params.hInstance                = GetModuleHandle (nullptr);
    params.ownerHwnd                = m_host.GetOwnerHwnd();
    params.initialSizeDip           = ExportDialog::kSizeDip;
    params.minSizeDip               = ExportDialog::kSizeDip;
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);
    CHRA (hr);

    dialog.SetTheme (m_session.theme);
    dialog.ShowModalDialog (IDOK);
    BAIL_OUT_IF (!dialog.IsChosen(), S_OK);

    for (const std::wstring & note : dialog.GetRequest().notes)
    {
        list += note + L"\n";
    }

    isGoingOn = list.empty() || DxuiMessageBox (m_host.GetOwnerHwnd(), m_session.theme, (L"The export will include these as noted:\n\n" + list + L"\nExport anyway?").c_str(),
                                                L"Export", MB_OKCANCEL | MB_ICONWARNING) == IDOK;
    BAIL_OUT_IF (!isGoingOn, S_OK);

    spec.filters          = { { dialog.GetRequest().extension == L"woz" ? L"WOZ images" : L"Binary files", L"*." + dialog.GetRequest().extension } };
    spec.defaultExtension = dialog.GetRequest().extension;
    spec.defaultFileName  = dialog.GetRequest().fileName;
    spec.initialFolder    = image.parent_path();

    hr = dialogs.PickFileToSave (m_host.GetOwnerHwnd(), spec, target, isPicked);
    CHR (hr);
    BAIL_OUT_IF (!isPicked, S_OK);

    hr = DurableCommit::Commit (fileIo, TextEncoding::WideToUtf8 (target.wstring()), dialog.GetRequest().bytes, GetTickCount64(),
                                std::filesystem::exists (target) ? CommitMode::Replace : CommitMode::CreateNew, progress);

    if (FAILED (hr))
    {
        (void) DxuiMessageBox (m_host.GetOwnerHwnd(), m_session.theme, (L"The export could not be written to " + target.wstring() + L".").c_str(), L"Export", MB_OK | MB_ICONWARNING);
    }

Error:
    return;
}
