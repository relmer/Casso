#include "Pch.h"

#include "Devices/Disk/Partitions/PhysicalCardFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCardFileIo::ReadAllBytes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PhysicalCardFileIo::ReadAllBytes (const std::string & path, std::vector<Byte> & outBytes)
{
    HRESULT  hr      = S_OK;
    Target   target  = Resolve (path);
    bool     pending = false;



    switch (target.kind)
    {
        case PhysicalCards::PathKind::None:
            hr = m_inner.ReadAllBytes (path, outBytes);
            break;

        case PhysicalCards::PathKind::Partition:
            hr = m_cards.ReadPartition (target.drive, target.partition, outBytes);
            break;

        case PhysicalCards::PathKind::Other:
            pending = TryGetPending (path, outBytes, false);
            hr      = pending ? S_OK : HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
            break;

        default:
            outBytes.clear();
            hr = HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED);
            break;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCardFileIo::WriteAllBytes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PhysicalCardFileIo::WriteAllBytes (const std::string & path, const std::vector<Byte> & bytes)
{
    HRESULT  hr     = S_OK;
    Target   target = Resolve (path);



    switch (target.kind)
    {
        case PhysicalCards::PathKind::None:
            hr = m_inner.WriteAllBytes (path, bytes);
            break;

        case PhysicalCards::PathKind::Partition:
            hr = m_cards.WritePartition (target.drive, target.partition, bytes);
            break;

        case PhysicalCards::PathKind::Other:
        {
            std::lock_guard<std::mutex>  lock (m_mutex);

            m_pending[path] = bytes;
            break;
        }

        default:
            hr = HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED);
            break;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCardFileIo::WriteAt
//
//  A partition is written whole, never in place.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PhysicalCardFileIo::WriteAt (const std::string & path, uint64_t offset, std::span<const Byte> bytes)
{
    bool  isOnDrive = Resolve (path).kind != PhysicalCards::PathKind::None;



    return isOnDrive ? HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED) : m_inner.WriteAt (path, offset, bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCardFileIo::ReadAt
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PhysicalCardFileIo::ReadAt (const std::string & path, uint64_t offset, std::span<Byte> outBytes)
{
    HRESULT            hr     = S_OK;
    bool               inside = false;
    Target             target = Resolve (path);
    std::vector<Byte>  bytes;



    if (target.kind == PhysicalCards::PathKind::None)
    {
        return m_inner.ReadAt (path, offset, outBytes);
    }

    if (target.kind == PhysicalCards::PathKind::Partition)
    {
        return m_cards.ReadPartitionAt (target.drive, target.partition, offset, outBytes);
    }

    hr = ReadAllBytes (path, bytes);
    CHR (hr);

    inside = offset <= bytes.size() && outBytes.size() <= bytes.size() - offset;
    CBREx (inside, HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF));

    std::copy_n (bytes.begin() + (ptrdiff_t) offset, outBytes.size(), outBytes.begin());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCardFileIo::Stat
//
//  A card's size is the drive's; its time is zero.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PhysicalCardFileIo::Stat (const std::string & path, FileStamp & outStamp)
{
    HRESULT            hr     = S_OK;
    Target             target = Resolve (path);
    bool               found  = false;
    PhysicalDrive      drive;
    std::vector<Byte>  bytes;



    outStamp = {};

    switch (target.kind)
    {
        case PhysicalCards::PathKind::None:
            hr = m_inner.Stat (path, outStamp);
            break;

        case PhysicalCards::PathKind::Partition:
            hr = m_cards.GetStamp (target.drive, target.partition, outStamp);
            break;

        case PhysicalCards::PathKind::Card:
            found              = m_cards.TryGetDrive (target.drive, drive);
            outStamp.sizeBytes = drive.bytes;
            hr                 = found ? S_OK : HRESULT_FROM_WIN32 (ERROR_DEVICE_NOT_CONNECTED);
            break;

        default:
            found              = TryGetPending (path, bytes, false);
            outStamp.sizeBytes = bytes.size();
            hr                 = found ? S_OK : HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
            break;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCardFileIo::Exists
//
////////////////////////////////////////////////////////////////////////////////

bool PhysicalCardFileIo::Exists (const std::string & path)
{
    HRESULT            hr     = S_OK;
    Target             target = Resolve (path);
    bool               exists = false;
    FileStamp          stamp;
    std::vector<Byte>  bytes;



    switch (target.kind)
    {
        case PhysicalCards::PathKind::None:
            exists = m_inner.Exists (path);
            break;

        case PhysicalCards::PathKind::Partition:
            hr     = m_cards.GetStamp (target.drive, target.partition, stamp);
            exists = SUCCEEDED (hr);
            break;

        case PhysicalCards::PathKind::Card:
            exists = m_cards.IsOpen (target.drive);
            break;

        default:
            exists = TryGetPending (path, bytes, false);
            break;
    }

    return exists;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCardFileIo::Remove
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PhysicalCardFileIo::Remove (const std::string & path)
{
    HRESULT            hr      = S_OK;
    Target             target  = Resolve (path);
    bool               removed = false;
    std::vector<Byte>  bytes;



    switch (target.kind)
    {
        case PhysicalCards::PathKind::None:
            hr = m_inner.Remove (path);
            break;

        case PhysicalCards::PathKind::Other:
            removed = TryGetPending (path, bytes, true);
            hr      = removed ? S_OK : HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
            break;

        default:
            hr = HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED);
            break;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCardFileIo::ListFiles
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PhysicalCardFileIo::ListFiles (const std::string & folder, std::vector<std::string> & outNames)
{
    return m_inner.ListFiles (folder, outNames);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCardFileIo::ReplaceAtomically
//
//  Onto a partition, the temporary file's bytes are written over the
//  partition's start.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PhysicalCardFileIo::ReplaceAtomically (const std::string & tempPath, const std::string & targetPath)
{
    HRESULT            hr      = S_OK;
    Target             target  = Resolve (targetPath);
    bool               pending = false;
    std::vector<Byte>  bytes;



    if (target.kind == PhysicalCards::PathKind::None && Resolve (tempPath).kind == PhysicalCards::PathKind::None)
    {
        return m_inner.ReplaceAtomically (tempPath, targetPath);
    }

    CBREx (target.kind == PhysicalCards::PathKind::Partition, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    pending = TryGetPending (tempPath, bytes, true);
    CBREx (pending, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    hr = m_cards.WritePartition (target.drive, target.partition, bytes);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCardFileIo::FlushToStorage
//
//  A write to a card is on the card when WritePartition returns.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PhysicalCardFileIo::FlushToStorage (const std::string & path)
{
    bool  isOnDrive = Resolve (path).kind != PhysicalCards::PathKind::None;



    return isOnDrive ? S_OK : m_inner.FlushToStorage (path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCardFileIo::CopyFileMetadata
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PhysicalCardFileIo::CopyFileMetadata (const std::string & fromPath, const std::string & toPath)
{
    bool  isOnDrive = Resolve (fromPath).kind != PhysicalCards::PathKind::None || Resolve (toPath).kind != PhysicalCards::PathKind::None;



    return isOnDrive ? S_OK : m_inner.CopyFileMetadata (fromPath, toPath);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCardFileIo::RenameWithoutReplacing
//
//  Nothing on a drive can be created, and a partition is there already.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PhysicalCardFileIo::RenameWithoutReplacing (const std::string & tempPath, const std::string & targetPath)
{
    Target  target = Resolve (targetPath);



    if (target.kind == PhysicalCards::PathKind::None && Resolve (tempPath).kind == PhysicalCards::PathKind::None)
    {
        return m_inner.RenameWithoutReplacing (tempPath, targetPath);
    }

    return HRESULT_FROM_WIN32 ((target.kind == PhysicalCards::PathKind::Partition) ? ERROR_ALREADY_EXISTS : ERROR_NOT_SUPPORTED);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCardFileIo::IsHeldByAnotherProcess
//
////////////////////////////////////////////////////////////////////////////////

bool PhysicalCardFileIo::IsHeldByAnotherProcess (const std::string & path)
{
    bool  isOnDrive = Resolve (path).kind != PhysicalCards::PathKind::None;



    return isOnDrive ? false : m_inner.IsHeldByAnotherProcess (path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCardFileIo::WritePayloadToStandardOutput
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PhysicalCardFileIo::WritePayloadToStandardOutput (const std::vector<Byte> & bytes)
{
    return m_inner.WritePayloadToStandardOutput (bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCardFileIo::Resolve
//
////////////////////////////////////////////////////////////////////////////////

PhysicalCardFileIo::Target PhysicalCardFileIo::Resolve (const std::string & path)
{
    Target  target;



    target.kind = PhysicalCards::ParsePath (path, target.drive, target.partition);

    return target;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCardFileIo::TryGetPending
//
//  The bytes of a temporary file stored in memory, which `remove` erases.
//
////////////////////////////////////////////////////////////////////////////////

bool PhysicalCardFileIo::TryGetPending (const std::string & path, std::vector<Byte> & outBytes, bool remove)
{
    std::lock_guard<std::mutex>  lock (m_mutex);
    auto                         found = m_pending.find (path);



    outBytes.clear();

    if (found == m_pending.end())
    {
        return false;
    }

    if (remove)
    {
        outBytes = std::move (found->second);
        m_pending.erase (found);
    }
    else
    {
        outBytes = found->second;
    }

    return true;
}