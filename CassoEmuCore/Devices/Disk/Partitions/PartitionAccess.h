#pragma once

#include "Pch.h"

#include "Devices/Disk/Partitions/PartitionLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionEntry
//
//  One partition of an image file, and what a list of partitions displays for
//  it.
//
////////////////////////////////////////////////////////////////////////////////

struct PartitionEntry
{
    Partition    partition;

    //  The partition's step of a container path: `#3.po` for a volume of
    //  blocks, `#3.do` for a DOS volume in track and sector order.
    std::string  step;

    uint64_t     byteCount       = 0;

    //  Empty for a volume with no name, and for a partition with no file
    //  system Casso reads.
    std::string  volumeName;
    bool         hasVolumeNumber = false;
    Byte         volumeNumber    = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionAccess
//
//  The partitions of an image file, one partition's bytes as an image file,
//  and the image file with one partition's bytes replaced.
//
//  A partition's bytes are the file a `.po` or `.do` image of that volume
//  has: blocks in order for a volume of blocks, and sectors in track and
//  sector order for a DOS volume.
//
//  The file's bytes are its blocks when the file is a block image larger
//  than a 5.25" disk. Any other file is loaded as a disk image, and saved in
//  the form it was loaded from.
//
////////////////////////////////////////////////////////////////////////////////

class PartitionAccess
{
public:
    //  A gzip file is expanded first. A file with no partitions has an empty
    //  list and PartitionLayoutKind::None.
    static HRESULT  List (const std::vector<Byte>      & fileBytes,
                          const std::string            & name,
                          PartitionLayoutKind          & outKind,
                          std::vector<PartitionEntry>  & outEntries);

    //  The partitions of a store of blocks, such as a card in a reader.
    static HRESULT  ListStore (IBlockStore & store, PartitionLayoutKind & outKind, std::vector<PartitionEntry> & outEntries);

    //  The partitions of a storage card. A card of one CFFA slice or less that
    //  is one ProDOS or HFS volume is a CFFA card of one partition, as the
    //  CFFA firmware maps it.
    static HRESULT  ListCard  (IBlockStore & store, PartitionLayoutKind & outKind, std::vector<PartitionEntry> & outEntries);

    static constexpr uint32_t  kCffaSliceBlocks = 65536;

    //  One partition's bytes, read from and written to a store of blocks.
    //  WriteBytes takes the partition's whole length, and does not commit.
    static HRESULT  ReadBytes  (IBlockStore & store, const Partition & partition, std::vector<Byte> & outBytes);
    static HRESULT  WriteBytes (IBlockStore & store, const Partition & partition, const std::vector<Byte> & bytes);

    static std::string  GetStepName      (const Partition & partition);
    static bool         TryParseStepName (const std::string & step, int & outIndex);
    static uint64_t     GetByteCount     (const Partition & partition);

    //  "Partition 2: GAMES", and with the size, "Partition 2: GAMES (32 MB)".
    //  The text after the colon is the volume's name, the map's name for the
    //  partition, or a DOS volume's number.
    static std::string  GetLabel         (const PartitionEntry & entry, bool withSize);
    static std::string  FormatSize       (uint64_t byteCount);

    static HRESULT  Extract (const std::vector<Byte>  & fileBytes,
                             const std::string        & name,
                             int                        index,
                             std::vector<Byte>        & outPartitionBytes,
                             std::string              & outError);

    //  `partitionBytes` is written over the start of the partition, and may
    //  be shorter than it.
    static HRESULT  Replace (const std::vector<Byte>  & fileBytes,
                             const std::string        & name,
                             int                        index,
                             std::span<const Byte>      partitionBytes,
                             std::vector<Byte>        & outFileBytes,
                             std::string              & outError);

    //  The bytes of a disk image file as the bytes Replace writes over the
    //  partition.
    static HRESULT  ConvertImage (const Partition          & partition,
                                  const std::vector<Byte>  & imageBytes,
                                  const std::string        & imageName,
                                  std::vector<Byte>        & outPartitionBytes,
                                  std::string              & outError);

private:
    static constexpr size_t  kFloppyBytes = 143360;

    static HRESULT  OpenBlocks (const std::vector<Byte> & fileBytes, const std::string & name, std::vector<Byte> & outBlocks, bool & outIsRaw);

    static HRESULT  FindPartition (IBlockStore        & store,
                                   const std::string  & name,
                                   int                  index,
                                   Partition          & outPartition,
                                   std::string        & outError);

    static void         ReadVolumeLabel (IBlockStore & store, PartitionEntry & ioEntry);
    static std::string  GetLeafName     (const std::string & path);
};
