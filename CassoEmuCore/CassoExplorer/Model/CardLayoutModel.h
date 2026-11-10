#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutModel
//
//  The choices for initializing a storage card, as positions along a bar:
//  1 to GetVolumeLimit() CFFA volumes of 32 MB, then FAT32 for a CFFA3000
//  when the card is large enough. For a position, the kind of each part of
//  the bar and each CFFA adapter's support.
//
//  The adapters' support is from the CFFA 1.x and 2.0 manuals: CFFA 1.x
//  maps four volumes, or eight with the 8-drive version of firmware 1.2, and
//  its GS/OS driver reads the space after the fourth as 1 GB drives. CFFA
//  2.0 maps up to 13 with its 65C02 firmware once its partition count is
//  set, and two, or four in slot 5, with its 6502 firmware.
//
////////////////////////////////////////////////////////////////////////////////

class CardLayoutModel
{
public:
    static constexpr uint64_t  kSliceBytes       = 32ull * 1024 * 1024;
    static constexpr int       kMaxVolumes       = 13;
    static constexpr int       kUniversalVolumes = 4;

    enum class Segment { Universal, NeedsSetup, Unused, Unavailable, Fat32 };
    enum class Support { Works, Step, Fails, Unknown };

    struct Row
    {
        std::wstring  adapter;
        Support       support = Support::Unknown;
        std::wstring  note;
    };

    CardLayoutModel (uint64_t bytes, int volumeLimit, bool canFat32);

    int   GetVolumeLimit  () const { return m_volumeLimit; }
    bool  CanFat32        () const { return m_canFat32; }

    //  Positions are 1 to GetPositionCount(). The last is FAT32 when the card
    //  is large enough for it.
    int   GetPositionCount   () const { return m_volumeLimit + (m_canFat32 ? 1 : 0); }
    bool  IsFat32            (int position) const { return m_canFat32 && position == m_volumeLimit + 1; }
    int   GetDefaultPosition () const { return std::min (kUniversalVolumes, m_volumeLimit); }

    //  The layout code RawDeviceProtocol takes for a position.
    uint64_t  GetLayout   (int position) const;

    //  The kind of volume slot `slot`, 1 to GetVolumeLimit(), at a position.
    Segment   GetSlotKind (int position, int slot) const;

    //  The space past the last slot the CFFA layout maps.
    uint64_t  GetUnavailableBytes () const;

    std::wstring  DescribePosition (int position) const;
    std::wstring  DescribeSpace    (int position) const;
    void          GetRows          (int position, std::vector<Row> & outRows) const;

private:
    uint64_t  m_bytes       = 0;
    int       m_volumeLimit = 0;
    bool      m_canFat32    = false;
};
