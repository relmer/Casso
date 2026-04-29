#include "Pch.h"

#include "DiskIIController.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DOS 3.3 sector interleave table (logical → physical)
//
////////////////////////////////////////////////////////////////////////////////

static const int kDos33Interleave[16] =
{
    0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15
};





////////////////////////////////////////////////////////////////////////////////
//
//  6-and-2 write translate table (64 entries)
//
////////////////////////////////////////////////////////////////////////////////

static const Byte kWriteTranslate[64] =
{
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
};





////////////////////////////////////////////////////////////////////////////////
//
//  4-and-4 encoding helpers
//
////////////////////////////////////////////////////////////////////////////////

static Byte Encode44Odd (Byte val)
{
    return static_cast<Byte> (((val >> 1) | 0xAA));
}


static Byte Encode44Even (Byte val)
{
    return static_cast<Byte> ((val | 0xAA));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImage
//
////////////////////////////////////////////////////////////////////////////////

DiskImage::DiskImage ()
    : m_loaded        (false),
      m_dirty         (false),
      m_writeProtected (false)
{
    m_data.fill (0);
}


HRESULT DiskImage::Load (const std::string & filePath)
{
    HRESULT hr = S_OK;

    std::ifstream file (filePath, std::ios::binary);
    CBREx (file.good (), E_FAIL);

    file.read (reinterpret_cast<char *> (m_data.data ()), 143360);
    CBREx (file.gcount () == 143360, E_FAIL);

    m_filePath = filePath;
    m_loaded   = true;
    m_dirty    = false;

Error:
    return hr;
}


void DiskImage::Eject ()
{
    if (m_dirty && !m_writeProtected)
    {
        Flush ();
    }

    m_loaded = false;
    m_data.fill (0);
    m_filePath.clear ();
    m_dirty = false;
}


void DiskImage::ReadSector (int track, int logicalSector, Byte * outData) const
{
    int physicalSector = kDos33Interleave[logicalSector];
    size_t offset = static_cast<size_t> (track * 16 + physicalSector) * 256;

    memcpy (outData, &m_data[offset], 256);
}


void DiskImage::WriteSector (int track, int logicalSector, const Byte * data)
{
    if (m_writeProtected)
    {
        return;
    }

    int physicalSector = kDos33Interleave[logicalSector];
    size_t offset = static_cast<size_t> (track * 16 + physicalSector) * 256;

    memcpy (&m_data[offset], data, 256);
    m_dirty = true;
}


HRESULT DiskImage::Flush ()
{
    HRESULT hr = S_OK;

    if (!m_dirty || m_filePath.empty ())
    {
        return S_OK;
    }

    std::ofstream file (m_filePath, std::ios::binary);
    CBREx (file.good (), E_FAIL);

    file.write (reinterpret_cast<const char *> (m_data.data ()), 143360);
    m_dirty = false;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIIController
//
////////////////////////////////////////////////////////////////////////////////

DiskIIController::DiskIIController (int slot)
    : m_slot          (slot),
      m_ioStart       (static_cast<Word> (0xC080 + slot * 16)),
      m_ioEnd         (static_cast<Word> (0xC08F + slot * 16)),
      m_phases        (0),
      m_quarterTrack  (0),
      m_motorOn       (false),
      m_activeDrive   (0),
      m_q6            (false),
      m_q7            (false),
      m_shiftRegister (0),
      m_nibblePos     (0)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
////////////////////////////////////////////////////////////////////////////////

Byte DiskIIController::Read (Word address)
{
    int offset = (address - m_ioStart) & 0x0F;

    switch (offset)
    {
        case 0x0: HandlePhase (0, false); break;
        case 0x1: HandlePhase (0, true);  break;
        case 0x2: HandlePhase (1, false); break;
        case 0x3: HandlePhase (1, true);  break;
        case 0x4: HandlePhase (2, false); break;
        case 0x5: HandlePhase (2, true);  break;
        case 0x6: HandlePhase (3, false); break;
        case 0x7: HandlePhase (3, true);  break;
        case 0x8: m_motorOn = false; break;
        case 0x9: m_motorOn = true;  break;
        case 0xA: m_activeDrive = 0; break;
        case 0xB: m_activeDrive = 1; break;
        case 0xC: m_q6 = false; break;
        case 0xD: m_q6 = true;  break;
        case 0xE: m_q7 = false; break;
        case 0xF: m_q7 = true;  break;
    }

    // Q6=0, Q7=0: Read mode — return next nibble
    if (!m_q6 && !m_q7)
    {
        return ReadDataLatch ();
    }

    // Q6=1, Q7=0: Sense write protect
    if (m_q6 && !m_q7)
    {
        DiskImage * disk = GetDisk (m_activeDrive);

        if (disk && disk->IsWriteProtected ())
        {
            return 0x80;
        }

        return 0x00;
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIController::Write (Word address, Byte value)
{
    // Process the switch
    Read (address);

    // Q6=1, Q7=1: Load data latch
    if (m_q6 && m_q7)
    {
        m_shiftRegister = value;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandlePhase
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIController::HandlePhase (int phase, bool on)
{
    if (on)
    {
        m_phases |= (1 << phase);
    }
    else
    {
        m_phases &= ~(1 << phase);
    }

    // Determine direction based on current phase and energized phases
    int currentPhase = (m_quarterTrack / 2) & 3;
    int targetPhase  = -1;

    for (int i = 0; i < 4; i++)
    {
        if (m_phases & (1 << i))
        {
            targetPhase = i;
        }
    }

    if (targetPhase < 0)
    {
        return;
    }

    int delta = targetPhase - currentPhase;

    if (delta == 1 || delta == -3)
    {
        m_quarterTrack++;
    }
    else if (delta == -1 || delta == 3)
    {
        m_quarterTrack--;
    }

    // Clamp to valid range
    if (m_quarterTrack < 0)
    {
        m_quarterTrack = 0;
    }

    if (m_quarterTrack > 139)
    {
        m_quarterTrack = 139;
    }

    // Re-nibblize for new track
    NibblizeTrack (m_activeDrive);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadDataLatch
//
////////////////////////////////////////////////////////////////////////////////

Byte DiskIIController::ReadDataLatch ()
{
    if (m_nibbleBuffer.empty ())
    {
        NibblizeTrack (m_activeDrive);
    }

    if (m_nibbleBuffer.empty ())
    {
        return 0;
    }

    Byte value = m_nibbleBuffer[m_nibblePos];
    m_nibblePos = (m_nibblePos + 1) % m_nibbleBuffer.size ();

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblizeTrack
//
//  Convert raw sector data from .dsk into nibble stream (~6400 bytes/track).
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIController::NibblizeTrack (int drive)
{
    m_nibbleBuffer.clear ();
    m_nibblePos = 0;

    DiskImage * disk = GetDisk (drive);

    if (!disk || !disk->IsLoaded ())
    {
        return;
    }

    int track = m_quarterTrack / 4;
    Byte volume = 254;

    m_nibbleBuffer.reserve (6400);

    for (int sector = 0; sector < 16; sector++)
    {
        Byte sectorData[256];
        disk->ReadSector (track, sector, sectorData);

        // Gap 1
        for (int i = 0; i < 20; i++)
        {
            m_nibbleBuffer.push_back (0xFF);
        }

        // Address field
        m_nibbleBuffer.push_back (0xD5);
        m_nibbleBuffer.push_back (0xAA);
        m_nibbleBuffer.push_back (0x96);

        Byte checksum = static_cast<Byte> (volume ^ track ^ sector);
        m_nibbleBuffer.push_back (Encode44Odd (volume));
        m_nibbleBuffer.push_back (Encode44Even (volume));
        m_nibbleBuffer.push_back (Encode44Odd (static_cast<Byte> (track)));
        m_nibbleBuffer.push_back (Encode44Even (static_cast<Byte> (track)));
        m_nibbleBuffer.push_back (Encode44Odd (static_cast<Byte> (sector)));
        m_nibbleBuffer.push_back (Encode44Even (static_cast<Byte> (sector)));
        m_nibbleBuffer.push_back (Encode44Odd (checksum));
        m_nibbleBuffer.push_back (Encode44Even (checksum));

        m_nibbleBuffer.push_back (0xDE);
        m_nibbleBuffer.push_back (0xAA);
        m_nibbleBuffer.push_back (0xEB);

        // Gap 2
        for (int i = 0; i < 6; i++)
        {
            m_nibbleBuffer.push_back (0xFF);
        }

        // Data field
        m_nibbleBuffer.push_back (0xD5);
        m_nibbleBuffer.push_back (0xAA);
        m_nibbleBuffer.push_back (0xAD);

        // 6-and-2 encoding
        Byte buffer[342];

        // Secondary buffer (86 bytes from low 2 bits of each byte)
        for (int i = 0; i < 86; i++)
        {
            Byte val = 0;

            if (i < 86)
            {
                val = static_cast<Byte> (
                    ((sectorData[i] & 0x01) << 1) |
                    ((sectorData[i] & 0x02) >> 1));
            }

            if (i + 86 < 256)
            {
                val |= static_cast<Byte> (
                    ((sectorData[i + 86] & 0x01) << 3) |
                    ((sectorData[i + 86] & 0x02) << 1));
            }

            if (i + 172 < 256)
            {
                val |= static_cast<Byte> (
                    ((sectorData[i + 172] & 0x01) << 5) |
                    ((sectorData[i + 172] & 0x02) << 3));
            }

            buffer[i] = val;
        }

        // Primary buffer (256 bytes, shifted right 2)
        for (int i = 0; i < 256; i++)
        {
            buffer[86 + i] = sectorData[i] >> 2;
        }

        // XOR encode
        Byte prev = 0;

        for (int i = 0; i < 342; i++)
        {
            Byte encoded = buffer[i] ^ prev;
            m_nibbleBuffer.push_back (kWriteTranslate[encoded & 0x3F]);
            prev = buffer[i];
        }

        // Checksum
        m_nibbleBuffer.push_back (kWriteTranslate[prev & 0x3F]);

        m_nibbleBuffer.push_back (0xDE);
        m_nibbleBuffer.push_back (0xAA);
        m_nibbleBuffer.push_back (0xEB);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountDisk
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskIIController::MountDisk (int drive, const std::string & path)
{
    if (drive < 0 || drive > 1)
    {
        return E_INVALIDARG;
    }

    return m_disks[drive].Load (path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EjectDisk
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIController::EjectDisk (int drive)
{
    if (drive >= 0 && drive <= 1)
    {
        m_disks[drive].Eject ();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDisk
//
////////////////////////////////////////////////////////////////////////////////

DiskImage * DiskIIController::GetDisk (int drive)
{
    if (drive >= 0 && drive <= 1)
    {
        return &m_disks[drive];
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIController::Reset ()
{
    m_phases        = 0;
    m_quarterTrack  = 0;
    m_motorOn       = false;
    m_activeDrive   = 0;
    m_q6            = false;
    m_q7            = false;
    m_shiftRegister = 0;
    m_nibbleBuffer.clear ();
    m_nibblePos     = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<MemoryDevice> DiskIIController::Create (const DeviceConfig & config, MemoryBus & bus)
{
    UNREFERENCED_PARAMETER (bus);

    int slot = config.hasSlot ? config.slot : 6;
    return std::make_unique<DiskIIController> (slot);
}
