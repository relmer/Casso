#include "Pch.h"

#include "Shell/MachineStateFile.h"

#include "Core/StateHash.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Shell/MachineHost.h"





//  The first eight bytes of every machine state file.
static constexpr Byte  s_kMagic[] =
{
    'C', 'A', 'S', 'S', 'O', 'S', 'T', 'A',
};





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateFile::Build
//
//  The machine as a file's bytes. Run on the thread that owns the machine.
//  Each mounted disk is written whole from memory, so writes the guest made
//  that are not yet saved to the image file are in the state, and no image
//  file is read or written.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineStateFile::Build (
    MachineHost        & machine,
    std::vector<Byte>  & outBytes,
    MachineStateError  & outError)
{
    HRESULT                   hr          = S_OK;
    DiskImageStore          & store       = machine.GetDiskStore();
    const std::wstring      & name        = machine.GetCurrentMachineName();
    StateWriter               state;
    StateWriter               payload;
    std::vector<SavedDisk>    disks;
    SavedDisk                 disk;
    DiskImage               * image       = nullptr;
    int                       slot        = 0;
    int                       drive       = 0;



    outBytes.clear();

    hr = machine.SaveState (state);
    CHRF (hr, outError = MakeError ("state not saved", "The machine's state could not be read."));

    for (slot = 0; slot < DiskImageStore::kSlotCount; slot++)
    {
        for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
        {
            image = store.IsMounted (slot, drive) ? store.GetImage (slot, drive) : nullptr;

            if (image == nullptr)
            {
                continue;
            }

            disk        = SavedDisk();
            disk.slot   = slot;
            disk.drive  = drive;
            disk.format = image->GetSourceFormat();
            disk.path   = store.GetSourcePath (slot, drive);

            hr = image->Serialize (disk.image);
            CHRF (hr, outError = MakeError ("disk not saved", "A mounted disk holds data its image format cannot store, so the state was not saved."));

            disks.push_back (std::move (disk));
        }
    }

    payload.BeginSection (kPayloadTag, kPayloadVersion);
    payload.WriteUInt32 (static_cast<uint32_t> (name.size()));

    for (wchar_t ch : name)
    {
        payload.WriteWord (static_cast<Word> (ch));
    }

    payload.WriteUInt64 (machine.GetRomIdentity());
    payload.WriteUInt32 (static_cast<uint32_t> (disks.size()));

    for (const SavedDisk & saved : disks)
    {
        payload.WriteByte   (static_cast<Byte> (saved.slot));
        payload.WriteByte   (static_cast<Byte> (saved.drive));
        payload.WriteByte   (static_cast<Byte> (saved.format));
        payload.WriteUInt32 (static_cast<uint32_t> (saved.path.size()));
        payload.WriteBytes  (reinterpret_cast<const Byte *> (saved.path.data()), saved.path.size());
        payload.WriteUInt32 (static_cast<uint32_t> (saved.image.size()));
        payload.WriteBytes  (saved.image.data(), saved.image.size());
    }

    payload.WriteUInt32 (static_cast<uint32_t> (state.GetBytes().size()));
    payload.WriteBytes  (state.GetBytes().data(), state.GetBytes().size());

    hr = payload.EndSection();
    CHRA (hr);

    WriteHeader (payload.GetBytes(), outBytes);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateFile::Parse
//
//  Reads a file's bytes without touching any machine. The header and the
//  payload hash are checked first, so a damaged or cut-short file fails here
//  with ERROR_INVALID_DATA, and a file a newer Casso wrote with
//  ERROR_REVISION_MISMATCH.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineStateFile::Parse (
    const std::vector<Byte>  & bytes,
    MachineStateContents     & outContents,
    MachineStateError        & outError)
{
    HRESULT  hr = S_OK;



    outContents = MachineStateContents();

    hr = CheckHeader (bytes, outError);
    CHR (hr);

    hr = ReadPayload (bytes.data() + kHeaderSize, bytes.size() - kHeaderSize, outContents);
    CHRF (hr, outError = MakeError ("damaged state file", "The file is not a complete Casso machine state."));

Error:
    if (FAILED (hr))
    {
        outContents = MachineStateContents();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateFile::Check
//
//  Whether the state can load on this machine, checked without changing
//  it: the machine kind and ROM identity that saved it, and every disk it
//  carries.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineStateFile::Check (
    const MachineHost           & machine,
    const MachineStateContents  & contents,
    MachineStateError           & outError)
{
    HRESULT  hr            = S_OK;
    bool     isSameMachine = contents.machineName == machine.GetCurrentMachineName();
    bool     isSameRoms    = contents.romIdentity == machine.GetRomIdentity();



    CBRFEx (isSameMachine, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA),
            outError = MakeError ("different machine", "The state was saved by another kind of machine. Open that machine before loading it."));

    CBRFEx (isSameRoms, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA),
            outError = MakeError ("different ROM images", "The state was saved by a machine with different ROM images. A state loads only on a machine with the ROM images it was saved with."));

    hr = CheckDisks (contents, outError);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateFile::Apply
//
//  Puts a parsed state into the machine, which must already be the machine
//  kind that saved it. Check runs first, so a refused state changes
//  nothing. Then the disks in the bays are flushed, as a machine switch
//  flushes them, each bay is given the state's disk or emptied, and the
//  machine's state is loaded over them. The state's disks are built from
//  the bytes it carries; no image file is read.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineStateFile::Apply (
    MachineHost                 & machine,
    const MachineStateContents  & contents,
    MachineStateError           & outError)
{
    HRESULT      hr     = S_OK;
    StateReader  reader (contents.machineState);



    hr = Check (machine, contents, outError);
    CHR (hr);

    // The disks leaving the bays keep the writes made to them, as on a
    // machine switch.
    hr = machine.GetDiskStore().FlushAll();
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = RestoreDisks (machine, contents);
    CHRF (hr, outError = MakeError ("disk not restored", "A disk the state holds could not be put back in its drive."));

    hr = machine.LoadStateOverMountedMedia (reader);
    CHRF (hr, outError = MakeError ("state not loaded", "The machine's state could not be read from the file."));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateFile::MakeError
//
////////////////////////////////////////////////////////////////////////////////

MachineStateError MachineStateFile::MakeError (
    const char  * label,
    const char  * detail)
{
    MachineStateError  error;



    error.label  = label;
    error.detail = detail;

    return error;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateFile::MakeDefaultFileName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring MachineStateFile::MakeDefaultFileName (
    const std::wstring  & machineName,
    const SYSTEMTIME    & time)
{
    return std::format (L"{} {:04}-{:02}-{:02} {:02}{:02}{:02}.{}",
                        machineName,
                        time.wYear,
                        time.wMonth,
                        time.wDay,
                        time.wHour,
                        time.wMinute,
                        time.wSecond,
                        kExtension);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateFile::WriteHeader
//
////////////////////////////////////////////////////////////////////////////////

void MachineStateFile::WriteHeader (
    const std::vector<Byte>  & payload,
    std::vector<Byte>        & outBytes)
{
    constexpr size_t  kWordBytes  = sizeof (uint16_t);
    constexpr size_t  kQuadBytes  = sizeof (uint64_t);



    outBytes.clear();
    outBytes.reserve (kHeaderSize + payload.size());
    outBytes.insert  (outBytes.end(), std::begin (s_kMagic), std::end (s_kMagic));

    AppendLittleEndian (kFormatVersion,                         kWordBytes, outBytes);
    AppendLittleEndian (0,                                      kWordBytes, outBytes);
    AppendLittleEndian (payload.size(),                         kQuadBytes, outBytes);
    AppendLittleEndian (StateHash::Hash (payload.data(), payload.size()), kQuadBytes, outBytes);

    outBytes.insert (outBytes.end(), payload.begin(), payload.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateFile::CheckHeader
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineStateFile::CheckHeader (
    const std::vector<Byte>  & bytes,
    MachineStateError        & outError)
{
    constexpr size_t  kVersionOffset = kMagicSize;
    constexpr size_t  kSizeOffset    = kMagicSize + 2 * sizeof (uint16_t);
    constexpr size_t  kHashOffset    = kSizeOffset + sizeof (uint64_t);
    HRESULT           hr             = S_OK;
    bool              isLongEnough   = bytes.size() >= kHeaderSize;
    bool              hasMagic       = false;
    uint64_t          version        = 0;
    bool              isKnownVersion = false;
    uint64_t          payloadSize    = 0;
    uint64_t          hash           = 0;
    bool              isWholeFile    = false;
    bool              isIntact       = false;



    CBRFEx (isLongEnough, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = MakeError ("not a machine state", "The file is too short to be a Casso machine state."));

    hasMagic = std::equal (std::begin (s_kMagic), std::end (s_kMagic), bytes.begin());
    CBRFEx (hasMagic, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = MakeError ("not a machine state", "The file is not a Casso machine state."));

    version        = ReadLittleEndian (bytes.data() + kVersionOffset, sizeof (uint16_t));
    isKnownVersion = version != 0 && version <= kFormatVersion;
    CBRFEx (isKnownVersion, HRESULT_FROM_WIN32 (ERROR_REVISION_MISMATCH), outError = MakeError ("newer state format", "The file was saved by a newer version of Casso."));

    payloadSize = ReadLittleEndian (bytes.data() + kSizeOffset, sizeof (uint64_t));
    hash        = ReadLittleEndian (bytes.data() + kHashOffset, sizeof (uint64_t));

    isWholeFile = payloadSize == bytes.size() - kHeaderSize;
    CBRFEx (isWholeFile, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = MakeError ("damaged state file", "The file is shorter or longer than the state it holds, so it was cut short or changed."));

    isIntact = hash == StateHash::Hash (bytes.data() + kHeaderSize, bytes.size() - kHeaderSize);
    CBRFEx (isIntact, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = MakeError ("damaged state file", "The file's contents changed after it was saved."));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateFile::ReadPayload
//
//  Every length is checked against what is left before anything is sized
//  from it, and the payload must be read to its last byte.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineStateFile::ReadPayload (
    const Byte            * data,
    size_t                  size,
    MachineStateContents  & outContents)
{
    HRESULT      hr         = S_OK;
    StateReader  reader (data, size);
    uint16_t     version    = 0;
    uint32_t     count      = 0;
    uint32_t     length     = 0;
    uint32_t     i          = 0;
    Word         ch         = 0;
    Byte         value      = 0;
    bool         fits       = false;
    bool         isAtEnd    = false;
    SavedDisk    disk;



    hr = reader.BeginSection (kPayloadTag, kPayloadVersion, version);
    CHR (hr);

    reader.ReadUInt32 (count);
    fits = count <= size;
    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    for (i = 0; i < count; i++)
    {
        reader.ReadWord (ch);
        outContents.machineName.push_back (static_cast<wchar_t> (ch));
    }

    reader.ReadUInt64 (outContents.romIdentity);
    reader.ReadUInt32 (count);

    fits = count <= DiskImageStore::kSlotCount * DiskImageStore::kDriveCount;
    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    for (i = 0; i < count; i++)
    {
        disk = SavedDisk();

        reader.ReadByte (value);
        disk.slot = value;
        reader.ReadByte (value);
        disk.drive = value;
        reader.ReadByte (value);
        disk.format = static_cast<DiskFormat> (value);

        fits = value <= static_cast<Byte> (DiskFormat::Nib);
        CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        reader.ReadUInt32 (length);
        fits = length <= size;
        CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        disk.path.resize (length);
        reader.ReadBytes (reinterpret_cast<Byte *> (disk.path.data()), length);

        reader.ReadUInt32 (length);
        fits = length <= size;
        CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        disk.image.resize (length);
        reader.ReadBytes (disk.image.data(), length);

        outContents.disks.push_back (std::move (disk));
    }

    reader.ReadUInt32 (length);
    fits = length <= size;
    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outContents.machineState.resize (length);
    reader.ReadBytes (outContents.machineState.data(), length);

    hr = reader.EndSection();
    CHR (hr);

    isAtEnd = reader.IsAtEnd();
    CBREx (isAtEnd, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateFile::CheckDisks
//
//  Each disk goes in a bay the store has, no bay twice, and its bytes load
//  as an image of its format, so restoring them cannot fail part way.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineStateFile::CheckDisks (
    const MachineStateContents  & contents,
    MachineStateError           & outError)
{
    constexpr size_t              kBayCount = DiskImageStore::kSlotCount * DiskImageStore::kDriveCount;
    HRESULT                       hr        = S_OK;
    std::array<bool, kBayCount>   isTaken   = {};
    bool                          isValid   = false;
    bool                          wasTaken  = false;
    size_t                        bay       = 0;
    std::unique_ptr<DiskImage>    scratch;



    for (const SavedDisk & disk : contents.disks)
    {
        isValid = disk.slot >= 0 && disk.slot < DiskImageStore::kSlotCount && disk.drive >= 0 && disk.drive < DiskImageStore::kDriveCount;
        CBRFEx (isValid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = MakeError ("damaged state file", "The state holds a disk for a drive Casso does not have."));

        bay = static_cast<size_t> (disk.slot * DiskImageStore::kDriveCount + disk.drive);
        wasTaken = isTaken[bay];
        CBRFEx (!wasTaken, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = MakeError ("damaged state file", "The state holds two disks for one drive."));

        isTaken[bay] = true;

        scratch = std::make_unique<DiskImage>();
        scratch->LoadFromBytes (disk.format, disk.image, disk.path);

        isValid = scratch->IsLoaded();
        CBRFEx (isValid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = MakeError ("damaged disk in state", "A disk the state holds is not a readable disk image."));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateFile::RestoreDisks
//
//  Each bay gets the state's disk, or is emptied when the state had none
//  there. The bays were flushed before, so an eject here writes nothing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineStateFile::RestoreDisks (
    MachineHost                 & machine,
    const MachineStateContents  & contents)
{
    HRESULT            hr     = S_OK;
    DiskImageStore   & store  = machine.GetDiskStore();
    const SavedDisk  * saved  = nullptr;
    int                slot   = 0;
    int                drive  = 0;



    for (slot = 0; slot < DiskImageStore::kSlotCount; slot++)
    {
        for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
        {
            saved = nullptr;

            for (const SavedDisk & disk : contents.disks)
            {
                if (disk.slot == slot && disk.drive == drive)
                {
                    saved = &disk;
                }
            }

            if (saved != nullptr)
            {
                hr = store.MountRestored (slot, drive, saved->path, saved->format, saved->image);
                CHR (hr);
            }
            else if (store.IsMounted (slot, drive))
            {
                store.Eject (slot, drive);
            }
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateFile::ReadLittleEndian
//
////////////////////////////////////////////////////////////////////////////////

uint64_t MachineStateFile::ReadLittleEndian (
    const Byte  * data,
    size_t        byteCount)
{
    uint64_t  value = 0;
    size_t    i     = 0;



    for (i = 0; i < byteCount; i++)
    {
        value |= static_cast<uint64_t> (data[i]) << (i * CHAR_BIT);
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateFile::AppendLittleEndian
//
////////////////////////////////////////////////////////////////////////////////

void MachineStateFile::AppendLittleEndian (
    uint64_t            value,
    size_t              byteCount,
    std::vector<Byte> & outBytes)
{
    size_t  i = 0;



    for (i = 0; i < byteCount; i++)
    {
        outBytes.push_back (static_cast<Byte> (value >> (i * CHAR_BIT)));
    }
}





