#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/ShellDebugger.h"
#include "Shell/EmulatorShellInternal.h"
#include "Shell/CpuCommandDispatcher.h"
#include "Shell/MachineStateFile.h"
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

    hr = m_hostDialogs.PickFileToSave (m_hwnd, spec, chosen, picked);
    CHR (hr);

    BAIL_OUT_IF (!picked, S_OK);

    PostCommand (IDM_FILE_SAVE_STATE, CpuCommandDispatcher::PathToPayload (chosen));

Error:
    if (FAILED (hr))
    {
        PostNotice (L"The file dialog could not be opened.");
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

    hr = m_hostDialogs.PickFileToOpen (m_hwnd, spec, chosen, picked);
    CHR (hr);

    BAIL_OUT_IF (!picked, S_OK);

    PostCommand (IDM_FILE_LOAD_STATE, CpuCommandDispatcher::PathToPayload (chosen));

Error:
    if (FAILED (hr))
    {
        PostNotice (L"The file dialog could not be opened.");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveMachineState
//
//  CPU thread. Writes the whole machine, its disks included as the machine
//  holds them, to path. No disk image file is written.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SaveMachineState (const std::filesystem::path & path)
{
    HRESULT            hr        = S_OK;
    std::vector<Byte>  bytes;
    MachineStateError  error;
    std::ofstream      out;
    bool               isOpen    = false;
    bool               isWritten = false;



    hr = MachineStateFile::Build (m_machine, bytes, error);
    CHR (hr);

    out.open (path, std::ios::binary | std::ios::trunc);

    isOpen = out.is_open();
    CBRF (isOpen, error = MachineStateFile::MakeError ("file not written", "The state file could not be created."));

    out.write (reinterpret_cast<const char *> (bytes.data()), static_cast<std::streamsize> (bytes.size()));
    out.close();

    isWritten = !out.fail();
    CBRF (isWritten, error = MachineStateFile::MakeError ("file not written", "The state file could not be written in full."));

    PostNotice (L"Machine state saved to " + path.wstring() + L".");

Error:
    if (FAILED (hr))
    {
        PostNotice (FormatStateError (L"saved", error));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadMachineState
//
//  CPU thread. Reads and checks the whole file before anything changes; a
//  state another machine kind saved opens that machine first, as opening a
//  machine does. History is dropped and starts again at the loaded point,
//  and the debugger hears of it as a machine replaced.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::LoadMachineState (const std::filesystem::path & path)
{
    HRESULT                hr             = S_OK;
    std::vector<Byte>      bytes;
    MachineStateContents   contents;
    MachineStateError      error;
    AppleSpeaker         * speaker        = nullptr;
    bool                   isOtherMachine = false;



    hr = ReadStateFile (path, bytes);
    CHRF (hr, error = MachineStateFile::MakeError ("file not read", "The state file could not be read."));

    hr = MachineStateFile::Parse (bytes, contents, error);
    CHR (hr);

    isOtherMachine = contents.machineName != m_machine.GetCurrentMachineName();

    if (isOtherMachine)
    {
        hr = SwitchMachine (contents.machineName);
        CHRF (hr, error = MachineStateFile::MakeError ("machine not opened", "The machine that saved the state could not be opened."));
    }

    hr = MachineStateFile::Check (m_machine, contents, error);
    CHR (hr);

    m_debugger->StopReverseRecording();

    hr = MachineStateFile::Apply (m_machine, contents, error);

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

    RenderFramebuffer();
    PublishFramebuffer();

    PostNotice (L"Machine state loaded from " + path.wstring() + L".");

Error:
    if (FAILED (hr))
    {
        PostNotice (FormatStateError (L"loaded", error));
    }
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





