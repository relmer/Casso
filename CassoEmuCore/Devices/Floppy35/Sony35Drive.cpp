#include "Pch.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Devices/Disk/CommitPlan.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/DurableCommit.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Floppy35/Gcr35Track.h"
#include "Devices/Floppy35/Sony35Drive.h"
#include "Machines/Apple2/Common/WozLoader.h"




//  Register indices: CA1, CA0, SEL, CA2 from bit 3 down.
static constexpr int  s_kRegDirection  = 0x0;
static constexpr int  s_kRegIndex0     = 0x1;
static constexpr int  s_kRegIndex1     = 0x3;
static constexpr int  s_kRegSuperDrive = 0x5;
static constexpr int  s_kRegMfmMode    = 0x7;
static constexpr int  s_kRegSwitched   = 0xC;
static constexpr int  s_kRegHighDens   = 0xF;
static constexpr int  s_kRegDiskIn     = 0x2;
static constexpr int  s_kRegStepping   = 0x4;
static constexpr int  s_kRegProtected  = 0x6;
static constexpr int  s_kRegMotorOn    = 0x8;
static constexpr int  s_kRegSingleSide = 0x9;
static constexpr int  s_kRegTrack0     = 0xA;
static constexpr int  s_kRegReady      = 0xB;
static constexpr int  s_kRegInstalled  = 0xD;
static constexpr int  s_kRegTach       = 0xE;

//  Commands, by the same index.
static constexpr int  s_kCmdStepIn     = 0x0;
static constexpr int  s_kCmdStepOut    = 0x1;
static constexpr int  s_kCmdClearSwitch = 0x3;
static constexpr int  s_kCmdStep       = 0x4;
static constexpr int  s_kCmdMotorOn    = 0x8;
static constexpr int  s_kCmdMotorOff   = 0x9;
static constexpr int  s_kCmdEject      = 0xD;

//  Tachometer half-periods a revolution: 60 pulses.
static constexpr size_t  s_kTachEdges  = 120;





////////////////////////////////////////////////////////////////////////////////
//
//  ~Sony35Drive
//
////////////////////////////////////////////////////////////////////////////////

Sony35Drive::~Sony35Drive()
{
    HRESULT  hr = Flush();



    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Insert
//
//  A file that starts "WOZ" is a WOZ image. Otherwise an 800K image is
//  two-sided, a 400K image one-sided, and any other size fails.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Sony35Drive::Insert (IDiskFileIo & fileIo, const std::string & path, const WriteProtectInfo & hostProtect, std::wstring & outError)
{
    constexpr size_t  kSignatureBytes = 4;
    HRESULT           hr              = S_OK;
    HRESULT           probe           = S_OK;
    uint32_t          blocks          = 0;
    bool              isFit           = false;
    bool              isWoz           = false;
    std::vector<Byte>  head   (kSignatureBytes, 0);
    std::vector<Byte>  bytes;



    hr = Eject();
    CHR (hr);

    probe = fileIo.ReadAt (path, 0, head);
    isWoz = SUCCEEDED (probe) && head[0] == 'W' && head[1] == 'O' && head[2] == 'Z';

    if (isWoz)
    {
        hr = fileIo.ReadAllBytes (path, bytes);
        CHR (hr);

        hr = InsertWoz (&fileIo, path, bytes, hostProtect, outError);
        CHR (hr);
    }
    else
    {
        hr = m_media.Attach (fileIo, path, Gcr35Track::kBlocksTwoSides, hostProtect, outError);
        CHR (hr);

        blocks = m_media.GetBlockCount();
        isFit  = blocks == Gcr35Track::kBlocksTwoSides || blocks == Gcr35Track::kBlocksOneSide;

        if (!isFit)
        {
            hr = m_media.Detach();
            IGNORE_RETURN_VALUE (hr, S_OK);

            outError = std::format (L"A 3.5-inch disk is 800 or 1,600 blocks, and the image has {}.", blocks);
            CBREx (false, HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT));
        }

        m_sides = (blocks == Gcr35Track::kBlocksTwoSides) ? kSides : 1;
    }

    m_tracks.clear();
    m_dirty.clear();
    Emit (Floppy35Op::Insert);

    //  A motor-on command latched while the drive was empty starts the
    //  spindle now.
    if (m_motorOn)
    {
        m_spinLeftNs = kSpinUpNs;
        Emit (Floppy35Op::MotorOn);
    }

    m_diskSwitched = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Eject
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Sony35Drive::Eject()
{
    HRESULT  hr      = S_OK;
    bool     hasDisk = HasDisk();



    BAIL_OUT_IF (!hasDisk, S_OK);

    hr = Flush();
    CHR (hr);

    if (m_woz != nullptr)
    {
        m_woz.reset();
        m_wozBytes.reset();
        m_wozIo = nullptr;
        m_wozPath.clear();
    }
    else
    {
        hr = m_media.Detach();
        CHR (hr);
    }

    if (m_motorOn)
    {
        Emit (Floppy35Op::MotorOff);
    }

    m_tracks.clear();
    m_dirty.clear();
    m_motorOn = false;
    Emit (Floppy35Op::Eject);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Flush
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Sony35Drive::Flush()
{
    return (m_woz != nullptr) ? FlushWoz() : FlushBlocks();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlushBlocks
//
//  Each written track decoded back to its sectors, which go to the image.
//  A sector that no longer decodes is left as the image has it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Sony35Drive::FlushBlocks()
{
    HRESULT             hr = S_OK;
    std::vector<Byte>   data;
    std::vector<bool>   found;



    for (int key : m_dirty)
    {
        int        track = key / kSides;
        int        side  = key % kSides;
        uint32_t   first = Gcr35Track::GetFirstBlock (track, side, m_sides);



        Gcr35Track::DecodeTrack (track, side, m_tracks[key], data, found);

        for (size_t sector = 0; sector < found.size(); sector++)
        {
            if (!found[sector])
            {
                continue;
            }

            hr = m_media.WriteBlock (first + (uint32_t) sector, std::span<const Byte> (data).subspan (sector * Gcr35Track::kDataBytes, Gcr35Track::kDataBytes));
            CHR (hr);
        }
    }

    m_dirty.clear();

    hr = m_media.Flush();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetPhases
//
//  LSTRB rising runs the command the other lines select.
//
////////////////////////////////////////////////////////////////////////////////

void Sony35Drive::SetPhases (bool ca0, bool ca1, bool ca2, bool lstrb)
{
    bool  strobe = lstrb && !m_lstrb && m_enabled;



    m_ca0   = ca0;
    m_ca1   = ca1;
    m_ca2   = ca2;
    m_lstrb = lstrb;

    if (strobe)
    {
        RunCommand (GetRegister());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetEnabled
//
////////////////////////////////////////////////////////////////////////////////

void Sony35Drive::SetEnabled (bool isEnabled)
{
    if (isEnabled)
    {
        m_offLeftNs = 0;
    }
    else if (m_enabled && m_motorOn)
    {
        m_offLeftNs = kMotorOffNs;
    }

    m_enabled = isEnabled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSel
//
////////////////////////////////////////////////////////////////////////////////

void Sony35Drive::SetSel (bool sel)
{
    if (sel != m_sel)
    {
        m_sel = sel;
        Emit (Floppy35Op::Side);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSense
//
////////////////////////////////////////////////////////////////////////////////

bool Sony35Drive::GetSense() const
{
    bool  hasDisk  = HasDisk();
    bool  ejecting = m_ejectLeftNs > 0;
    int   reg      = GetRegister();



    switch (reg)
    {
        case s_kRegDirection:  return m_stepOut;
        case s_kRegDiskIn:     return !hasDisk;
        case s_kRegStepping:   return m_stepLeftNs == 0;
        case s_kRegProtected:  return !(hasDisk && IsWriteProtected());
        case s_kRegMotorOn:    return !m_motorOn;
        case s_kRegSingleSide: return true;
        case s_kRegTrack0:     return m_track != 0;
        case s_kRegReady:      return !(m_motorOn && hasDisk && !ejecting && m_spinLeftNs == 0);
        case s_kRegInstalled:  return false;

        // The SuperDrive's lines, low on an 800K drive: not a SuperDrive, not
        // in MFM mode, no index pulse, no 1.44 MB disk. The 3.5 controller
        // card's firmware reads them to find which kind of drive is attached.
        case s_kRegIndex0:
        case s_kRegIndex1:
        case s_kRegSuperDrive:
        case s_kRegMfmMode:
        case s_kRegHighDens:   return false;

        case s_kRegSwitched:   return m_diskSwitched;

        case s_kRegTach:
        {
            size_t  bits = Gcr35Track::GetTrackBits (m_track);



            return ((m_position * s_kTachEdges / bits) & 1) != 0;
        }

        default:
            return true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Advance
//
////////////////////////////////////////////////////////////////////////////////

void Sony35Drive::Advance (uint64_t elapsedNs)
{
    m_stepLeftNs = (m_stepLeftNs > elapsedNs) ? m_stepLeftNs - elapsedNs : 0;
    m_spinLeftNs = (m_spinLeftNs > elapsedNs) ? m_spinLeftNs - elapsedNs : 0;

    if (m_offLeftNs > 0)
    {
        m_offLeftNs = (m_offLeftNs > elapsedNs) ? m_offLeftNs - elapsedNs : 0;

        if (m_offLeftNs == 0)
        {
            RunCommand (s_kCmdMotorOff);
        }
    }

    if (m_ejectLeftNs > 0)
    {
        m_ejectLeftNs = (m_ejectLeftNs > elapsedNs) ? m_ejectLeftNs - elapsedNs : 0;

        if (m_ejectLeftNs == 0)
        {
            HRESULT  hr = Eject();



            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadCell
//
//  The bit under the head, then the disk turns one cell. A stopped motor or
//  an empty drive returns 0 for every cell.
//
////////////////////////////////////////////////////////////////////////////////

Byte Sony35Drive::ReadCell (uint64_t cellEndNs)
{
    Byte  bit = 0;



    (void) cellEndNs;

    if (!m_motorOn || !HasDisk() || GetSide() >= m_sides)
    {
        return 0;
    }

    {
        std::vector<Byte>  & bits = GetTrackBits();



        m_position %= bits.size();
        bit         = bits[m_position];
        m_position  = (m_position + 1) % bits.size();
    }

    return bit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteCell
//
////////////////////////////////////////////////////////////////////////////////

void Sony35Drive::WriteCell (Byte bit, uint64_t cellEndNs)
{
    (void) cellEndNs;

    if (!m_motorOn || !HasDisk() || IsWriteProtected() || GetSide() >= m_sides)
    {
        return;
    }

    {
        std::vector<Byte>  & bits = GetTrackBits();



        m_position %= bits.size();
        bits[m_position] = bit;
        m_position       = (m_position + 1) % bits.size();
        m_dirty.insert (m_track * kSides + GetSide());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRegister
//
////////////////////////////////////////////////////////////////////////////////

int Sony35Drive::GetRegister() const
{
    return (m_ca1 ? 8 : 0) | (m_ca0 ? 4 : 0) | (m_sel ? 2 : 0) | (m_ca2 ? 1 : 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunCommand
//
////////////////////////////////////////////////////////////////////////////////

void Sony35Drive::RunCommand (int command)
{
    constexpr int  kLastTrack = Gcr35Track::kTracks - 1;



    switch (command)
    {
        case s_kCmdStepIn:
            m_stepOut = false;
            break;

        case s_kCmdStepOut:
            m_stepOut = true;
            break;

        case s_kCmdStep:
            m_track      = std::clamp (m_track + (m_stepOut ? -1 : 1), 0, kLastTrack);
            m_stepLeftNs = kStepNs;
            Emit (Floppy35Op::Step);
            break;

        case s_kCmdMotorOn:
            if (!m_motorOn)
            {
                m_motorOn    = true;
                m_spinLeftNs = kSpinUpNs;

                if (HasDisk())
                {
                    Emit (Floppy35Op::MotorOn);
                }
            }

            break;

        case s_kCmdMotorOff:
            if (m_motorOn)
            {
                HRESULT  hr = S_OK;



                m_motorOn = false;

                if (HasDisk())
                {
                    Emit (Floppy35Op::MotorOff);
                }

                hr = Flush();
                IGNORE_RETURN_VALUE (hr, S_OK);
            }

            break;

        case s_kCmdEject:
            if (HasDisk())
            {
                m_ejectLeftNs = kEjectNs;
            }

            break;

        case s_kCmdClearSwitch:
            m_diskSwitched = false;
            break;

        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTrackBits
//
//  The current track and side's bit stream, built from the image's blocks
//  the first time.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> & Sony35Drive::GetTrackBits()
{
    int                  key   = m_track * kSides + GetSide();
    auto                 found = m_tracks.find (key);
    int                  count = Gcr35Track::GetSectorsPerTrack (m_track);
    uint32_t             first = Gcr35Track::GetFirstBlock (m_track, GetSide(), m_sides);
    std::vector<Byte>    data ((size_t) count * Gcr35Track::kDataBytes, 0);
    std::vector<Byte>  & bits  = m_tracks[key];



    if (found != m_tracks.end())
    {
        return bits;
    }

    if (m_woz != nullptr)
    {
        int     slot  = m_woz->GetMappedSlot (key);
        size_t  count = (slot >= 0) ? m_woz->GetTrackBitCount (slot) : 0;



        bits.assign ((count != 0) ? count : Gcr35Track::GetTrackBits (m_track), 0);

        for (size_t i = 0; i < count; i++)
        {
            bits[i] = m_woz->ReadBit (slot, i);
        }

        return bits;
    }

    for (int sector = 0; sector < count; sector++)
    {
        HRESULT  hr = m_media.ReadBlock (first + (uint32_t) sector, std::span<Byte> (data).subspan ((size_t) sector * Gcr35Track::kDataBytes, Gcr35Track::kDataBytes));



        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    Gcr35Track::BuildTrack (m_track, GetSide(), m_sides == kSides ? Gcr35Track::kFormatTwoSides : Gcr35Track::kFormatOneSide, data, bits);

    return bits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InsertWoz
//
//  A WOZ image of a 3.5-inch disk, write protected when INFO marks it or the
//  host file is read only.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Sony35Drive::InsertWoz (IDiskFileIo * fileIo, const std::string & path, const std::vector<Byte> & bytes, const WriteProtectInfo & hostProtect, std::wstring & outError)
{
    HRESULT                     hr       = S_OK;
    WozLoader::Description      about;
    std::unique_ptr<DiskImage>  image    = std::make_unique<DiskImage>();
    bool                        isLoaded = false;



    WozLoader::Describe (bytes, about);

    if (!about.isWoz || about.diskType != WozLoader::kDiskType35)
    {
        outError = L"The WOZ image isn't a 3.5-inch disk.";
        CBREx (false, HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT));
    }

    image->LoadFromBytes (DiskFormat::Woz, bytes, path);
    isLoaded = image->IsLoaded();

    if (!isLoaded)
    {
        outError = L"The WOZ image's tracks don't load.";
        CBREx (false, HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT));
    }

    m_sides        = (about.info.sides == 1) ? 1 : kSides;
    m_wozProtected = about.writeProtected || hostProtect.Any();
    m_wozIo        = fileIo;
    m_wozPath      = path;
    m_woz          = std::move (image);
    m_wozBytes     = std::make_shared<const std::vector<Byte>> (bytes);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlushWoz
//
//  Each written track's bits replace the WOZ track at TMAP entry track * 2 +
//  side, a new track where the entry is empty, and the image replaces its
//  file in one step: after an interruption the file is the old one or the
//  new one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Sony35Drive::FlushWoz()
{
    HRESULT               hr       = S_OK;
    bool                  hasDirty = !m_dirty.empty();
    std::vector<Byte>     bytes;
    CommitPlan::Progress  progress = {};



    BAIL_OUT_IF (!hasDirty, S_OK);

    for (int key : m_dirty)
    {
        const std::vector<Byte>  & bits = m_tracks[key];
        int                        slot = m_woz->GetMappedSlot (key);



        if (slot < 0)
        {
            slot = m_woz->GetTrackCount();
            m_woz->EnsureTrackSlots (slot + 1);
            m_woz->SetQuarterTrackSlot (key, slot);
        }

        m_woz->ResizeTrack (slot, bits.size());

        for (size_t i = 0; i < bits.size(); i++)
        {
            m_woz->WriteBit (slot, i, bits[i]);
        }
    }

    m_dirty.clear();

    hr = m_woz->Serialize (bytes);
    CHR (hr);

    //  A replay's copy writes no file.
    BAIL_OUT_IF (m_wozIo == nullptr, S_OK);

    hr = DurableCommit::Commit (*m_wozIo, m_wozPath, bytes, 0, CommitMode::Replace, progress);
    CHR (hr);

    m_wozBytes = std::make_shared<const std::vector<Byte>> (std::move (bytes));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Emit
//
////////////////////////////////////////////////////////////////////////////////

void Sony35Drive::Emit (Floppy35Op op)
{
    Floppy35Event  event;



    if (m_sink == nullptr)
    {
        return;
    }

    event.drive = (Byte) m_driveIndex;
    event.op    = op;
    event.track = (Byte) m_track;
    event.side  = (Byte) GetSide();

    m_sink->OnFloppy35Event (event);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Sony35Drive::SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Sony35Drive::SaveState (StateWriter & writer) const
{
    HRESULT  hr = S_OK;



    writer.WriteBool   (m_ca0);
    writer.WriteBool   (m_ca1);
    writer.WriteBool   (m_ca2);
    writer.WriteBool   (m_lstrb);
    writer.WriteBool   (m_sel);
    writer.WriteBool   (m_stepOut);
    writer.WriteBool   (m_motorOn);
    writer.WriteBool   (m_enabled);
    writer.WriteBool   (m_diskSwitched);
    writer.WriteUInt32 (static_cast<uint32_t> (m_track));
    writer.WriteUInt32 (static_cast<uint32_t> (m_sides));
    writer.WriteUInt64 (m_stepLeftNs);
    writer.WriteUInt64 (m_spinLeftNs);
    writer.WriteUInt64 (m_ejectLeftNs);
    writer.WriteUInt64 (m_offLeftNs);
    writer.WriteUInt64 (static_cast<uint64_t> (m_position));

    writer.WriteUInt32 (static_cast<uint32_t> (m_dirty.size()));

    for (int key : m_dirty)
    {
        const std::vector<Byte>  & bits = m_tracks.at (key);

        writer.WriteUInt32 (static_cast<uint32_t> (key));
        writer.WriteUInt32 (static_cast<uint32_t> (bits.size()));
        writer.WriteBytes  (bits.data(), bits.size());
    }

    writer.WriteBool (m_woz != nullptr);

    if (m_woz != nullptr)
    {
        writer.WriteUInt32 (static_cast<uint32_t> (m_wozPath.size()));
        writer.WriteBytes  (reinterpret_cast<const Byte *> (m_wozPath.data()), m_wozPath.size());

        hr = m_woz->SaveState (writer);
        CHR (hr);
    }
    else
    {
        m_media.SaveState (writer);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Sony35Drive::LoadState
//
//  The tracks not written since the disk last flushed are encoded again
//  from the disk as the head reaches them.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Sony35Drive::LoadState (StateReader & reader)
{
    constexpr uint32_t  kMostTrackBits = 0x40000;
    HRESULT             hr             = S_OK;
    uint32_t            track          = 0;
    uint32_t            sides          = 0;
    uint64_t            position       = 0;
    uint32_t            dirtyCount     = 0;
    uint32_t            key            = 0;
    uint32_t            bitCount       = 0;
    uint32_t            pathLength     = 0;
    bool                hasWoz         = false;
    std::string         wozPath;
    std::vector<Byte>   bits;



    reader.ReadBool   (m_ca0);
    reader.ReadBool   (m_ca1);
    reader.ReadBool   (m_ca2);
    reader.ReadBool   (m_lstrb);
    reader.ReadBool   (m_sel);
    reader.ReadBool   (m_stepOut);
    reader.ReadBool   (m_motorOn);
    reader.ReadBool   (m_enabled);
    reader.ReadBool   (m_diskSwitched);
    reader.ReadUInt32 (track);
    reader.ReadUInt32 (sides);
    reader.ReadUInt64 (m_stepLeftNs);
    reader.ReadUInt64 (m_spinLeftNs);
    reader.ReadUInt64 (m_ejectLeftNs);
    reader.ReadUInt64 (m_offLeftNs);
    reader.ReadUInt64 (position);
    reader.ReadUInt32 (dirtyCount);

    hr = reader.GetResult();
    CHR (hr);

    CBREx (track < (uint32_t) Gcr35Track::kTracks && sides == (uint32_t) m_sides, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    m_track    = static_cast<int> (track);
    m_position = static_cast<size_t> (position);

    m_tracks.clear();
    m_dirty.clear();

    for (uint32_t i = 0; i < dirtyCount; i++)
    {
        reader.ReadUInt32 (key);
        reader.ReadUInt32 (bitCount);
        CBREx (bitCount <= kMostTrackBits && key < (uint32_t) (Gcr35Track::kTracks * kSides), HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        bits.assign (bitCount, 0);
        reader.ReadBytes (bits.data(), bits.size());

        m_tracks[(int) key] = bits;
        m_dirty.insert ((int) key);
    }

    reader.ReadBool (hasWoz);
    CBREx (hasWoz == (m_woz != nullptr), HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    if (hasWoz)
    {
        reader.ReadUInt32 (pathLength);
        CBREx (pathLength <= MAX_PATH * 4, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        wozPath.resize (pathLength);
        reader.ReadBytes (reinterpret_cast<Byte *> (wozPath.data()), pathLength);
        CBREx (wozPath == m_wozPath, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        hr = m_woz->LoadState (reader);
        CHR (hr);
    }
    else
    {
        hr = m_media.LoadState (reader);
        CHR (hr);
    }

    hr = reader.GetResult();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Sony35Drive::CopyForReplay
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Sony35Drive::CopyForReplay (ReplayBlockMedia & outCopy)
{
    HRESULT  hr = S_OK;



    if (m_woz != nullptr)
    {
        outCopy                   = {};
        outCopy.path              = m_wozPath;
        outCopy.protect.imageFlag = m_wozProtected;
        outCopy.isWoz             = true;
        outCopy.content           = m_wozBytes;
    }
    else
    {
        hr = m_media.CopyForReplay (outCopy);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Sony35Drive::MountReplayCopy
//
//  The disk in the drive is ejected first. A copy with no content leaves the
//  drive empty.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Sony35Drive::MountReplayCopy (const ReplayBlockMedia & copy)
{
    HRESULT       hr = S_OK;
    std::wstring  error;



    hr = Eject();
    CHR (hr);

    BAIL_OUT_IF (copy.content == nullptr, S_OK);

    if (copy.isWoz)
    {
        hr = InsertWoz (nullptr, copy.path, *copy.content, copy.protect, error);
        CHR (hr);
    }
    else
    {
        hr = m_media.MountReplayCopy (copy);
        CHR (hr);

        m_sides = (m_media.GetBlockCount() == Gcr35Track::kBlocksTwoSides) ? kSides : 1;
    }

    m_tracks.clear();
    m_dirty.clear();

Error:
    return hr;
}
