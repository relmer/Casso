#include "Pch.h"

#include "Devices/Disk/ImageClassifier.h"
#include "Shell/DrivePickers.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DrivePickers::DrivePickers
//
////////////////////////////////////////////////////////////////////////////////

DrivePickers::DrivePickers (IDrivePickerFactory & factory, Hooks hooks)
    : m_factory (factory),
      m_hooks   (std::move (hooks))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrivePickers::~DrivePickers
//
////////////////////////////////////////////////////////////////////////////////

DrivePickers::~DrivePickers()
{
    CloseAll();
    ReleaseClosed();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrivePickers::GetViewKey
//
//  One saved view for each kind of drive.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DrivePickers::GetViewKey (ReceivingDriveKind kind)
{
    switch (kind)
    {
        case ReceivingDriveKind::Floppy525:  return L"Floppy525";
        case ReceivingDriveKind::Apple35:    return L"Apple35";
        case ReceivingDriveKind::UniDisk35:  return L"UniDisk35";
        case ReceivingDriveKind::HardDisk:   return L"HardDisk";
        case ReceivingDriveKind::Recorder:   return L"Recorder";
    }

    return std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrivePickers::MakeRequest
//
//  The first filter entry has every disk image extension, or every tape
//  extension for the recorder. Under either entry, an image the drive does
//  not accept is not listed.
//
////////////////////////////////////////////////////////////////////////////////

PickerRequest DrivePickers::MakeRequest (const PickerDrive & drive)
{
    PickerRequest  request;
    bool           isTape = drive.receiving.kind == ReceivingDriveKind::Recorder;



    request.form           = PickerForm::DriveWindow;
    request.title          = std::format (L"{} - {}", isTape ? L"Insert tape" : L"Insert disk", drive.label);
    request.receivingDrive = drive.receiving;
    request.driveLabel     = drive.label;
    request.viewKey        = GetViewKey (drive.receiving.kind);
    request.filters        = { { isTape ? L"Tape recordings" : L"Disk images", isTape ? ImageClassifier::GetTapeMask() : ImageClassifier::GetDiskMask() },
                               { L"All files", L"*.*" } };

    return request;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrivePickers::Find
//
////////////////////////////////////////////////////////////////////////////////

DrivePickers::Entry * DrivePickers::Find (const DriveAddress & address) const
{
    for (const std::unique_ptr<Entry> & entry : m_entries)
    {
        if (!entry->isClosed && entry->drive.address == address)
        {
            return entry.get();
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrivePickers::Open
//
////////////////////////////////////////////////////////////////////////////////

void DrivePickers::Open (const PickerDrive & drive)
{
    Entry                  * open    = Find (drive.address);
    std::unique_ptr<Entry>   entry;
    Entry                  * raw     = nullptr;
    PickerRequest            request;
    bool                     cascade = false;



    if (open != nullptr)
    {
        open->picker->BringToFront();
        return;
    }

    entry        = std::make_unique<Entry>();
    raw          = entry.get();
    entry->drive = drive;

    //  A second picker opens beside the first, not over it.
    cascade = GetOpenCount() > 0;

    request           = MakeRequest (drive);
    request.onConfirm = [this, raw] (const std::wstring & path) { return Confirm (*raw, path); };

    if (m_hooks.getCreateNew)
    {
        std::function<bool (std::wstring & outPath)>  create = m_hooks.getCreateNew (drive);

        //  A create flow that returns an empty path inserted what it made.
        if (create)
        {
            request.onCreateNew = [raw, create] (std::wstring & outPath)
            {
                bool  isCreated = create (outPath);

                raw->isInserted = raw->isInserted || (isCreated && outPath.empty());

                return isCreated;
            };
        }
    }

    if (m_hooks.getStockDisks)
    {
        request.stockDisks   = m_hooks.getStockDisks (drive);
        request.onFetchStock = m_hooks.fetchStockDisk;
    }

    if (m_hooks.getMounted)
    {
        request.mounted = m_hooks.getMounted();
    }

    m_entries.push_back (std::move (entry));

    if (m_hooks.onOpened)
    {
        m_hooks.onOpened (drive.address);
    }

    raw->picker = m_factory.OpenPicker (std::move (request), cascade, [this, raw] ()
    {
        raw->isClosed = true;

        if (m_hooks.onClosed)
        {
            m_hooks.onClosed (raw->drive.address, raw->isInserted);
        }
    });

    if (raw->picker == nullptr && !raw->isClosed)
    {
        raw->isClosed = true;

        if (m_hooks.onClosed)
        {
            m_hooks.onClosed (drive.address, false);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrivePickers::Confirm
//
//  True closes the picker.
//
////////////////////////////////////////////////////////////////////////////////

bool DrivePickers::Confirm (Entry & entry, const std::wstring & path)
{
    std::wstring  error;



    if (m_hooks.confirmDiscard && !m_hooks.confirmDiscard (entry.drive.address))
    {
        return false;
    }

    if (!m_hooks.insert || !m_hooks.insert (entry.drive, path, error))
    {
        if (m_hooks.reportFailure && !error.empty())
        {
            m_hooks.reportFailure (error);
        }

        return false;
    }

    entry.isInserted = true;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrivePickers::Close
//
////////////////////////////////////////////////////////////////////////////////

void DrivePickers::Close (const DriveAddress & address)
{
    Entry *  entry = Find (address);



    if (entry != nullptr && entry->picker != nullptr)
    {
        entry->picker->Close();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrivePickers::CloseAll
//
//  By index: a close does not change the list, which ReleaseClosed does.
//
////////////////////////////////////////////////////////////////////////////////

void DrivePickers::CloseAll()
{
    for (size_t i = 0; i < m_entries.size(); i++)
    {
        if (!m_entries[i]->isClosed && m_entries[i]->picker != nullptr)
        {
            m_entries[i]->picker->Close();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrivePickers::ReleaseClosed
//
////////////////////////////////////////////////////////////////////////////////

void DrivePickers::ReleaseClosed()
{
    std::erase_if (m_entries, [] (const std::unique_ptr<Entry> & entry) { return entry->isClosed; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrivePickers::IsOpenFor
//
////////////////////////////////////////////////////////////////////////////////

bool DrivePickers::IsOpenFor (const DriveAddress & address) const
{
    return Find (address) != nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrivePickers::GetOpenCount
//
////////////////////////////////////////////////////////////////////////////////

size_t DrivePickers::GetOpenCount() const
{
    return (size_t) std::count_if (m_entries.begin(), m_entries.end(), [] (const std::unique_ptr<Entry> & entry) { return !entry->isClosed; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrivePickers::IsPickerWindow
//
//  A picker is owned by the emulator's window, so the walk up the owners
//  stops at the first picker found.
//
////////////////////////////////////////////////////////////////////////////////

bool DrivePickers::IsPickerWindow (HWND hwnd) const
{
    HWND  root = nullptr;



    if (m_entries.empty())
    {
        return false;
    }

    while (hwnd != nullptr)
    {
        root = GetAncestor (hwnd, GA_ROOT);
        root = (root != nullptr) ? root : hwnd;

        for (const std::unique_ptr<Entry> & entry : m_entries)
        {
            if (!entry->isClosed && entry->picker != nullptr && entry->picker->GetHwnd() == root)
            {
                return true;
            }
        }

        hwnd = GetWindow (root, GW_OWNER);
    }

    return false;
}
