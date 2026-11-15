#pragma once

#include "Pch.h"

#include "Shell/DrivePickers.h"
#include "Ui/Scene/DeskDrivePlan.h"

class EmulatorShell;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDrivePickers
//
//  The emulator's drive pickers: Casso Explorer windows in picker mode,
//  one for each drive that was clicked, owned by the emulator's window and
//  run by its message loop. A pick is inserted in the drive the picker was
//  opened for: mounted in a floppy drive, loaded in the recorder, or staged
//  for a hard disk.
//
//  The pickers close on a machine switch and when the emulator's window
//  closes, since their drives may be gone after either.
//
////////////////////////////////////////////////////////////////////////////////

class ShellDrivePickers : public IDrivePickerFactory
{
public:
    explicit ShellDrivePickers (EmulatorShell & shell);
    ~ShellDrivePickers() override;

    //  A Disk II drive in slot 6, by its index.
    void  OpenForDiskII    (int drive);
    void  OpenForDeskDrive (const DeskDrive & drive);
    void  OpenForRecorder  ();

    void  CloseAll         ()                             { m_pickers.CloseAll(); }
    void  Close            (const DriveAddress & address) { m_pickers.Close (address); }

    //  Called by the message loop between messages.
    void  ReleaseClosed    ()                             { m_pickers.ReleaseClosed(); }

    //  Whether a message is for a picker, which the emulator's accelerators
    //  are not applied to.
    bool  IsPickerWindow   (HWND hwnd) const              { return m_pickers.IsPickerWindow (hwnd); }

    DrivePickers &  GetPickers () { return m_pickers; }

    //  IDrivePickerFactory
    std::unique_ptr<IDrivePicker>  OpenPicker (PickerRequest request, bool cascade, std::function<void ()> onClosed) override;

    static constexpr const wchar_t *  kRecorderLabel = L"Cassette recorder";

private:
    DrivePickers::Hooks  MakeHooks      ();
    bool                 Insert         (const PickerDrive & drive, const std::wstring & path, std::wstring & outError);
    bool                 FetchStockDisk (const std::wstring & path);

    EmulatorShell  & m_shell;
    DrivePickers     m_pickers;
};
