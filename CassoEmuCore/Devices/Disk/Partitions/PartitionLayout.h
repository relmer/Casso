#pragma once

#include "Pch.h"

#include "Devices/Disk/IBlockStore.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionLayoutKind
//
////////////////////////////////////////////////////////////////////////////////

enum class PartitionLayoutKind
{
    None,
    Cffa,
    FocusDrive,
    MicroDrive,
    DosMaster,
    AmDos,
    UniDos,
    OzDos,
    Ppm,
};





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionSpan
//
//  Which bytes of its blocks a partition has. An OzDOS disk has two volumes
//  on the same blocks: one in the first 256 bytes of each block and one in
//  the last 256.
//
////////////////////////////////////////////////////////////////////////////////

enum class PartitionSpan
{
    WholeBlocks,
    FirstHalves,
    SecondHalves,
};





////////////////////////////////////////////////////////////////////////////////
//
//  Partition
//
//  One volume of a partitioned card or disk. Partitions don't overlap except
//  in a hybrid, whose file systems share one buffer.
//
////////////////////////////////////////////////////////////////////////////////

struct Partition
{
    //  Counted from 1, in the order of the layout's map.
    int                  index      = 0;
    uint32_t             firstBlock = 0;
    uint32_t             blockCount = 0;
    PartitionLayoutKind  layout     = PartitionLayoutKind::None;
    VolumeKind           volumeKind = VolumeKind::Unknown;

    //  True when the map has the partition ending past the last block of the
    //  store. `blockCount` is the part inside it.
    bool                 damaged    = false;

    //  The name or description the map has for the partition, when it has one.
    std::string          name;

    PartitionSpan        span       = PartitionSpan::WholeBlocks;

    //  True for a DOS volume whose sectors are in track and sector order on
    //  the blocks, false for a volume of blocks.
    bool                 dosSectors = false;

    //  True for an HFS volume, which has no volume kind yet.
    bool                 isHfs      = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionLayout
//
//  The partitions of a block store, and one partition's bytes as the buffer
//  the volume layer reads: DOS logical order for a 140K volume, track and
//  sector order for a larger DOS volume, block order for any other.
//
////////////////////////////////////////////////////////////////////////////////

class PartitionLayout
{
public:
    static constexpr uint32_t  kFloppyBlocks = 280;

    //  For a store with no partitions, `outKind` is PartitionLayoutKind::None and
    //  the list is empty.
    static HRESULT  Detect (IBlockStore & store, PartitionLayoutKind & outKind, std::vector<Partition> & outPartitions);

    static HRESULT  ReadPartition  (IBlockStore & store, const Partition & partition, std::vector<Byte> & outSectors);

    //  `IBlockStore::Commit` is not called.
    static HRESULT  WritePartition (IBlockStore & store, const Partition & partition, const std::vector<Byte> & sectors);

    static const char *  GetLayoutName (PartitionLayoutKind kind);

    //  Blocks of the store as bytes, and ERROR_HANDLE_EOF for a run that ends
    //  past the last block.
    static HRESULT  ReadBlocks (IBlockStore & store, uint32_t first, uint32_t count, std::vector<Byte> & outBytes);

    static Word      ReadWord (const std::vector<Byte> & bytes, size_t offset);
    static uint32_t  ReadLong (const std::vector<Byte> & bytes, size_t offset);

    //  Appends a partition of whole blocks, clipped to the store, with the
    //  volume kind its first blocks indicate.
    static HRESULT  AppendBlockPartition (IBlockStore             & store,
                                          PartitionLayoutKind       layout,
                                          uint32_t                  firstBlock,
                                          uint32_t                  blockCount,
                                          const std::string       & name,
                                          std::vector<Partition>  & ioPartitions);

    //  The volume kind of a run of blocks, from its volume directory or its
    //  HFS master directory block. `outVolumeBlocks` is the block count in
    //  that directory.
    static HRESULT  ProbeBlockVolume (IBlockStore & store,
                                      uint32_t      firstBlock,
                                      uint32_t      blockCount,
                                      VolumeKind  & outKind,
                                      bool        & outIsHfs,
                                      uint32_t    & outVolumeBlocks);

    //  True when the partition's bytes are a DOS 3.x volume of the tracks and
    //  sectors given.
    static HRESULT  IsDosVolume (IBlockStore & store, const Partition & partition, int tracks, int sectorsPerTrack, bool & outIsDos);

private:
    static bool  IsProDosDirectory (const std::vector<Byte> & blocks, uint32_t blockCount, uint32_t & outVolumeBlocks);
    static bool  IsHfsDirectory    (const std::vector<Byte> & blocks, uint32_t blockCount, uint32_t & outVolumeBlocks);
};