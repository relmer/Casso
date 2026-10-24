#include "Pch.h"
#include "Shell/Components/ShellChrome.h"
#include "Shell/Components/ShellRenderer.h"

#include "Shell/EmulatorShell.h"
#include "Shell/ShellDebugger.h"
#include "Shell/EmulatorShellInternal.h"
#include "Shell/CpuCommandDispatcher.h"
#include "Shell/MachineStateFile.h"
#include "AssetBootstrap.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Printer/PngCodec.h"
#include "Devices/Printer/RgbaImage.h"
#include "Shell/AutoStatePolicy.h"
#include "Shell/Components/ShellSettings.h"
#include "Ui/Dialogs/DialogDefinition.h"
#include "Machines/Apple2/Common/AppleSpeaker.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShowSaveStateDialog
//
//  File > Save state. UI thread: asks where, offering the machine's name and
//  the time, and posts the path to the CPU thread, which saves the machine
//  between instructions.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ShowSaveStateDialog()
{
    HRESULT         hr     = S_OK;
    FileDialogSpec  spec;
    SYSTEMTIME      now    = {};
    fs::path        chosen;
    bool            picked = false;



    GetLocalTime (&now);

    spec.filters          = { { L"Casso machine state", std::wstring (L"*.") + MachineStateFile::kExtension } };
    spec.defaultExtension = MachineStateFile::kExtension;
    spec.defaultFileName  = MachineStateFile::MakeDefaultFileName (m_machine.GetCurrentMachineName(), now);
    spec.initialFolder    = AssetBootstrap::GetStateDirectory();

    hr = m_hostDialogs.PickFileToSave (m_hwnd, spec, chosen, picked);
    CHR (hr);

    BAIL_OUT_IF (!picked, S_OK);

    PostCommand (IDM_FILE_SAVE_STATE, CpuCommandDispatcher::PathToPayload (chosen));

Error:
    if (FAILED (hr))
    {
        m_chrome->PostNotice (L"The file dialog could not be opened.");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShowLoadStateDialog
//
//  File > Load state. UI thread: asks which file, and posts its path to the
//  CPU thread, which loads it.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ShowLoadStateDialog()
{
    HRESULT         hr     = S_OK;
    FileDialogSpec  spec;
    fs::path        chosen;
    bool            picked = false;



    spec.filters          = { { L"Casso machine state", std::wstring (L"*.") + MachineStateFile::kExtension } };
    spec.defaultExtension = MachineStateFile::kExtension;
    spec.initialFolder    = AssetBootstrap::GetStateDirectory();

    hr = m_hostDialogs.PickFileToOpen (m_hwnd, spec, chosen, picked);
    CHR (hr);

    BAIL_OUT_IF (!picked, S_OK);

    PostCommand (IDM_FILE_LOAD_STATE, CpuCommandDispatcher::PathToPayload (chosen));

Error:
    if (FAILED (hr))
    {
        m_chrome->PostNotice (L"The file dialog could not be opened.");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveMachineState
//
//  CPU thread. Writes the whole machine, its disks included as the machine
//  has them in memory, to path. No disk image file is written. The file is
//  written whole to a temporary file and renamed over path, so after a write
//  that fails partway any earlier file at path is intact.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SaveMachineState (const std::filesystem::path & path)
{
    HRESULT            hr        = S_OK;
    std::vector<Byte>  bytes;
    std::vector<Byte>  thumbnail;
    MachineStateError  error;



    // A state without a thumbnail loads the same; the picture is only for the
    // prompt that offers to replace it.
    hr = m_renderer->CaptureStateThumbnail (thumbnail);

    if (FAILED (hr))
    {
        thumbnail.clear();
    }

    hr = MachineStateFile::Build (m_machine, thumbnail, bytes, error);
    CHR (hr);

    hr = DiskImageStore::WriteFileAtomically (path.string(), bytes);
    CHRF (hr, error = MachineStateFile::MakeError ("file not written", "The state file could not be written in full."));

    m_chrome->PostNotice (L"Machine state saved to " + path.wstring() + L".");

Error:
    if (FAILED (hr))
    {
        m_chrome->PostNotice (FormatStateError (L"saved", error));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadMachineState
//
//  CPU thread. The file's bytes are loaded as LoadMachineStateBytes loads
//  them, and the outcome is posted as a notice.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::LoadMachineState (const std::filesystem::path & path)
{
    HRESULT              hr    = S_OK;
    std::vector<Byte>    bytes;
    MachineStateError    error;



    hr = ReadStateFile (path, bytes);
    CHRF (hr, error = MachineStateFile::MakeError ("file not read", "The state file could not be read."));

    hr = LoadMachineStateBytes (bytes, error);
    CHR (hr);

    m_chrome->PostNotice (L"Machine state loaded from " + path.wstring() + L".");

Error:
    if (FAILED (hr))
    {
        m_chrome->PostNotice (FormatStateError (L"loaded", error));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadMachineStateBytes
//
//  CPU thread. Parses the state and checks its disk images before anything
//  changes; a state another machine kind saved opens that machine first, as
//  opening a machine does, and the rest is checked on it. History is dropped
//  and starts again at the loaded point, and the debugger is notified of a
//  machine replaced.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::LoadMachineStateBytes (
    const std::vector<Byte>  & bytes,
    MachineStateError        & outError)
{
    HRESULT                hr             = S_OK;
    MachineStateContents   contents;
    AppleSpeaker         * speaker        = nullptr;
    bool                   isOtherMachine = false;
    bool                   isSwitched     = false;



    hr = MachineStateFile::Parse (bytes, contents, outError);
    CHR (hr);

    //  The disk images are checked here, with no machine, before another
    //  machine is opened.
    hr = MachineStateFile::CheckDisks (contents, outError);
    CHR (hr);

    isOtherMachine = contents.machineName != m_machine.GetCurrentMachineName();

    // The switch saves the outgoing machine's automatic state as any switch
    // does, but the state being loaded replaces the incoming machine's, so
    // its automatic state is not resumed.
    if (isOtherMachine)
    {
        m_isLoadingStateSwitch = true;
        hr = SwitchMachine (contents.machineName);
        m_isLoadingStateSwitch = false;
        CHRF (hr, outError = MachineStateFile::MakeError ("machine not opened", "The machine that saved the state could not be opened."));

        isSwitched = true;
    }

    hr = MachineStateFile::Check (m_machine, contents, outError);
    CHR (hr);

    m_debugger->StopReverseRecording();

    hr = MachineStateFile::Apply (m_machine, contents, outError);

    m_debugger->StartReverseRecording();

    CHR (hr);

    speaker = m_machine.GetRefs().speaker;

    if (speaker != nullptr)
    {
        speaker->ClearTimestamps();
        speaker->BeginFrame();
    }

    m_debugger->NotifyDebugMachineChanged (fs::path (m_machine.GetCurrentMachineName()).string());

    m_debugger->MarkViewDirty();

    m_renderer->RenderFramebuffer();
    m_renderer->PublishFramebuffer();

Error:
    if (FAILED (hr) && isSwitched)
    {
        outError.detail += " The machine that saved the state is open now, and nothing was loaded into it.";
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadStateFile
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::ReadStateFile (
    const std::filesystem::path  & path,
    std::vector<Byte>            & outBytes)
{
    HRESULT          hr      = S_OK;
    std::ifstream    in;
    std::streamoff   size    = 0;
    bool             isOpen  = false;
    bool             hasSize = false;
    bool             isRead  = false;



    outBytes.clear();

    in.open (path, std::ios::binary | std::ios::ate);

    isOpen = in.is_open();
    CBR (isOpen);

    size = in.tellg();
    hasSize = size >= 0;
    CBR (hasSize);

    outBytes.resize (static_cast<size_t> (size));

    in.seekg (0);
    in.read (reinterpret_cast<char *> (outBytes.data()), size);

    isRead = !in.fail();
    CBR (isRead);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStateError
//
//  "Machine state not loaded: different ROM images. The state was ..."
//
////////////////////////////////////////////////////////////////////////////////

std::wstring EmulatorShell::FormatStateError (
    const wchar_t            * verb,
    const MachineStateError  & error)
{
    return std::format (L"Machine state not {}: {}. {}",
                        verb,
                        fs::path (error.label).wstring(),
                        fs::path (error.detail).wstring());
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetAutoStatePath
//
////////////////////////////////////////////////////////////////////////////////

std::filesystem::path EmulatorShell::GetAutoStatePath() const
{
    return AutoStatePolicy::GetAutoStatePath (m_stateFolder.empty() ? AssetBootstrap::GetStateDirectory() : m_stateFolder, m_machine.GetCurrentMachineName());
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveAutoState
//
//  The thread that owns the machine: the CPU thread on a switch, the UI
//  thread at exit once the CPU thread has stopped. For the machine a
//  command-line launch started with its own media, an earlier automatic
//  state is replaced only if the user chose to in the prompt. The file is
//  written whole and renamed over the old one, so after a failure the
//  earlier state is intact. At exit the window is being destroyed and a
//  notice posted to it never appears, so a failure is reported in a system
//  box.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SaveAutoState (bool isExiting)
{
    HRESULT                hr         = S_OK;
    std::filesystem::path  path       = GetAutoStatePath();
    std::error_code        ec;
    bool                   hasState   = std::filesystem::exists (path, ec);
    bool                   isApproved = m_isAutoStateReplaceApproved.load();
    AutoStateSave          action     = AutoStatePolicy::GetSaveAction (m_settings->GetPrefs().autoSaveMachineState,
                                                                        m_isCommandLineMediaSession,
                                                                        hasState);
    std::vector<Byte>      thumbnail;
    std::vector<Byte>      bytes;
    MachineStateError      error;



    BAIL_OUT_IF (action == AutoStateSave::Skip, S_OK);
    BAIL_OUT_IF (action == AutoStateSave::PromptFirst && !isApproved, S_OK);

    // A state without a thumbnail resumes the same; the picture is only for
    // the prompt that offers to replace it.
    hr = m_renderer->CaptureStateThumbnail (thumbnail);

    if (FAILED (hr))
    {
        thumbnail.clear();
    }

    hr = MachineStateFile::Build (m_machine, thumbnail, bytes, error);
    CHR (hr);

    hr = DiskImageStore::WriteFileAtomically (path.string(), bytes);
    CHRF (hr, error = MachineStateFile::MakeError ("file not written", "The automatic state file could not be written in full."));

Error:
    if (FAILED (hr) && isExiting && m_exitReport != nullptr)
    {
        m_exitReport (FormatStateError (L"saved", error));
    }
    else if (FAILED (hr) && isExiting)
    {
        MessageBoxW (nullptr, FormatStateError (L"saved", error).c_str(), L"Casso", MB_OK | MB_ICONWARNING | MB_TASKMODAL);
    }
    else if (FAILED (hr))
    {
        m_chrome->PostNotice (FormatStateError (L"saved", error));
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResumeAutoState
//
//  The thread that owns the machine, with history not recording: at startup
//  before the CPU thread starts, and late in a machine switch, after the
//  machine is built and power cycled and before history starts. When the
//  state does not resume, the cold-booted machine runs on, the file is not
//  modified, and a notice with the reason appears.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ResumeAutoState (bool isStartup)
{
    HRESULT                hr          = S_OK;
    std::filesystem::path  path        = GetAutoStatePath();
    std::error_code        ec;
    bool                   hasState    = std::filesystem::exists (path, ec);
    bool                   isEnabled   = m_settings->GetPrefs().autoSaveMachineState;
    bool                   shouldLoad  = false;
    std::vector<Byte>      bytes;
    MachineStateContents   contents;
    MachineStateError      error;
    AppleSpeaker         * speaker     = nullptr;



    shouldLoad = isStartup
                 ? AutoStatePolicy::ShouldResumeAtStartup (isEnabled, m_isCommandLineMediaSession, hasState)
                 : AutoStatePolicy::ShouldResumeOnSwitch  (isEnabled, m_isLoadingStateSwitch, hasState);

    BAIL_OUT_IF (!shouldLoad, S_OK);

    hr = ReadStateFile (path, bytes);
    CHRF (hr, error = MachineStateFile::MakeError ("file not read", "The automatic state file could not be read."));

    hr = MachineStateFile::Parse (bytes, contents, error);
    CHR (hr);

    hr = MachineStateFile::Apply (m_machine, contents, error);
    CHR (hr);

    speaker = m_machine.GetRefs().speaker;

    if (speaker != nullptr)
    {
        speaker->ClearTimestamps();
        speaker->BeginFrame();
    }

Error:
    if (FAILED (hr))
    {
        m_chrome->PostNotice (AutoStatePolicy::FormatResumeFailure (std::wstring (m_machine.GetConfig().name.begin(), m_machine.GetConfig().name.end()),
                                                                    error.label));
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PromptToReplaceAutoState
//
//  UI thread. For the machine a command-line launch started with its own
//  media, an existing automatic state is the user's earlier session, so
//  before it is replaced the prompt has when it was saved and its screen,
//  and the user chooses. Closing the prompt replaces nothing.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PromptToReplaceAutoState()
{
    constexpr int          kReplace    = 1;
    constexpr int          kKeep       = 0;
    constexpr float        kThumbDp    = 280.0f;
    HRESULT                hr          = S_OK;
    HRESULT                hrThumb     = S_OK;
    std::filesystem::path  path        = GetAutoStatePath();
    std::error_code        ec;
    bool                   hasState    = std::filesystem::exists (path, ec);
    AutoStateSave          action      = AutoStatePolicy::GetSaveAction (m_settings->GetPrefs().autoSaveMachineState,
                                                                         m_isCommandLineMediaSession,
                                                                         hasState);
    std::vector<Byte>      bytes;
    MachineStateContents   contents;
    MachineStateError      error;
    RgbaImage              thumbnail;
    DialogImage            image;
    DialogDefinition       def;
    int                    choice      = kKeep;



    m_isAutoStateReplaceApproved = false;

    BAIL_OUT_IF (action != AutoStateSave::PromptFirst, S_OK);

    hr = ReadStateFile (path, bytes);
    CHR (hr);

    hr = MachineStateFile::Parse (bytes, contents, error);
    CHR (hr);

    def.title = L"Replace the saved machine state?";
    def.body.push_back (DialogTextRun { std::format (L"Casso started with media from the command line, so the {} state saved {} was not resumed. "
                                                     L"Replace it with the current state?",
                                                     std::wstring (m_machine.GetConfig().name.begin(), m_machine.GetConfig().name.end()),
                                                     FormatAutoStateTime (contents.savedAtUtc, path)),
                                        false,
                                        std::wstring() });

    if (!contents.thumbnailPng.empty())
    {
        hrThumb = PngCodec::DecodeRgba (contents.thumbnailPng, thumbnail);
    }

    if (!contents.thumbnailPng.empty() && SUCCEEDED (hrThumb))
    {
        image.rgba      = std::move (thumbnail.rgba);
        image.width     = thumbnail.width;
        image.height    = thumbnail.height;
        image.displayDp = kThumbDp;
        def.image       = std::move (image);
    }

    def.buttons.push_back (DialogButton { L"Replace",       kReplace, false, false, false });
    def.buttons.push_back (DialogButton { L"Don't replace", kKeep,    true,  true,  false });
    def.closeBoxResult = kKeep;

    choice = ShowModalDialog (def);

    m_isAutoStateReplaceApproved = choice == kReplace;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatAutoStateTime
//
//  The local date and time an automatic state was saved, for the prompt. A
//  version-1 state has no save time, so the file's time is used instead.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring EmulatorShell::FormatAutoStateTime (
    uint64_t                       savedAtUtc,
    const std::filesystem::path  & path)
{
    constexpr int               kTextChars        = 128;
    ULARGE_INTEGER              ticks             = {};
    FILETIME                    utc               = {};
    FILETIME                    local             = {};
    SYSTEMTIME                  time              = {};
    WIN32_FILE_ATTRIBUTE_DATA   attributes        = {};
    wchar_t                     date[kTextChars]  = {};
    wchar_t                     clock[kTextChars] = {};
    bool                        hasFileTime       = false;



    ticks.QuadPart     = savedAtUtc;
    utc.dwLowDateTime  = ticks.LowPart;
    utc.dwHighDateTime = ticks.HighPart;

    if (savedAtUtc == 0)
    {
        hasFileTime = GetFileAttributesExW (path.c_str(), GetFileExInfoStandard, &attributes) != FALSE;
    }

    if (hasFileTime)
    {
        utc = attributes.ftLastWriteTime;
    }

    FileTimeToLocalFileTime (&utc, &local);
    FileTimeToSystemTime    (&local, &time);

    GetDateFormatEx (LOCALE_NAME_USER_DEFAULT, DATE_LONGDATE,  &time, nullptr, date,  kTextChars, nullptr);
    GetTimeFormatEx (LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &time, nullptr, clock, kTextChars);

    return std::format (L"{} at {}", date, clock);
}





