#include "Pch.h"

#include "Devices/Disk/Partitions/CardVolumeEdit.h"
#include "Devices/Disk/BlockVolumeOpener.h"
#include "Devices/Disk/CommitPlan.h"
#include "Devices/Disk/Partitions/PhysicalCards.h"
#include "Machines/Apple2/Common/HfsVolume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CardVolumeEdit::CardVolumeEdit
//
////////////////////////////////////////////////////////////////////////////////

CardVolumeEdit::CardVolumeEdit() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  CardVolumeEdit::~CardVolumeEdit
//
//  The volume goes before the store it reads.
//
////////////////////////////////////////////////////////////////////////////////

CardVolumeEdit::~CardVolumeEdit()
{
    m_volume.reset();
    m_store.reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardVolumeEdit::TryOpen
//
////////////////////////////////////////////////////////////////////////////////

bool CardVolumeEdit::TryOpen (IDiskFileIo & fileIo, const std::string & imagePath)
{
    int         drive     = 0;
    int         partition = 0;
    VolumeKind  kind      = VolumeKind::Unknown;
    HRESULT     hr        = S_OK;
    bool        opened    = false;



    if (PhysicalCards::ParsePath (imagePath, drive, partition) != PhysicalCards::PathKind::Partition)
    {
        return false;
    }

    opened = BlockVolumeOpener::TryOpen (fileIo, imagePath, VolumeKind::Unknown, m_store, m_volume, kind);
    hr     = opened ? fileIo.Stat (imagePath, m_stamp) : E_FAIL;

    if (!opened || (kind != VolumeKind::ProDos && kind != VolumeKind::Hfs) || FAILED (hr))
    {
        m_volume.reset();
        m_store.reset();
        return false;
    }

    m_fileIo = &fileIo;
    m_path   = imagePath;
    m_kind   = kind;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardVolumeEdit::GetImage
//
////////////////////////////////////////////////////////////////////////////////

BlockImage CardVolumeEdit::GetImage() const
{
    return BlockImage (*m_store);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardVolumeEdit::OpenEdited
//
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<IVolume> CardVolumeEdit::OpenEdited (const BlockBuffer & edited) const
{
    if (m_kind == VolumeKind::Hfs)
    {
        return std::make_unique<HfsVolume> (BlockImage (edited));
    }

    return std::make_unique<ProDosVolume> (BlockImage (edited));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardVolumeEdit::Commit
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CardVolumeEdit::Commit (const BlockBuffer & edited, std::string & outRefusal)
{
    HRESULT    hr       = S_OK;
    FileStamp  observed;
    bool       stale    = false;



    outRefusal.clear();

    hr = m_fileIo->Stat (m_path, observed);
    CHRF (hr, outRefusal = "has gone away since it was read");

    stale = CommitPlan::IsStale (m_stamp.sizeBytes, m_stamp.modifiedUnix, observed.sizeBytes, observed.modifiedUnix);
    CBRFEx (!stale, STG_E_NOTCURRENT, outRefusal = "changed since it was read. Nothing was written; read it again and retry");

    for (const auto & [block, bytes] : edited.GetTouchedBlocks())
    {
        hr = m_store->WriteBlocks (block, bytes);
        CHRF (hr, outRefusal = DescribeWriteFailure (hr));
    }

    hr = m_store->Commit();
    CHRF (hr, outRefusal = DescribeWriteFailure (hr));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardVolumeEdit::GetLabel
//
////////////////////////////////////////////////////////////////////////////////

std::string CardVolumeEdit::GetLabel() const
{
    int            drive     = 0;
    int            partition = 0;
    VolumeListing  listing;
    HRESULT        hr        = S_OK;
    std::string    label;



    PhysicalCards::ParsePath (m_path, drive, partition);

    label = std::format ("Partition {}", partition);
    hr    = m_volume->Enumerate (listing);

    if (SUCCEEDED (hr) && !listing.volumeName.empty())
    {
        label += ": " + listing.volumeName;
    }

    return label;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardVolumeEdit::DescribeWriteFailure
//
////////////////////////////////////////////////////////////////////////////////

std::string CardVolumeEdit::DescribeWriteFailure (HRESULT hr)
{
    std::string  sentence = "could not be written";



    if (hr == E_ACCESSDENIED || hr == HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED))
    {
        sentence += ". The storage card broker writes only inside an Apple partition it finds on the card, "
                    "and not over a volume Windows has mounted";
    }
    else if (hr == HRESULT_FROM_WIN32 (ERROR_DEVICE_NOT_CONNECTED))
    {
        sentence += ". The card is no longer open. Open it again and retry";
    }
    else if (hr == HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE) || hr == HRESULT_FROM_WIN32 (ERROR_NO_DATA) ||
             hr == HRESULT_FROM_WIN32 (ERROR_TIMEOUT)     || hr == HRESULT_FROM_WIN32 (WAIT_TIMEOUT))
    {
        sentence += ". The storage card broker stopped responding";
    }
    else if (hr == HRESULT_FROM_WIN32 (ERROR_WRITE_FAULT) || hr == HRESULT_FROM_WIN32 (ERROR_CRC) ||
             hr == HRESULT_FROM_WIN32 (ERROR_IO_DEVICE))
    {
        sentence += ". The card reported a write error";
    }
    else if (hr == HRESULT_FROM_WIN32 (ERROR_WRITE_PROTECT))
    {
        sentence += ". The card is write-protected";
    }

    return sentence;
}




