#pragma once

#include "Pch.h"

#include "Core/IMachineState.h"
#include "Devices/Disk/DiskImageStore.h"

class Prng;
class StateReader;
class StateWriter;
struct MachineConfig;





////////////////////////////////////////////////////////////////////////////////
//
//  IMachineStateParts
//
//  What a machine's state stream reads from the machine it is for: the
//  parts in their save order, the ROM identity, the power-on Prng, and the
//  drives to point at the bays once the saved disks are back in them.
//
////////////////////////////////////////////////////////////////////////////////

class IMachineStateParts
{
public:
    virtual ~IMachineStateParts() = default;

    virtual void      GetStateParts  (std::vector<IMachineState *> & outParts) = 0;
    virtual uint64_t  GetRomIdentity () const                                 = 0;
    virtual Prng    * GetStatePrng   () const                                 = 0;
    virtual void      BindDiskDrives ()                                       = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateStream
//
//  The whole machine's state as one stream, and back: a header with the
//  machine's name, its ROM and hardware identities, the disk in each drive
//  bay, the part count and the power-on Prng, then each part's section in
//  the save order. A state from another machine, ROM set or hardware fails
//  to load with ERROR_INVALID_DATA.
//
////////////////////////////////////////////////////////////////////////////////

class MachineStateStream
{
public:
    //  The disk each drive bay holds, slot-major, as a snapshot records it,
    //  zero for an empty bay.
    using MediaIds = std::array<uint64_t, DiskImageStore::kSlotCount * DiskImageStore::kDriveCount>;

    static constexpr uint32_t  kStateTag           = IMachineState::MakeTag ('M', 'A', 'C', 'H');
    static constexpr uint16_t  kStateVersion       = 3;
    static constexpr uint16_t  kOldestStateVersion = 3;   // a state before version 3 has no hardware identity, so it isn't known to match

    MachineStateStream (IMachineStateParts & parts, DiskImageStore & disks, const MachineConfig & config, const std::wstring & name);

    MachineStateStream (const MachineStateStream &)             = delete;
    MachineStateStream & operator= (const MachineStateStream &) = delete;

    //  See MachineHost::SaveState, LoadState, LoadStateOverMountedMedia and
    //  LoadStateForPicture.
    HRESULT  SaveState                 (StateWriter & writer) const;
    HRESULT  LoadState                 (StateReader & reader);
    HRESULT  LoadStateOverMountedMedia (StateReader & reader);
    HRESULT  LoadStateForPicture       (StateReader & reader);

    static HRESULT  ReadSavedMedia  (const std::vector<Byte> & state, MediaIds & outIds);
    static void     CheckSharedSave (const StateWriter & writer);

private:
    void            WriteStateHeader  (StateWriter & writer, size_t partCount) const;
    HRESULT         LoadStateSeating  (StateReader & reader, bool isOverMounted);
    HRESULT         CheckStateHeader  (StateReader & reader, bool isOverMounted, uint32_t & outParts);
    HRESULT         ReadPictureHeader (StateReader & reader, uint32_t & outParts) const;
    static HRESULT  GetPartTag        (const IMachineState & part, uint32_t & outTag);
    bool            CanSeatMedia      (const MediaIds & mediaIds) const;
    bool            AreBaysAsSaved    (const MediaIds & mediaIds) const;
    HRESULT         SeatMedia         (const MediaIds & mediaIds);

    IMachineStateParts    & m_parts;
    DiskImageStore        & m_disks;
    const MachineConfig   & m_config;
    const std::wstring    & m_name;

    // The part list a save or load fills; kept so it keeps its capacity.
    mutable std::vector<IMachineState *>  m_stateParts;
};
