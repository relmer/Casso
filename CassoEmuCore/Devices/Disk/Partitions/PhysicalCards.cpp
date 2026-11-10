#include "Pch.h"

#include "Devices/Disk/Partitions/PhysicalCards.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCards::MakeCardPath
//
////////////////////////////////////////////////////////////////////////////////

std::string PhysicalCards::MakeCardPath (int number)
{
    return std::format ("{}{}", kDrivePrefix, number);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCards::ParsePath
//
//  The prefix in any case, the drive's number in decimal, and then nothing
//  for a card or '|' and a partition's step for a partition.
//
////////////////////////////////////////////////////////////////////////////////

PhysicalCards::PathKind PhysicalCards::ParsePath (const std::string & path, int & outDrive, int & outPartition)
{
    size_t       i      = kDrivePrefix.size();
    size_t       digits = 0;
    int          drive  = 0;
    bool         isStep = false;
    std::string  rest;



    outDrive     = -1;
    outPartition = 0;

    if (path.size() <= kDrivePrefix.size() || _strnicmp (path.c_str(), kDrivePrefix.data(), kDrivePrefix.size()) != 0)
    {
        return PathKind::None;
    }

    while (i < path.size() && path[i] >= '0' && path[i] <= '9' && digits < kMaxDigits)
    {
        drive = drive * 10 + (path[i] - '0');
        digits++;
        i++;
    }

    if (digits == 0)
    {
        return PathKind::None;
    }

    outDrive = drive;

    if (i == path.size())
    {
        return PathKind::Card;
    }

    if (path[i] != '|')
    {
        return PathKind::Other;
    }

    rest   = path.substr (i + 1);
    isStep = PartitionAccess::TryParseStepName (rest, outPartition);

    return isStep ? PathKind::Partition : PathKind::Other;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCards::IsCardPath
//
////////////////////////////////////////////////////////////////////////////////

bool PhysicalCards::IsCardPath (const std::string & path)
{
    int  drive     = 0;
    int  partition = 0;



    return ParsePath (path, drive, partition) == PathKind::Card;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCards::IsCardPath
//
////////////////////////////////////////////////////////////////////////////////

bool PhysicalCards::IsCardPath (const std::wstring & path)
{
    return IsCardPath (TextEncoding::WideToUtf8 (path));
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCards::IsOnCard
//
//  True for a card, a partition of one, and anything inside a partition.
//
////////////////////////////////////////////////////////////////////////////////

bool PhysicalCards::IsOnCard (const std::wstring & path)
{
    int  drive     = 0;
    int  partition = 0;



    return ParsePath (TextEncoding::WideToUtf8 (path), drive, partition) != PathKind::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCards::FormatDrive
//
//  Gigabytes to one decimal place from a gigabyte up. A drive with no model
//  is "Drive 2".
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PhysicalCards::FormatDrive (const PhysicalDrive & drive)
{
    static constexpr uint64_t  kGigabyte = 1024ull * 1024 * 1024;



    uint64_t     tenths = (drive.bytes * 10 + kGigabyte / 2) / kGigabyte;
    std::string  model  = drive.model.empty() ? std::format ("Drive {}", drive.number) : drive.model;
    std::string  size   = PartitionAccess::FormatSize (drive.bytes);



    if (drive.bytes >= kGigabyte)
    {
        size = std::format ("{}.{} GB", tenths / 10, tenths % 10);
    }

    return TextEncoding::Utf8ToWide (std::format ("{} ({})", model, size));
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCards::ListRemovableDrives
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PhysicalCards::ListRemovableDrives (std::vector<PhysicalDrive> & outDrives)
{
    HRESULT                     hr = S_OK;
    std::vector<PhysicalDrive>  drives;



    outDrives.clear();

    hr = m_host.ListDrives (drives);
    CHR (hr);

    for (PhysicalDrive & drive : drives)
    {
        if (drive.removable)
        {
            outDrives.push_back (std::move (drive));
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCards::Open
//
//  The drive is opened outside the lock, as the UAC prompt can stay up
//  until the user answers it. A card that is open already is closed
//  first, so the blocks are read again.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PhysicalCards::Open (int number, HWND owner)
{
    HRESULT                     hr        = S_OK;
    bool                        removable = false;
    std::vector<PhysicalDrive>  drives;
    Session                     session;



    hr = ListRemovableDrives (drives);
    CHR (hr);

    for (const PhysicalDrive & drive : drives)
    {
        removable = removable || drive.number == number;
    }

    CBREx (removable, E_ACCESSDENIED);

    Close (number);

    hr = m_host.OpenDrive (number, owner, session.drive, session.store);
    CHR (hr);

    CBREx (session.store != nullptr, E_UNEXPECTED);

    session.cache = std::make_unique<CachingBlockStore> (*session.store);

    {
        std::lock_guard<std::mutex>  lock (m_mutex);

        session.stamp = m_nextStamp++;
        m_sessions.erase (number);
        m_sessions.emplace (number, std::move (session));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCards::Close
//
////////////////////////////////////////////////////////////////////////////////

void PhysicalCards::Close (int number)
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    m_sessions.erase (number);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCards::IsOpen
//
////////////////////////////////////////////////////////////////////////////////

bool PhysicalCards::IsOpen (int number)
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    return m_sessions.contains (number);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCards::TryGetDrive
//
////////////////////////////////////////////////////////////////////////////////

bool PhysicalCards::TryGetDrive (int number, PhysicalDrive & outDrive)
{
    std::lock_guard<std::mutex>  lock (m_mutex);
    auto                         found = m_sessions.find (number);



    outDrive = {};

    if (found == m_sessions.end())
    {
        return false;
    }

    outDrive = found->second.drive;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCards::ListPartitions
//
//  The list is made once for each open of the card and after each write.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PhysicalCards::ListPartitions (int number, PartitionLayoutKind & outKind, std::vector<PartitionEntry> & outEntries)
{
    HRESULT                      hr      = S_OK;
    std::lock_guard<std::mutex>  lock (m_mutex);
    Session                    * session = nullptr;



    outKind = PartitionLayoutKind::None;
    outEntries.clear();

    hr = FindSession (number, session);
    CHR (hr);

    hr = ListSession (*session);
    CHR (hr);

    outKind    = session->kind;
    outEntries = session->entries;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCards::ReadPartition
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PhysicalCards::ReadPartition (int number, int partition, std::vector<Byte> & outBytes)
{
    HRESULT                      hr      = S_OK;
    std::lock_guard<std::mutex>  lock (m_mutex);
    Session                    * session = nullptr;
    const PartitionEntry       * entry   = nullptr;



    outBytes.clear();

    hr = FindSession (number, session);
    CHR (hr);

    hr = FindPartition (*session, partition, entry);
    CHR (hr);

    hr = PartitionAccess::ReadBytes (*session->cache, entry->partition, outBytes);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCards::WritePartition
//
//  The write is staged first to find whether any block differs, and a write
//  that changes nothing succeeds without a question. The staged blocks are
//  dropped while the question is up, so a read on another thread returns
//  the card's blocks, and staged again after the user confirms.
//
//  A write made while a question is up fails with ERROR_BUSY. After a write
//  that fails partway the card is closed, as its blocks are then unknown.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PhysicalCards::WritePartition (int number, int partition, std::span<const Byte> bytes)
{
    HRESULT                       hr        = S_OK;
    std::unique_lock<std::mutex>  lock (m_mutex);
    Session                     * session   = nullptr;
    bool                          confirmed = false;
    PhysicalDrive                 drive;



    CBREx (!m_confirming, HRESULT_FROM_WIN32 (ERROR_BUSY));

    hr = StageWrite (number, partition, bytes, session);
    CHR (hr);

    BAIL_OUT_IF (session->cache->GetChangedBlockCount() == 0, S_OK);

    session->cache->Discard();
    drive        = session->drive;
    m_confirming = true;

    lock.unlock();
    confirmed = m_confirmWrite && m_confirmWrite (drive);
    lock.lock();

    m_confirming = false;

    CBREx (confirmed, HRESULT_FROM_WIN32 (ERROR_CANCELLED));

    hr = StageWrite (number, partition, bytes, session);
    CHR (hr);

    hr = session->cache->Commit();

    if (FAILED (hr))
    {
        m_sessions.erase (number);
    }

    CHR (hr);

    session->stamp  = m_nextStamp++;
    session->listed = false;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCards::GetStamp
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PhysicalCards::GetStamp (int number, int partition, FileStamp & outStamp)
{
    HRESULT                      hr      = S_OK;
    std::lock_guard<std::mutex>  lock (m_mutex);
    Session                    * session = nullptr;
    const PartitionEntry       * entry   = nullptr;



    outStamp = {};

    hr = FindSession (number, session);
    CHR (hr);

    hr = FindPartition (*session, partition, entry);
    CHR (hr);

    outStamp.sizeBytes    = entry->byteCount;
    outStamp.modifiedUnix = session->stamp;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCards::FindSession
//
//  Called with the lock taken.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PhysicalCards::FindSession (int number, Session * & outSession)
{
    HRESULT  hr     = S_OK;
    auto     found  = m_sessions.find (number);
    bool     isOpen = found != m_sessions.end();



    outSession = nullptr;

    CBREx (isOpen, HRESULT_FROM_WIN32 (ERROR_DEVICE_NOT_CONNECTED));

    outSession = &found->second;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCards::ListSession
//
//  Lists the card's partitions when the session has no list.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PhysicalCards::ListSession (Session & session)
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (session.listed, S_OK);

    hr = PartitionAccess::ListStore (*session.cache, session.kind, session.entries);
    CHR (hr);

    session.listed = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCards::FindPartition
//
//  A partition that ends past the end of the card fails with ERROR_FILE_CORRUPT, and one
//  longer than kMaxPartitionBytes with ERROR_NOT_SUPPORTED.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PhysicalCards::FindPartition (Session & session, int partition, const PartitionEntry * & outEntry)
{
    HRESULT  hr = S_OK;



    outEntry = nullptr;

    hr = ListSession (session);
    CHR (hr);

    for (const PartitionEntry & entry : session.entries)
    {
        if (outEntry == nullptr && entry.partition.index == partition)
        {
            outEntry = &entry;
        }
    }

    CBREx (outEntry != nullptr,                      HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));
    CBREx (!outEntry->partition.damaged,             HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));
    CBREx (outEntry->byteCount <= kMaxPartitionBytes, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

Error:
    if (FAILED (hr))
    {
        outEntry = nullptr;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCards::StageWrite
//
//  The partition's bytes with `bytes` over their start, written to the
//  cache and not committed. Called with the lock taken.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PhysicalCards::StageWrite (int number, int partition, std::span<const Byte> bytes, Session * & outSession)
{
    HRESULT                 hr      = S_OK;
    Session               * session = nullptr;
    const PartitionEntry  * entry   = nullptr;
    std::vector<Byte>       current;
    bool                    fits    = false;



    outSession = nullptr;

    hr = FindSession (number, session);
    CHR (hr);

    hr = FindPartition (*session, partition, entry);
    CHR (hr);

    fits = bytes.size() <= entry->byteCount;
    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH));

    hr = PartitionAccess::ReadBytes (*session->cache, entry->partition, current);
    CHR (hr);

    std::copy (bytes.begin(), bytes.end(), current.begin());

    hr = PartitionAccess::WriteBytes (*session->cache, entry->partition, current);
    CHR (hr);

    outSession = session;

Error:
    if (FAILED (hr) && session != nullptr)
    {
        session->cache->Discard();
    }

    return hr;
}