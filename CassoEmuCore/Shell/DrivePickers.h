#pragma once

#include "Pch.h"

#include "CassoExplorer/Picker/PickerRequest.h"
#include "Devices/Disk/DriveImageRule.h"
#include "Shell/DriveDropRule.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PickerDrive
//
//  A drive a picker is opened for: its address, what it accepts, and its
//  label, such as "UniDisk 3.5 - S5,D1".
//
////////////////////////////////////////////////////////////////////////////////

struct PickerDrive
{
    DriveAddress    address;
    ReceivingDrive  receiving;
    std::wstring    label;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IDrivePicker
//
//  One open picker window.
//
////////////////////////////////////////////////////////////////////////////////

class IDrivePicker
{
public:
    virtual ~IDrivePicker() = default;

    virtual HWND  GetHwnd      () const = 0;
    virtual void  BringToFront () = 0;

    //  As the close box: nothing is inserted, and the closed callback runs.
    virtual void  Close        () = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IDrivePickerFactory
//
//  Opens a picker window for a request. `onClosed` is called once, when the
//  picker has ended, from inside a message of the picker's window. Null when the
//  window could not be created.
//
////////////////////////////////////////////////////////////////////////////////

class IDrivePickerFactory
{
public:
    virtual ~IDrivePickerFactory() = default;

    virtual std::unique_ptr<IDrivePicker>  OpenPicker (PickerRequest request, bool cascade, std::function<void ()> onClosed) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DrivePickers
//
//  The emulator's open drive pickers: at most one for each drive. A picker
//  inserts in the drive it was opened for until it closes. Opening a drive
//  whose picker is open brings that picker to the front, and opening
//  another drive opens another picker.
//
//  On a confirm in a picker the user is asked about unsaved inspector edits,
//  and then the image is inserted in the picker's drive. The picker closes
//  when the insert was started and stays open otherwise. A closed picker is
//  released by ReleaseClosed, which the message loop calls between messages.
//
////////////////////////////////////////////////////////////////////////////////

class DrivePickers
{
public:
    struct Hooks
    {
        //  False when the user chooses not to replace the disk whose
        //  inspector edits are unsaved.
        std::function<bool (const DriveAddress & address)>                               confirmDiscard;

        //  Inserts the image, or stages it for a hard disk. False with the
        //  reason when nothing was started.
        std::function<bool (const PickerDrive & drive, const std::wstring & path, std::wstring & outError)>  insert;
        std::function<void (const std::wstring & text)>                                  reportFailure;

        //  The drive's door: opened with the picker, and on its close either
        //  left to the insert or put back.
        std::function<void (const DriveAddress & address)>                               onOpened;
        std::function<void (const DriveAddress & address, bool isInserted)>              onClosed;

        //  The create flow for the drive, or a null function for a drive
        //  without one.
        std::function<std::function<bool (std::wstring & outPath)> (const PickerDrive & drive)>  getCreateNew;

        //  The stock disks the drive takes, and the download of one that
        //  is not on the disk yet; false with the reason when it could not
        //  be downloaded.
        std::function<std::vector<StockDiskRow> (const PickerDrive & drive)>             getStockDisks;
        std::function<bool (const std::wstring & path, std::wstring & outError)>         fetchStockDisk;
        std::function<std::map<std::wstring, std::wstring> ()>                           getMounted;
    };

    DrivePickers (IDrivePickerFactory & factory, Hooks hooks);
    ~DrivePickers();

    void    Open           (const PickerDrive & drive);
    void    Close          (const DriveAddress & address);
    void    CloseAll       ();
    void    ReleaseClosed  ();

    bool    IsOpenFor      (const DriveAddress & address) const;
    size_t  GetOpenCount   () const;

    //  Whether a message for `hwnd` is for a picker: the window itself, a
    //  child of it, or a popup one of them owns.
    bool    IsPickerWindow (HWND hwnd) const;

    //  The request for a drive: its title, filter entries, view key and
    //  receiving drive.
    static PickerRequest  MakeRequest (const PickerDrive & drive);
    static std::wstring   GetViewKey  (ReceivingDriveKind kind);

private:
    struct Entry
    {
        PickerDrive                    drive;
        std::unique_ptr<IDrivePicker>  picker;
        bool                           isClosed   = false;
        bool                           isInserted = false;
    };

    Entry *  Find    (const DriveAddress & address) const;
    bool     Confirm (Entry & entry, const std::wstring & path);

    IDrivePickerFactory                  & m_factory;
    Hooks                                  m_hooks;
    std::vector<std::unique_ptr<Entry>>    m_entries;
};
