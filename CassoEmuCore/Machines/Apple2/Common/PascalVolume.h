#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/SectorReadObserver.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume
//
//  An Apple Pascal filesystem over a flat sector buffer in DOS logical order.
//  Pascal shares ProDOS's mapping of 512-byte blocks onto sectors.
//
//  Blocks 0 and 1 are the boot area, and the directory is blocks 2 to 5: the
//  volume's entry, then the file entries, in order of first block with no gaps
//  between them. Each file is one contiguous run of blocks, and a block no entry
//  covers is free. There is no free map.
//
//  The dates are month, day and year in the low 4, next 5 and top 7 bits.
//
//  Addressing for the integrity pass is one unit per block.
//
////////////////////////////////////////////////////////////////////////////////

class PascalVolume : public IVolume
{
public:
    static constexpr uint32_t  kUnitCount           = 280;
    static constexpr int       kFirstDirectoryBlock = 2;
    static constexpr int       kFirstDataBlock      = 6;
    static constexpr int       kMaxFiles            = 77;
    static constexpr size_t    kBlockBytes          = 512;

    //  The file kinds, as stored in the low four bits of the entry's kind word.
    static constexpr Byte  kKindNone      = 0;
    static constexpr Byte  kKindBad       = 1;
    static constexpr Byte  kKindCode      = 2;
    static constexpr Byte  kKindText      = 3;
    static constexpr Byte  kKindInfo      = 4;
    static constexpr Byte  kKindData      = 5;
    static constexpr Byte  kKindGraf      = 6;
    static constexpr Byte  kKindFoto      = 7;
    static constexpr Byte  kKindSecureDir = 8;

    //  The buffer is accessed by reference and isn't modified.
    explicit PascalVolume (const vector<Byte> & sectors);

    void     SetReadObserver (ISectorReadObserver * observer) { m_readObserver = observer; }

    HRESULT  Enumerate (VolumeListing & outListing) const override;
    HRESULT  Read      (const FilePath & path, FilePayload & outPayload) const override;

    HRESULT  Write     (const FilePath     & path,
                        const FilePayload  & payload,
                        vector<Byte>       & outBuffer) const override;

    HRESULT  Delete    (const FilePath & path, vector<Byte> & outBuffer) const override;

    HRESULT  Delete    (const FilePath  & path,
                        vector<Byte>    & outBuffer,
                        DeleteOutcome   & outOutcome) const override;

    HRESULT  BuildIntegrityReport (VolumeIntegrityReport & outReport) const override;
    HRESULT  SetStartupProgram    (const FilePath & path, vector<Byte> & outBuffer) const override;

    HRESULT  Rename    (const FilePath     & from,
                        const std::string  & to,
                        vector<Byte>       & outBuffer) const override;

    HRESULT  SetAttributes (const FilePath        & path,
                            const FileAttributes  & attributes,
                            vector<Byte>          & outBuffer) const override;

    //  First block 0, next block 6, kind 0, a volume name of 1 to 7 printable
    //  characters, a block count that fits the disk and a file count the
    //  directory has room for.
    static bool  IsFound (const vector<Byte> & sectors);

    //  1 to 15 printable characters with no space or any of $ = ? , : [ ].
    static bool  IsValidName (const std::string & name);

    //  The kind the name's suffix selects, as the Filer does; data otherwise.
    static Byte  GetKindFromName (const std::string & name);

    //  A .TEXT file's lines, without its two-block header, with each indent
    //  code expanded and CR LF line ends.
    static void  DecodeText (std::span<const Byte> bytes, std::string & outText);

    //  Host text as a .TEXT file: the header, then pages of two blocks that no
    //  line crosses. A byte above 127 returns ERROR_INVALID_DATA and its offset.
    static HRESULT  EncodeText (const std::string & text, vector<Byte> & outBytes, size_t & outBadOffset);

private:
    //  One directory entry, with its slot from 1 and its bytes as stored.
    struct RawEntry
    {
        std::array<Byte, 26>  bytes     = {};
        int                   slot      = 0;
        int                   first     = 0;
        int                   next      = 0;
        Byte                  kind      = 0;
        Word                  lastBytes = 0;
        Word                  date      = 0;
        bool                  isValid   = false;
        std::string           name;
    };

    //  The directory as blocks 2 to 5 end to end.
    void  ReadDirectory (vector<Byte> & outDirectory) const;
    void  ReadBlock     (int block, std::span<Byte> outBytes) const;
    void  ReadEntries   (const vector<Byte> & directory, vector<RawEntry> & outEntries, vector<std::string> & outDamage) const;

    //  Only the slots up to `previousCount` are cleared past the last entry.
    static void  WriteDirectory (vector<Byte> & buffer, vector<Byte> & directory, const vector<RawEntry> & entries, int previousCount);
    static void  WriteBlockPart (vector<Byte> & buffer, int block, size_t offset, std::span<const Byte> bytes);

    static bool     TryFindEntry    (const vector<RawEntry> & entries, const std::string & name, size_t & outIndex);
    static int      FindGap         (const vector<RawEntry> & entries, int total, int blocks);
    static void     SetName         (RawEntry & entry, const std::string & name);
    static HRESULT  HandBackResult  (const VolumeIntegrityReport & before, const vector<Byte> & result, vector<Byte> & outBuffer);
    static Word     ToProDosDate    (Word date);
    static Word     FromProDosDate  (Word date);
    static void     SetWord         (std::span<Byte> bytes, size_t offset, Word value);
    static Word     GetWord         (std::span<const Byte> bytes, size_t offset);
    static void     FillEntry       (const RawEntry & raw, size_t index, FileEntry & outEntry);

    //  Sets the kind or date of one entry in a copy of the volume.
    HRESULT  EditEntry (const FilePath & path, const std::function<void (RawEntry &)> & edit, vector<Byte> & outBuffer) const;

    const vector<Byte> &   m_sectors;
    ISectorReadObserver  * m_readObserver = nullptr;
};
