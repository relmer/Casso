#include "Pch.h"

#include "Shell/Components/ShellHistory.h"
#include "Debugger/Reverse/ReverseHost.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Shell/CpuManager.h"
#include "Shell/IDebuggerHost.h"
#include "Shell/MachineHost.h"
#include "Shell/SavedHistory.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BuildSavedHistory
//
//  The state saved is the active path's live end, so behind live the
//  machine goes there first; the position it was at is where a load
//  resumes. A history that cannot be saved is left out of the file, which
//  then loads with history starting afresh.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ShellHistory::BuildSavedHistory (
    std::vector<Byte>  & outSection,
    uint64_t           & outResumePosition)
{
    HRESULT               hr       = S_OK;
    EmuCpu              * cpu      = m_machine.GetCpu();
    DiskImageStore      & store    = m_machine.GetDiskStore();
    bool                  isBehind = false;
    uint64_t              mediaId  = 0;
    SavedHistoryContents  contents;
    ReverseResult         result;



    outSection.clear();
    outResumePosition = m_machine.GetPosition();

    BAIL_OUT_IF (!m_isOn || !IsRecording() || cpu == nullptr, S_OK);

    //  A disk that left a bay is not saved with history yet, so neither is
    //  history whose keyframes may have it in a bay.
    BAIL_OUT_IF (store.GetRetainedMediaCount() > 0, S_OK);

    contents.resumePosition = m_machine.GetPosition();
    contents.resumeCycle    = cpu->GetHistoryCycles();

    isBehind = m_reverseHost->IsBehindLive();

    if (isBehind)
    {
        hr = m_reverseHost->Execute (ReverseCommand::GoLive, 0, nullptr, result);
        CHR (hr);
    }

    hr = m_machine.ComputeStateFingerprint (contents.fingerprint);
    CHR (hr);

    hr = m_reverseHost->GetController().SaveHistory (contents.keyframes);
    CHR (hr);

    for (int slot = 0; slot < DiskImageStore::kSlotCount; slot++)
    {
        for (int drive = 0; drive < DiskImageStore::kDriveCount; drive++)
        {
            mediaId = store.GetMediaId (slot, drive);

            if (mediaId != 0)
            {
                contents.media.push_back (SavedBayMedium { slot, drive, mediaId });
            }
        }
    }

    hr = SavedHistory::Build (contents, outSection);
    CHR (hr);

Error:
    if (FAILED (hr))
    {
        outSection.clear();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReturnFromSave
//
//  A save made behind live went to the live end; the machine goes back to
//  where it was.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::ReturnFromSave (uint64_t resumePosition)
{
    HRESULT        hr = S_OK;
    ReverseResult  result;



    BAIL_OUT_IF (!IsRecording() || resumePosition >= m_machine.GetPosition(), S_OK);

    hr = m_reverseHost->Execute (ReverseCommand::Seek, resumePosition, nullptr, result);
    CHR (hr);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StartFromSaved
//
//  The pending section is taken whatever happens. It must parse, come from
//  a build that saves the machine as this one does, and have the media in
//  the bays the state put there; then recording starts from it, and a save
//  made behind live goes back to where it was, paused. When it fails,
//  nothing is recording.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ShellHistory::StartFromSaved()
{
    HRESULT               hr          = S_OK;
    uint64_t              fingerprint = 0;
    bool                  isSameBuild = false;
    bool                  isBehind    = false;
    std::vector<Byte>     section;
    SavedHistoryContents  contents;
    ReverseResult         result;



    section.swap (m_pendingHistory);

    hr = SavedHistory::Parse (section.data(), section.size(), contents);
    CHR (hr);

    hr = m_machine.ComputeStateFingerprint (fingerprint);
    CHR (hr);

    isSameBuild = fingerprint == contents.fingerprint;
    CBREx (isSameBuild, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = RestoreMediaIds (contents.media);
    CHR (hr);

    hr = m_reverseHost->StartRecordingFromSaved (ReverseHost::MakeSettings (m_budgetMb, static_cast<int> (KeyframeSettings::kDefaultFrames)), contents.keyframes);
    CHR (hr);

    isBehind = contents.resumePosition < m_machine.GetPosition();

    if (isBehind)
    {
        m_cpuManager.SetPaused (true);
        m_host.PostCommand (IDM_DEBUG_PAUSE_CHANGED, "1");

        hr = m_reverseHost->Execute (ReverseCommand::Seek, contents.resumePosition, nullptr, result);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RestoreMediaIds
//
//  Each bay with a medium in the saved history has one now, and no other
//  bay has one; each such medium is given the id the keyframes use for it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ShellHistory::RestoreMediaIds (const std::vector<SavedBayMedium> & media)
{
    HRESULT           hr         = S_OK;
    DiskImageStore  & store      = m_machine.GetDiskStore();
    size_t            mounted    = 0;
    bool              isSameBays = false;
    bool              isInBay    = false;
    DiskImage       * image      = nullptr;



    for (int slot = 0; slot < DiskImageStore::kSlotCount; slot++)
    {
        for (int drive = 0; drive < DiskImageStore::kDriveCount; drive++)
        {
            mounted += (store.GetMediaId (slot, drive) != 0) ? 1 : 0;
        }
    }

    isSameBays = mounted == media.size();
    CBREx (isSameBays, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    for (const SavedBayMedium & medium : media)
    {
        isInBay = medium.slot < DiskImageStore::kSlotCount && medium.drive < DiskImageStore::kDriveCount && store.GetMediaId (medium.slot, medium.drive) != 0;
        CBREx (isInBay, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    }

    for (const SavedBayMedium & medium : media)
    {
        image = store.GetImage (medium.slot, medium.drive);
        CBRA (image);

        image->SetImageId (medium.mediaId);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrepareStateLoad
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::PrepareStateLoad()
{
    HRESULT   hr  = S_OK;
    EmuCpu  * cpu = m_machine.GetCpu();



    m_cyclesBeforeLoad = (cpu != nullptr) ? cpu->GetHistoryCycles() : 0;

    BAIL_OUT_IF (!IsRecording() || !m_reverseHost->IsBehindLive(), S_OK);

    hr = m_reverseHost->Fork();
    CHR (hr);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FinishStateLoad
//
//  The loaded state's history cycles count from the power-on before its
//  save, and may be fewer than the newest keyframe's, so they are raised to
//  those before the load. A load that succeeds gets a StateLoad entry in
//  the journal and a boundary keyframe of the loaded machine.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::FinishStateLoad (bool isLoaded)
{
    HRESULT   hr  = S_OK;
    EmuCpu  * cpu = m_machine.GetCpu();



    BAIL_OUT_IF (!IsRecording() || cpu == nullptr, S_OK);

    cpu->AdvanceHistoryCycles (m_cyclesBeforeLoad);

    BAIL_OUT_IF (!isLoaded, S_OK);

    m_machine.RecordInput (InputKind::StateLoad, 0, 0, std::string_view());

    hr = m_reverseHost->GetController().OnMachineChanged();
    CHR (hr);

Error:
    return;
}
