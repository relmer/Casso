#include "Pch.h"

#include "CassoLink/Host/CassoLinkVirtualDrives.h"
#include "CassoLink/CassoLinkCodec.h"
#include "Devices/Disk/IDiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkVirtualDrives::CassoLinkVirtualDrives
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkVirtualDrives::CassoLinkVirtualDrives (IDiskFileIo & fileIo) :
    m_fileIo (fileIo)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkVirtualDrives::SetImage
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkVirtualDrives::SetImage (int drive, const std::string & path)
{
    std::lock_guard<std::mutex>  guard (m_lock);
    Drive                      * found = FindDrive (drive);



    if (found != nullptr)
    {
        *found      = Drive();
        found->path = path;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkVirtualDrives::GetImage
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkVirtualDrives::GetImage (int drive) const
{
    std::lock_guard<std::mutex>  guard (m_lock);
    bool                         valid = drive >= 1 && drive <= (int) kDrives;



    return valid ? m_drives[(size_t) drive - 1].path : std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkVirtualDrives::GetBlocks
//
////////////////////////////////////////////////////////////////////////////////

Byte CassoLinkVirtualDrives::GetBlocks (int drive, uint32_t & outBlocks)
{
    std::lock_guard<std::mutex>  guard (m_lock);
    HRESULT                      hr      = S_OK;
    Drive                      * found   = FindDrive (drive);
    bool                         present = found != nullptr && !found->path.empty();



    outBlocks = 0;
    CBREx (present, HRESULT_FROM_WIN32 (ERROR_NOT_READY));

    hr = OpenDrive (*found);
    CHR (hr);

    outBlocks = (uint32_t) std::min<uint64_t> (found->size / kBlockSize, 0xFFFF);

Error:
    return ToProDosError (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkVirtualDrives::ReadBlock
//
////////////////////////////////////////////////////////////////////////////////

Byte CassoLinkVirtualDrives::ReadBlock (int drive, uint32_t block, std::span<Byte> outData)
{
    std::lock_guard<std::mutex>  guard (m_lock);
    HRESULT                      hr      = S_OK;
    Drive                      * found   = FindDrive (drive);
    bool                         present = found != nullptr && !found->path.empty();
    bool                         sized   = outData.size() == kBlockSize;



    CBRAEx (sized, E_INVALIDARG);
    std::fill (outData.begin(), outData.end(), (Byte) 0);

    CBREx (present, HRESULT_FROM_WIN32 (ERROR_NOT_READY));

    hr = OpenDrive (*found);
    CHR (hr);

    CBREx ((uint64_t) (block + 1) * kBlockSize <= found->size, HRESULT_FROM_WIN32 (ERROR_NOT_READY));

    hr = ReadInPlace (*found, block, outData);
    CHR (hr);

Error:
    return ToProDosError (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkVirtualDrives::WriteBlock
//
//  The journal has the magic, the block number, the block and its CRC.
//
////////////////////////////////////////////////////////////////////////////////


Byte CassoLinkVirtualDrives::WriteBlock (int drive, uint32_t block, std::span<const Byte> data)
{
    std::lock_guard<std::mutex>  guard (m_lock);
    HRESULT                      hr      = S_OK;
    Drive                      * found   = FindDrive (drive);
    bool                         present = found != nullptr && !found->path.empty();
    bool                         sized   = data.size() == kBlockSize;
    std::vector<Byte>            journal (std::begin (s_kJournalMagic), std::end (s_kJournalMagic));
    Word                         crc     = CassoLinkCodec::ComputeCrc (data);
    std::string                  path;



    CBRAEx (sized, E_INVALIDARG);
    CBREx (present, HRESULT_FROM_WIN32 (ERROR_NOT_READY));

    hr = OpenDrive (*found);
    CHR (hr);

    CBREx ((uint64_t) (block + 1) * kBlockSize <= found->size, HRESULT_FROM_WIN32 (ERROR_NOT_READY));

    for (int shift = 0; shift < 32; shift += 8)
    {
        journal.push_back ((Byte) (block >> shift));
    }

    journal.insert (journal.end(), data.begin(), data.end());
    journal.push_back ((Byte) (crc & 0xFF));
    journal.push_back ((Byte) (crc >> 8));

    path = GetJournalPath (found->path);

    hr = m_fileIo.WriteAllBytes (path, journal);
    CHR (hr);

    hr = m_fileIo.FlushToStorage (path);
    CHR (hr);

    hr = WriteInPlace (*found, block, data);
    CHR (hr);

    hr = m_fileIo.FlushToStorage (found->path);
    CHR (hr);

    hr = m_fileIo.Remove (path);
    CHR (hr);

Error:
    return ToProDosError (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkVirtualDrives::GetJournalPath
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkVirtualDrives::GetJournalPath (const std::string & imagePath)
{
    return imagePath + kJournalSuffix;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkVirtualDrives::OpenDrive
//
//  The first use of a drive since its image was set applies a journal a
//  write left; each use measures the image again, as it can change on the
//  PC between requests.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkVirtualDrives::OpenDrive (Drive & drive)
{
    HRESULT    hr = S_OK;
    FileStamp  stamp;



    if (!drive.recovered)
    {
        hr = Recover (drive);
        CHR (hr);

        drive.recovered = true;
    }

    hr = m_fileIo.Stat (drive.path, stamp);
    CHR (hr);

    drive.size = stamp.sizeBytes;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkVirtualDrives::Recover
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkVirtualDrives::Recover (Drive & drive)
{
    HRESULT            hr      = S_OK;
    std::string        path    = GetJournalPath (drive.path);
    std::vector<Byte>  journal;
    bool               whole   = false;
    uint32_t           block   = 0;
    Word               crc     = 0;
    FileStamp          stamp;



    BAIL_OUT_IF (!m_fileIo.Exists (path), S_OK);

    hr = m_fileIo.ReadAllBytes (path, journal);
    CHR (hr);

    whole = journal.size() == kJournalSize && std::equal (std::begin (s_kJournalMagic), std::end (s_kJournalMagic), journal.begin());

    if (whole)
    {
        std::span<const Byte>  data = std::span<const Byte> (journal).subspan (8, kBlockSize);

        block = (uint32_t) journal[4] | ((uint32_t) journal[5] << 8) | ((uint32_t) journal[6] << 16) | ((uint32_t) journal[7] << 24);
        crc   = (Word) (journal[8 + kBlockSize] | (journal[9 + kBlockSize] << 8));
        whole = CassoLinkCodec::ComputeCrc (data) == crc;

        hr = m_fileIo.Stat (drive.path, stamp);
        CHR (hr);

        drive.size = stamp.sizeBytes;

        if (whole && (uint64_t) (block + 1) * kBlockSize <= drive.size)
        {
            hr = WriteInPlace (drive, block, data);
            CHR (hr);

            hr = m_fileIo.FlushToStorage (drive.path);
            CHR (hr);
        }
    }

    hr = m_fileIo.Remove (path);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkVirtualDrives::WriteInPlace
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkVirtualDrives::WriteInPlace (const Drive & drive, uint32_t block, std::span<const Byte> data)
{
    HRESULT  hr = S_OK;



    if (!IsDosOrder (drive))
    {
        hr = m_fileIo.WriteAt (drive.path, GetOffset (drive, block, 0), data);
        CHR (hr);
    }
    else
    {
        for (size_t half = 0; half < kBlockSize / kSectorSize; half++)
        {
            hr = m_fileIo.WriteAt (drive.path, GetOffset (drive, block, half), data.subspan (half * kSectorSize, kSectorSize));
            CHR (hr);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkVirtualDrives::ReadInPlace
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkVirtualDrives::ReadInPlace (const Drive & drive, uint32_t block, std::span<Byte> outData)
{
    HRESULT  hr = S_OK;



    if (!IsDosOrder (drive))
    {
        hr = m_fileIo.ReadAt (drive.path, GetOffset (drive, block, 0), outData);
        CHR (hr);
    }
    else
    {
        for (size_t half = 0; half < kBlockSize / kSectorSize; half++)
        {
            hr = m_fileIo.ReadAt (drive.path, GetOffset (drive, block, half), outData.subspan (half * kSectorSize, kSectorSize));
            CHR (hr);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkVirtualDrives::IsDosOrder
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkVirtualDrives::IsDosOrder (const Drive & drive) const
{
    std::string  extension = std::filesystem::path (drive.path).extension().string();
    bool         dosName   = _stricmp (extension.c_str(), ".dsk") == 0 || _stricmp (extension.c_str(), ".do") == 0;



    return dosName && drive.size == s_kDosOrderSize;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkVirtualDrives::GetOffset
//
//  In a DOS-order image, a block's two sectors are where DOS 3.3 puts them.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t CassoLinkVirtualDrives::GetOffset (const Drive & drive, uint32_t block, size_t half) const
{
    size_t  sector = (size_t) block * (kBlockSize / kSectorSize) + half;
    size_t  track  = sector / s_kSectorsPerTrack;



    if (!IsDosOrder (drive))
    {
        return (uint64_t) block * kBlockSize + half * kSectorSize;
    }

    return (uint64_t) (track * s_kSectorsPerTrack + s_kBlockSectorToDos[sector % s_kSectorsPerTrack]) * kSectorSize;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkVirtualDrives::FindDrive
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkVirtualDrives::Drive * CassoLinkVirtualDrives::FindDrive (int drive)
{
    bool  valid = drive >= 1 && drive <= (int) kDrives;



    return valid ? &m_drives[(size_t) drive - 1] : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkVirtualDrives::ToProDosError
//
////////////////////////////////////////////////////////////////////////////////

Byte CassoLinkVirtualDrives::ToProDosError (HRESULT hr)
{
    if (SUCCEEDED (hr))
    {
        return kNoError;
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_NOT_READY))
    {
        return kNoDevice;
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED) || hr == HRESULT_FROM_WIN32 (ERROR_WRITE_PROTECT))
    {
        return kWriteProtect;
    }

    return kIoError;
}
