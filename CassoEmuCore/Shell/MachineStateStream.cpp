#include "Pch.h"

#include "Core/IMachineState.h"
#include "Core/MachineConfig.h"
#include "Core/Prng.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Devices/BlockDevice/IReplayMedia.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Shell/MachineHardwareIdentity.h"
#include "Shell/MachineStateStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateStream::MachineStateStream
//
////////////////////////////////////////////////////////////////////////////////

MachineStateStream::MachineStateStream (IMachineStateParts & parts, DiskImageStore & disks, const MachineConfig & config, const std::wstring & name) :
    m_parts  (parts),
    m_disks  (disks),
    m_config (config),
    m_name   (name)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateStream::SaveState
//
//  One section holding the header, then every stateful part in the order
//  GetStateParts gives.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineStateStream::SaveState (StateWriter & writer) const
{
    HRESULT  hr = S_OK;



    // The parts are mutable, for LoadState; only SaveState, which is const
    // on every part, is called through them here.
    m_parts.GetStateParts (m_stateParts);

    writer.BeginSection (kStateTag, kStateVersion);

    WriteStateHeader (writer, m_stateParts.size());

    for (const IMachineState * part : m_stateParts)
    {
        hr = part->SaveState (writer);
        CHR (hr);
    }

    hr = writer.EndSection();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateStream::LoadState
//
//  The header is checked before any part is touched, so a state from another
//  machine fails without disturbing this one. A failure after that leaves the
//  machine partly loaded.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineStateStream::LoadState (StateReader & reader)
{
    return LoadStateSeating (reader, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateStream::LoadStateOverMountedMedia
//
//  A state file's disks are mounted from the file before the load, so they
//  are new media to the disk store, which has never seen the ones the state
//  recorded. The bays are checked against the state instead, as occupied or
//  empty, and the Disk II is pointed at what they hold.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineStateStream::LoadStateOverMountedMedia (StateReader & reader)
{
    return LoadStateSeating (reader, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateStream::LoadStateSeating
//
//  The load behind both entry points; isOverMounted picks how the saved
//  disks are found.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineStateStream::LoadStateSeating (StateReader & reader, bool isOverMounted)
{
    HRESULT   hr         = S_OK;
    uint16_t  version    = 0;
    uint32_t  savedParts = 0;
    size_t    partCount  = 0;
    int       slot       = 0;
    int       drive      = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    CBREx (version >= kOldestStateVersion, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = CheckStateHeader (reader, isOverMounted, savedParts);
    CHR (hr);

    // After the header, which put the saved disks back in their bays.
    m_parts.GetStateParts (m_stateParts);
    partCount = m_stateParts.size();

    CBREx (savedParts == partCount, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    for (IMachineState * part : m_stateParts)
    {
        hr = part->LoadState (reader);
        CHR (hr);
    }

    hr = reader.EndSection();
    CHR (hr);

    //  Each mounted disk's tracks may have changed under it, so a viewer
    //  analyzes it again.
    for (slot = 0; slot < DiskImageStore::kSlotCount; slot++)
    {
        for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
        {
            m_disks.NoteStateRestored (slot, drive);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateStream::LoadStateForPicture
//
//  Each saved part is matched by its section tag against this machine's
//  parts, in order: a match loads, anything else is skipped whole. The
//  saved disks never match, since this machine holds none, so no disk is
//  seated and no image is touched; a matched part whose state has a disk's
//  is skipped too.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineStateStream::LoadStateForPicture (StateReader & reader)
{
    HRESULT                 hr         = S_OK;
    uint16_t                version    = 0;
    uint32_t                savedParts = 0;
    uint32_t                i          = 0;
    uint32_t                tag        = 0;
    size_t                  next       = 0;
    bool                    isMedia    = false;
    std::vector<uint32_t>   tags;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    CBREx (version >= kOldestStateVersion, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = ReadPictureHeader (reader, savedParts);
    CHR (hr);

    m_parts.GetStateParts (m_stateParts);
    tags.resize (m_stateParts.size());

    for (next = 0; next < tags.size(); next++)
    {
        hr = GetPartTag (*m_stateParts[next], tags[next]);
        CHR (hr);
    }

    next = 0;

    for (i = 0; i < savedParts; i++)
    {
        hr = reader.PeekSectionTag (tag);
        CHR (hr);

        if (next < tags.size() && tags[next] == tag)
        {
            // A card or port with disks: the picture's machine doesn't have them.
            isMedia = dynamic_cast<IReplayMediaSource *> (m_stateParts[next]) != nullptr;
            hr      = isMedia ? reader.SkipSection() : m_stateParts[next]->LoadState (reader);
            CHR (hr);

            next++;
            continue;
        }

        hr = reader.SkipSection();
        CHR (hr);
    }

    hr = reader.EndSection();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateStream::ReadSavedMedia
//
//  The header as WriteStateHeader lays it out, read as far as the bays: the
//  machine's name, its ROM identity, then the medium in each bay.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineStateStream::ReadSavedMedia (
    const std::vector<Byte>  & state,
    MediaIds                 & outIds)
{
    constexpr uint32_t  kLongestName = 256;
    HRESULT             hr           = S_OK;
    StateReader         reader       (state);
    uint16_t            version      = 0;
    uint32_t            nameLength   = 0;
    Word                ch           = 0;
    uint64_t            romIdentity  = 0;
    uint64_t            hardware     = 0;
    uint32_t            i            = 0;



    outIds.fill (0);

    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    CBREx (version >= kOldestStateVersion, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    reader.ReadUInt32 (nameLength);
    CBREx (nameLength <= kLongestName, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    for (i = 0; i < nameLength; i++)
    {
        reader.ReadWord (ch);
    }

    reader.ReadUInt64 (romIdentity);
    reader.ReadUInt64 (hardware);

    for (uint64_t & mediaId : outIds)
    {
        reader.ReadUInt64 (mediaId);
    }

    hr = reader.GetResult();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateStream::ReadPictureHeader
//
//  The header as CheckStateHeader reads it, holding the state to the same
//  machine and ROM set, with the drive bays and the Prng left alone. The
//  hardware may differ from the machine's: the picture is taken from the
//  parts whose tags match.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineStateStream::ReadPictureHeader (
    StateReader  & reader,
    uint32_t     & outParts) const
{
    HRESULT       hr          = S_OK;
    uint32_t      nameLength  = 0;
    std::wstring  name;
    Word          ch          = 0;
    uint64_t      romIdentity = 0;
    uint64_t      hardware    = 0;
    uint64_t      mediaId     = 0;
    bool          hasPrng     = false;
    uint64_t      prngState   = 0;
    uint32_t      i           = 0;
    size_t        bayCount    = (size_t) DiskImageStore::kSlotCount * DiskImageStore::kDriveCount;
    size_t        ownLength   = m_name.size();
    uint64_t      ownRom      = m_parts.GetRomIdentity();



    outParts = 0;

    reader.ReadUInt32 (nameLength);
    CBREx (nameLength == ownLength, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    for (i = 0; i < nameLength; i++)
    {
        reader.ReadWord (ch);
        name.push_back (static_cast<wchar_t> (ch));
    }

    reader.ReadUInt64 (romIdentity);
    reader.ReadUInt64 (hardware);

    for (i = 0; i < bayCount; i++)
    {
        reader.ReadUInt64 (mediaId);
    }

    reader.ReadUInt32 (outParts);
    reader.ReadBool   (hasPrng);
    reader.ReadUInt64 (prngState);

    hr = reader.GetResult();
    CHR (hr);

    CBREx (name        == m_name, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (romIdentity == ownRom,               HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateStream::GetPartTag
//
//  A part's section tag: the first four bytes of what it saves.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineStateStream::GetPartTag (
    const IMachineState  & part,
    uint32_t             & outTag)
{
    HRESULT                    hr      = S_OK;
    StateWriter                writer;
    const std::vector<Byte>  & bytes   = writer.GetBytes();
    bool                       hasTag  = false;
    size_t                     i       = 0;



    outTag = 0;

    hr = part.SaveState (writer);
    CHR (hr);

    hasTag = bytes.size() >= sizeof (outTag);
    CBRA (hasTag);

    for (i = 0; i < sizeof (outTag); i++)
    {
        outTag |= static_cast<uint32_t> (bytes[i]) << (CHAR_BIT * i);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateStream::CheckSharedSave
//
//  A sharing save keeps each RAM chunk nobody wrote since the last one by
//  reference, trusting the written-page marks, and each disk track by
//  reference while its generation is unchanged; a write either one missed
//  would leave a stale run in this and every later save. In a debug build
//  every run the save kept is compared with the live buffer it stands for,
//  and a difference asserts. A release build does nothing.
//
//  Only the kept runs are compared, not the whole blob against a second
//  save. The keyboard, the game port and the Joyport save values the UI
//  thread writes, so a key or a joystick move landing between two saves
//  makes them differ although no write was missed.
//
////////////////////////////////////////////////////////////////////////////////

void MachineStateStream::CheckSharedSave (const StateWriter & writer)
{
#ifdef _DEBUG
    HRESULT  hr     = S_OK;
    size_t   size   = 0;
    bool     isSame = false;



    for (const StateSegment & segment : writer.GetSegments())
    {
        size   = segment.bytes->size();
        isSame = size == 0 || (segment.source != nullptr && memcmp (segment.bytes->data(), segment.source, size) == 0);
        CBRA (isSame);
    }

Error:
    return;
#else
    UNREFERENCED_PARAMETER (writer);
#endif
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateStream::WriteStateHeader
//
//  Which machine and ROM set, which disk each drive bay holds, how many parts
//  follow, and the power-on Prng.
//
////////////////////////////////////////////////////////////////////////////////

void MachineStateStream::WriteStateHeader (StateWriter & writer, size_t partCount) const
{
    int           slot  = 0;
    int           drive = 0;
    const Prng  * prng  = m_parts.GetStatePrng();



    writer.WriteUInt32 (static_cast<uint32_t> (m_name.size()));

    for (wchar_t ch : m_name)
    {
        writer.WriteWord (static_cast<Word> (ch));
    }

    writer.WriteUInt64 (m_parts.GetRomIdentity());
    writer.WriteUInt64 (MachineHardwareIdentity::Get (m_config));

    for (slot = 0; slot < DiskImageStore::kSlotCount; slot++)
    {
        for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
        {
            writer.WriteUInt64 (m_disks.GetMediaId (slot, drive));
        }
    }

    writer.WriteUInt32 (static_cast<uint32_t> (partCount));
    writer.WriteBool   (prng != nullptr);
    writer.WriteUInt64 ((prng != nullptr) ? prng->GetState() : 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateStream::CheckStateHeader
//
//  Fails with ERROR_INVALID_DATA unless the state was saved by a machine of
//  the same name and ROM set, and every disk it held is mounted or kept by
//  the disk store. Only then are the saved disks put back in their bays and
//  the Prng restored; the part count is the caller's to check, since it
//  depends on the disks.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineStateStream::CheckStateHeader (
    StateReader  & reader,
    bool           isOverMounted,
    uint32_t     & outParts)
{
    HRESULT       hr             = S_OK;
    bool          isSameHardware = false;
    uint32_t      nameLength     = 0;
    size_t        expectedLength = 0;
    std::wstring  name;
    Word          ch             = 0;
    uint64_t      romIdentity    = 0;
    uint64_t      expectedRom    = 0;
    uint64_t      hardware       = 0;
    MediaIds      mediaIds       = {};
    bool          canSeat        = false;
    uint32_t      savedParts     = 0;
    bool          hasPrng        = false;
    uint64_t      prngState      = 0;
    uint32_t      i              = 0;
    Prng        * prng           = m_parts.GetStatePrng();



    reader.ReadUInt32 (nameLength);

    expectedLength = m_name.size();
    CBREx (nameLength == expectedLength, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    for (i = 0; i < nameLength; i++)
    {
        reader.ReadWord (ch);
        name.push_back (static_cast<wchar_t> (ch));
    }

    reader.ReadUInt64 (romIdentity);
    reader.ReadUInt64 (hardware);

    for (uint64_t & mediaId : mediaIds)
    {
        reader.ReadUInt64 (mediaId);
    }

    reader.ReadUInt32 (savedParts);
    reader.ReadBool   (hasPrng);
    reader.ReadUInt64 (prngState);

    hr = reader.GetResult();
    CHR (hr);

    expectedRom = m_parts.GetRomIdentity();

    CBREx (name        == m_name, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (romIdentity == expectedRom,          HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    //  A state taken on other hardware doesn't load: its parts would be laid
    //  onto cards and drives it wasn't taken from.
    isSameHardware = hardware == MachineHardwareIdentity::Get (m_config);
    CBREx (isSameHardware, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    canSeat = isOverMounted ? AreBaysAsSaved (mediaIds) : CanSeatMedia (mediaIds);
    CBREx (canSeat, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    if (isOverMounted)
    {
        m_parts.BindDiskDrives();
    }
    else
    {
        hr = SeatMedia (mediaIds);
        CHR (hr);
    }

    if (hasPrng && prng != nullptr)
    {
        prng->SetState (prngState);
    }

    outParts = savedParts;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateStream::CanSeatMedia
//
//  Whether every saved disk is mounted or kept, so the bays can be put back
//  without failing part way.
//
////////////////////////////////////////////////////////////////////////////////

bool MachineStateStream::CanSeatMedia (const MediaIds & mediaIds) const
{
    bool  canSeat = true;
    int   slot    = 0;
    int   drive   = 0;



    for (slot = 0; slot < DiskImageStore::kSlotCount; slot++)
    {
        for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
        {
            canSeat = canSeat && m_disks.CanSeatMedia (slot, drive, mediaIds[slot * DiskImageStore::kDriveCount + drive]);
        }
    }

    return canSeat;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateStream::AreBaysAsSaved
//
//  Whether every bay that held a disk at the save holds one now, and every
//  bay that was empty is empty.
//
////////////////////////////////////////////////////////////////////////////////

bool MachineStateStream::AreBaysAsSaved (const MediaIds & mediaIds) const
{
    bool  isSame    = true;
    bool  wasLoaded = false;
    int   slot      = 0;
    int   drive     = 0;



    for (slot = 0; slot < DiskImageStore::kSlotCount; slot++)
    {
        for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
        {
            wasLoaded = mediaIds[slot * DiskImageStore::kDriveCount + drive] != 0;
            isSame    = isSame && wasLoaded == m_disks.IsMounted (slot, drive);
        }
    }

    return isSame;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateStream::SeatMedia
//
//  Puts each saved disk back in its bay, and points the Disk II at whatever
//  its bays now hold, whether or not a bay changed: something since the save
//  may have pointed a drive elsewhere. Nothing is flushed: a disk that leaves
//  a bay here is kept by the store with its writes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineStateStream::SeatMedia (const MediaIds & mediaIds)
{
    HRESULT  hr        = S_OK;
    bool     isChanged = false;
    int      slot      = 0;
    int      drive     = 0;



    for (slot = 0; slot < DiskImageStore::kSlotCount; slot++)
    {
        for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
        {
            hr = m_disks.SeatMedia (slot, drive, mediaIds[slot * DiskImageStore::kDriveCount + drive], isChanged);
            CHR (hr);
        }
    }

    m_parts.BindDiskDrives();

Error:
    return hr;
}
