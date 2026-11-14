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
        CHRF (hr, outRefusal = "could not be written");
    }

    hr = m_store->Commit();
    CHRF (hr, outRefusal = "could not be written");

Error:
    return hr;
}
