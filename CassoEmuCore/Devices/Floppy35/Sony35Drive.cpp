#include "Pch.h"

#include "Devices/Disk/CommitPlan.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/DurableCommit.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Floppy35/Gcr35Track.h"
#include "Devices/Floppy35/Sony35Drive.h"
#include "Machines/Apple2/Common/WozLoader.h"




//  Register indices: CA1, CA0, SEL, CA2 from bit 3 down.
static constexpr int  s_kRegDirection  = 0x0;
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

        hr = InsertWoz (fileIo, path, bytes, hostProtect, outError);
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
        m_wozIo = nullptr;
        m_wozPath.clear();
    }
    else
    {
        hr = m_media.Detach();
        CHR (hr);
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
    bool  strobe = lstrb && !m_lstrb;



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

Byte Sony35Drive::ReadCell()
{
    Byte  bit = 0;



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

void Sony35Drive::WriteCell (Byte bit)
{
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
                Emit (Floppy35Op::MotorOn);
            }

            break;

        case s_kCmdMotorOff:
            if (m_motorOn)
            {
                HRESULT  hr = S_OK;



                m_motorOn = false;
                Emit (Floppy35Op::MotorOff);

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

HRESULT Sony35Drive::InsertWoz (IDiskFileIo & fileIo, const std::string & path, const std::vector<Byte> & bytes, const WriteProtectInfo & hostProtect, std::wstring & outError)
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
    m_wozIo        = &fileIo;
    m_wozPath      = path;
    m_woz          = std::move (image);

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

    hr = DurableCommit::Commit (*m_wozIo, m_wozPath, bytes, 0, CommitMode::Replace, progress);
    CHR (hr);

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
