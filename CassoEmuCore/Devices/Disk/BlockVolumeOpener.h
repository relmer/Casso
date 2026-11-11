#pragma once

#include "Pch.h"

#include "Devices/Disk/IBlockStore.h"
#include "Machines/Apple2/Common/VolumeImage.h"

class IDiskFileIo;
class IVolume;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockVolumeOpener
//
//  A large ProDOS or HFS volume of whole blocks, read through its file as
//  its blocks are touched rather than read whole: a .po, a .hdv, a .2mg in
//  ProDOS order, or a partition on a storage card.
//
//  A path with a container step, as a file inside an archive, is read whole,
//  as is an image smaller than kMinBytes, where a whole read costs nothing. A
//  file system chosen for the image other than ProDOS or HFS is honored.
//
////////////////////////////////////////////////////////////////////////////////

class BlockVolumeOpener
{
public:
    static constexpr uint64_t  kMinBytes = 1024 * 1024;

    //  False, with nothing opened, for any other image. `outStore` must
    //  outlive `outVolume`.
    static bool  TryOpen (IDiskFileIo                    & fileIo,
                          const std::string              & imagePath,
                          VolumeKind                       choice,
                          std::unique_ptr<IBlockStore>   & outStore,
                          std::unique_ptr<IVolume>       & outVolume,
                          VolumeKind                     & outKind);

private:
    //  The .2mg header's fields: the image format, where the volume starts
    //  and its length.
    static constexpr size_t    s_k2mgHeaderBytes = 64;
    static constexpr size_t    s_k2mgFormat      = 0x0C;
    static constexpr size_t    s_k2mgDataOffset  = 0x18;
    static constexpr size_t    s_k2mgDataLength  = 0x1C;
    static constexpr uint32_t  s_k2mgProDosOrder = 1;

    //  A ProDOS volume's directory header, the key block's first entry, and
    //  its storage type in the high nibble.
    static constexpr size_t    s_kVolumeHeaderAt   = 2 * 512 + 4;
    static constexpr Byte      s_kVolumeHeaderType = 0xF0;

    static uint32_t  ReadLong (const std::vector<Byte> & bytes, size_t at);
};
