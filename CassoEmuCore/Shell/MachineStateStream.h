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
    static constexpr uint16_t  kStateVersion       = 4;
    static constexpr uint16_t  kFourDriveVersion   = 4;   // the first with four drives per slot; before it, two
    static constexpr uint16_t  kOldestStateVersion = 3;   // a state before version 3 has no hardware identity, so it isn't known to match

    MachineStateStream (IMachineStateParts & parts, DiskImageStore & disks, const MachineConfig & config, const std::wstring & name);

    MachineStateStream (const MachineStateStream &)             = delete;
    MachineStateStream & operator= (const MachineStateStream &) = delete;

    //  The whole machine's state as one stream, and back. The stream opens
    //  with the machine's name and ROM identity, so a state from another
    //  machine or another ROM set fails to load with ERROR_INVALID_DATA. The
    //  machine must already be built with the same devices and the same disks
    //  mounted; after a failed load it is unusable. Call both on the thread
    //  that runs the machine.
    HRESULT  SaveState                 (StateWriter & writer) const;
    HRESULT  LoadState                 (StateReader & reader);

    //  The same load for a state read back from a file, in a session that
    //  never had the saved disks: the caller has mounted them, and each bay
    //  that had a disk at the save must have one now, and each empty bay
    //  must be empty.
    HRESULT  LoadStateOverMountedMedia (StateReader & reader);

    //  Enough of a state to draw its screen, into a machine built from the
    //  same configuration with no disks: the disks the state has, and any
    //  part this machine has no counterpart for, are passed over. The machine
    //  and ROM set must match. The machine is for drawing only afterward;
    //  running it is undefined.
    HRESULT  LoadStateForPicture       (StateReader & reader);

    //  The media a saved state's bays had, from its header alone, so a second
    //  machine can check whether the disks it mounted are the ones the state
    //  was saved with before it loads it over them.
    static HRESULT  ReadSavedMedia  (const std::vector<Byte> & state, MediaIds & outIds);

    //  Debug builds only: asserts that every run a sharing save stored by
    //  reference matches the bytes of its buffer. A mismatch is a RAM write
    //  that missed the page marking or a track change that missed the
    //  generation count. Call it right after the save.
    static void     CheckSharedSave (const StateWriter & writer);

private:
    void            WriteStateHeader  (StateWriter & writer, size_t partCount) const;
    HRESULT         LoadStateSeating  (StateReader & reader, bool isOverMounted);
    HRESULT         CheckStateHeader  (StateReader & reader, uint16_t version, bool isOverMounted, uint32_t & outParts);
    HRESULT         ReadPictureHeader (StateReader & reader, uint16_t version, uint32_t & outParts) const;

    //  The saved bays' media ids, four per slot from version 4 and two before.
    static void     ReadMediaIds      (StateReader & reader, uint16_t version, MediaIds & outIds);
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
