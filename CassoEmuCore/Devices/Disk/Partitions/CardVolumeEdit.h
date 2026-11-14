#pragma once

#include "Pch.h"

#include "Devices/Disk/BlockBuffer.h"
#include "Devices/Disk/IBlockStore.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CardVolumeEdit
//
//  A change to a ProDOS or HFS volume on a partition of a card in a reader,
//  read and written by the block. The volume is read through the card's blocks
//  as the change touches them, and only the blocks the change touched are
//  written back, so a copy onto a 32 MB partition reads and sends a few
//  dozen blocks rather than the partition.
//
//  Any other image, a small partition among them, is read and written
//  whole.
//
////////////////////////////////////////////////////////////////////////////////

class CardVolumeEdit
{
public:
    CardVolumeEdit  ();
    ~CardVolumeEdit ();

    CardVolumeEdit (const CardVolumeEdit &)             = delete;
    CardVolumeEdit & operator= (const CardVolumeEdit &) = delete;

    //  False, with nothing opened, for any image but a ProDOS or HFS volume
    //  on a partition of a card.
    bool  TryOpen (IDiskFileIo & fileIo, const std::string & imagePath);

    const IVolume       & GetVolume () const { return *m_volume; }
    VolumeKind            GetKind   () const { return m_kind; }

    //  The volume's blocks as an image, for a path to be resolved against.
    BlockImage            GetImage  () const;

    //  The volume as `edited` has it, for a change made after another. The
    //  buffer must outlive the volume.
    std::unique_ptr<IVolume>  OpenEdited (const BlockBuffer & edited) const;

    //  The blocks `edited` touched written to the partition, once the
    //  partition is checked to be as it was at TryOpen. `outRefusal` is the
    //  reason for a failure.
    HRESULT  Commit (const BlockBuffer & edited, std::string & outRefusal);

private:
    IDiskFileIo                    * m_fileIo = nullptr;
    std::string                      m_path;
    std::unique_ptr<IBlockStore>     m_store;
    std::unique_ptr<IVolume>         m_volume;
    VolumeKind                       m_kind   = VolumeKind::Unknown;
    FileStamp                        m_stamp;
};
