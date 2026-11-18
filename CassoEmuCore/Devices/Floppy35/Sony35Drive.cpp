#include "Pch.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Floppy35/Gcr35Track.h"
#include "Devices/Floppy35/Sony35BlockDisk.h"
#include "Devices/Floppy35/Sony35Drive.h"
#include "Devices/Floppy35/Sony35WozDisk.h"




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
//  A file that starts "WOZ" is a WOZ image, and any other a block image.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Sony35Drive::Insert (IDiskFileIo & fileIo, const std::string & path, const WriteProtectInfo & hostProtect, std::wstring & outError)
{
    constexpr size_t                  kSignatureBytes = 4;
    HRESULT                           hr              = S_OK;
    HRESULT                           probe           = S_OK;
    bool                              isWoz           = false;
    std::vector<Byte>                 head            (kSignatureBytes, 0);
    std::vector<Byte>                 bytes;
    std::unique_ptr<Sony35WozDisk>    woz;
    std::unique_ptr<Sony35BlockDisk>  blocks;



    hr = Eject();
    CHR (hr);

    probe = fileIo.ReadAt (path, 0, head);
    isWoz = SUCCEEDED (probe) && head[0] == 'W' && head[1] == 'O' && head[2] == 'Z';

    if (isWoz)
    {
        hr = fileIo.ReadAllBytes (path, bytes);
        CHR (hr);

        woz = std::make_unique<Sony35WozDisk>();
        hr  = woz->Load (&fileIo, path, bytes, hostProtect, outError);
        CHR (hr);

        m_disk = std::move (woz);
    }
    else
    {
        blocks = std::make_unique<Sony35BlockDisk>();
        hr     = blocks->Attach (fileIo, path, hostProtect, outError);
        CHR (hr);

        m_disk = std::move (blocks);
    }

    m_sides = m_disk->GetSides();

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

    hr = m_disk->Detach();
    CHR (hr);

    m_disk.reset();

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
    return (m_disk != nullptr) ? m_disk->Flush (m_tracks, m_dirty) : S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetImagePath
//
//  Empty with no disk.
//
////////////////////////////////////////////////////////////////////////////////

const std::string & Sony35Drive::GetImagePath() const
{
    static const std::string  s_none;



    return (m_disk != nullptr) ? m_disk->GetPath() : s_none;
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
//  The current track and side's bit stream, read from the disk the first
//  time.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> & Sony35Drive::GetTrackBits()
{
    int                  key   = m_track * kSides + GetSide();
    auto                 found = m_tracks.find (key);
    std::vector<Byte>  & bits  = m_tracks[key];



    if (found == m_tracks.end())
    {
        m_disk->ReadTrack (m_track, GetSide(), bits);
    }

    return bits;
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

    //  With no disk, the state of an empty block image.
    writer.WriteBool (IsWoz());

    if (m_disk != nullptr)
    {
        hr = m_disk->SaveState (writer);
        CHR (hr);
    }
    else
    {
        BlockDeviceMedia().SaveState (writer);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Sony35Drive::LoadState
//
//  The tracks not written since the disk last flushed are encoded again
//  from the disk when the head is over them.
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
    bool                hasWoz         = false;
    bool                isSameKind     = false;
    std::vector<Byte>   bits;
    BlockDeviceMedia    empty;



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

    isSameKind = hasWoz == IsWoz();
    CBREx (isSameKind, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    if (m_disk != nullptr)
    {
        hr = m_disk->LoadState (reader);
        CHR (hr);
    }
    else
    {
        hr = empty.LoadState (reader);
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
//  An empty drive's copy has no content.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Sony35Drive::CopyForReplay (ReplayBlockMedia & outCopy)
{
    outCopy = {};

    return (m_disk != nullptr) ? m_disk->CopyForReplay (outCopy) : S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Sony35Drive::MountReplayCopy
//
//  The disk in the drive is ejected first. After a copy with no content,
//  the drive is empty.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Sony35Drive::MountReplayCopy (const ReplayBlockMedia & copy)
{
    HRESULT                           hr = S_OK;
    std::wstring                      error;
    std::unique_ptr<Sony35WozDisk>    woz;
    std::unique_ptr<Sony35BlockDisk>  blocks;



    hr = Eject();
    CHR (hr);

    BAIL_OUT_IF (copy.content == nullptr, S_OK);

    if (copy.isWoz)
    {
        woz = std::make_unique<Sony35WozDisk>();
        hr  = woz->Load (nullptr, copy.path, *copy.content, copy.protect, error);
        CHR (hr);

        m_disk = std::move (woz);
    }
    else
    {
        blocks = std::make_unique<Sony35BlockDisk>();
        hr     = blocks->MountReplayCopy (copy);
        CHR (hr);

        m_disk = std::move (blocks);
    }

    m_sides = m_disk->GetSides();

    m_tracks.clear();
    m_dirty.clear();

Error:
    return hr;
}





