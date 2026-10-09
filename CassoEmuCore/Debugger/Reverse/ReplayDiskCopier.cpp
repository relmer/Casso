#include "Pch.h"

#include "Debugger/Reverse/ReplayDiskCopier.h"

#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/DiskImageStore.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayDiskCopier::Copy
//
//  A disk its format cannot hold fails the copy.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReplayDiskCopier::Copy (
    DiskImageStore           & store,
    std::vector<ReplayDisk>  & outDisks)
{
    HRESULT                                                                  hr       = S_OK;
    DiskImage                                                              * image    = nullptr;
    std::unordered_map<uint64_t, std::shared_ptr<const std::vector<Byte>>>   kept;
    std::shared_ptr<std::vector<Byte>>                                       bytes;
    ReplayDisk                                                               disk;
    int                                                                      slot     = 0;
    int                                                                      drive    = 0;
    bool                                                                     isSector = false;



    outDisks.clear();

    if (m_blankSectorImage == nullptr)
    {
        m_blankSectorImage = std::make_shared<const std::vector<Byte>> (DiskImage::kDos33ImageSize, (Byte) 0);
    }

    for (slot = 0; slot < DiskImageStore::kSlotCount; slot++)
    {
        for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
        {
            image = store.IsMounted (slot, drive) ? store.GetImage (slot, drive) : nullptr;

            if (image == nullptr)
            {
                continue;
            }

            disk            = ReplayDisk();
            disk.slot       = slot;
            disk.drive      = drive;
            disk.mediaId    = store.GetMediaId (slot, drive);
            disk.trackCount = image->GetTrackCount();
            disk.format     = image->GetSourceFormat();
            isSector        = disk.format == DiskFormat::Dsk || disk.format == DiskFormat::Do || disk.format == DiskFormat::Po;

            if (isSector)
            {
                disk.image = m_blankSectorImage;
            }
            else if (m_images.contains (disk.mediaId))
            {
                disk.image = m_images[disk.mediaId];
            }
            else
            {
                bytes = std::make_shared<std::vector<Byte>>();
                CPRA (bytes);

                hr = image->Serialize (*bytes);
                CHR (hr);

                disk.image = bytes;
            }

            if (!isSector)
            {
                kept[disk.mediaId] = disk.image;
            }

            outDisks.push_back (disk);
        }
    }

    m_images.swap (kept);

Error:
    return hr;
}





