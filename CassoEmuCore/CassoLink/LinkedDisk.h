#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/SectorReadObserver.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ILinkedSectorSource
//
//  Where a LinkedDisk's sectors come from and where its changes go: the
//  Apple's drive, over the link. A sector is its index in the DOS-logical
//  buffer, track * 16 + sector, and its data is 256 bytes, in the order of
//  the list.
//
////////////////////////////////////////////////////////////////////////////////

class ILinkedSectorSource
{
public:
    virtual ~ILinkedSectorSource () = default;

    virtual HRESULT  ReadSectors  (std::span<const size_t> sectors, std::vector<Byte> & outData) = 0;
    virtual HRESULT  WriteSectors (std::span<const size_t> sectors, std::span<const Byte> data)  = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  LinkedDisk
//
//  A disk in an Apple's drive, as much of it as has been read: a 140 KB
//  DOS-logical sector buffer with a flag per sector for whether it is here.
//
//  An operation runs on the volume over the buffer. A read of an absent
//  sector finds zeros and adds the sector to the miss list; when the run
//  ends with misses, they are fetched in one request and the operation runs
//  again, until a run misses nothing. A catalog or a file takes a few such
//  rounds rather than a whole disk.
//
//  A write runs the same way, then sends only the sectors whose bytes it
//  changed.
//
////////////////////////////////////////////////////////////////////////////////

class LinkedDisk : public ISectorReadObserver
{
public:
    using ReadOperation  = std::function<HRESULT (const IVolume & volume)>;
    using WriteOperation = std::function<HRESULT (const IVolume & volume, std::vector<Byte> & outBuffer)>;

    static constexpr size_t  kSectorCount = 560;
    static constexpr size_t  kSectorSize  = 256;
    static constexpr int     kMaxRounds   = 64;

    explicit LinkedDisk (ILinkedSectorSource & source);

    //  Every sector absent again, as when the drive's disk may have changed.
    void     Clear ();

    //  The catalog's sectors -- the VTOC and track 17 for DOS 3.3, blocks 2
    //  to 6 for ProDOS -- in one request, and the volume's kind from them.
    HRESULT  Prefetch ();

    //  Runs `operation` until a run reads no absent sector. Its result is the
    //  last run's.
    HRESULT  Run   (const ReadOperation & operation);

    //  Runs `operation` the same way and sends the sectors its buffer
    //  changed; the buffer then becomes the disk's sectors. outSent is the
    //  sectors sent.
    HRESULT  Write (const WriteOperation & operation, std::vector<size_t> & outSent);

    //  Fetches the given sectors that are absent, in one request.
    HRESULT  Fetch (std::span<const size_t> sectors);

    VolumeKind                     GetKind     () const { return m_kind; }
    bool                           IsPresent   (size_t sector) const { return sector < kSectorCount && m_present[sector]; }
    const std::vector<Byte>      & GetSectors  () const { return m_sectors; }
    const std::vector<size_t>    & GetMisses   () const { return m_misses; }
    int                            GetRounds   () const { return m_rounds; }

    void     OnSectorRead (size_t sector) override;

private:
    HRESULT  RunRounds (const std::function<HRESULT (const IVolume & volume)> & operation);

    ILinkedSectorSource     & m_source;
    std::vector<Byte>         m_sectors;
    std::vector<bool>         m_present;
    std::vector<size_t>       m_misses;
    VolumeKind                m_kind   = VolumeKind::Unknown;
    int                       m_rounds = 0;
};
