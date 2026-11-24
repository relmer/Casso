#include "Pch.h"

#include "DiskCheck/ImageRecordStore.h"
#include "CassoLink/FullSend/FullSendDisk.h"
#include "Core/PathResolver.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordStore::ImageRecordStore
//
////////////////////////////////////////////////////////////////////////////////

ImageRecordStore::ImageRecordStore (std::filesystem::path folder) :
    m_folder (std::move (folder))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordStore::GetDefaultFolder
//
////////////////////////////////////////////////////////////////////////////////

std::filesystem::path ImageRecordStore::GetDefaultFolder()
{
    std::filesystem::path  dataFolder = PathResolver::GetDataFolder();



    return dataFolder.empty() ? std::filesystem::path() : dataFolder / L"ImageRecords";
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordStore::FindContents
//
////////////////////////////////////////////////////////////////////////////////

bool ImageRecordStore::FindContents (const std::wstring & path, ImageContents & outContents)
{
    Record  record;
    bool    isFound = Load (path, record) && record.hasContents;



    if (isFound)
    {
        outContents = std::move (record.contents);
    }

    return isFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordStore::StoreContents
//
//  Replaces the contents in the image's record. A saved game found for the
//  same bytes stays in it.
//
////////////////////////////////////////////////////////////////////////////////

void ImageRecordStore::StoreContents (const std::wstring & path, const ImageContents & contents)
{
    Record  record;
    bool    isLoaded = Load (path, record);



    IGNORE_RETURN_VALUE (isLoaded, true);

    record.hasContents = true;
    record.contents    = contents;

    Save (path, record);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordStore::FindSavedGame
//
////////////////////////////////////////////////////////////////////////////////

IImageRecords::SavedGame ImageRecordStore::FindSavedGame (const std::wstring & path)
{
    Record  record;
    bool    isFound = Load (path, record);



    return isFound ? record.savedGame : SavedGame::Unknown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordStore::StoreSavedGame
//
////////////////////////////////////////////////////////////////////////////////

void ImageRecordStore::StoreSavedGame (const std::wstring & path, bool hasSavedGame)
{
    Record     record;
    bool       isLoaded  = Load (path, record);
    SavedGame  savedGame = hasSavedGame ? SavedGame::Present : SavedGame::None;



    IGNORE_RETURN_VALUE (isLoaded, true);

    if (record.savedGame != savedGame || record.hash.empty())
    {
        record.savedGame = savedGame;
        Save (path, record);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordStore::GetHash
//
//  E_NOT_SUFFICIENT_BUFFER for an image larger than kMaxImageBytes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ImageRecordStore::GetHash (const std::wstring & path, std::string & outHash)
{
    HRESULT                    hr       = S_OK;
    WIN32_FILE_ATTRIBUTE_DATA  data     = {};
    BOOL                       hasData  = ::GetFileAttributesExW (path.c_str(), GetFileExInfoStandard, &data);
    uint64_t                   size     = ((uint64_t) data.nFileSizeHigh << 32) | data.nFileSizeLow;
    int64_t                    time     = (int64_t) (((uint64_t) data.ftLastWriteTime.dwHighDateTime << 32) | data.ftLastWriteTime.dwLowDateTime);
    std::string                text;
    std::vector<Byte>          bytes;
    bool                       isKnown  = false;
    bool                       isSmall  = size <= kMaxImageBytes;
    bool                       isRead   = false;



    outHash.clear();

    CBREx (hasData, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));
    CBREx (isSmall, E_NOT_SUFFICIENT_BUFFER);

    {
        std::lock_guard  lock (m_lock);
        auto             found = m_index.find (path);

        isKnown = found != m_index.end() && found->second.size == size && found->second.time == time;
        outHash = isKnown ? found->second.hash : std::string();
    }

    BAIL_OUT_IF (isKnown, S_OK);

    isRead = ReadText (path, text);
    CBREx (isRead, HRESULT_FROM_WIN32 (ERROR_READ_FAULT));

    bytes.assign (text.begin(), text.end());

    hr = FullSendDisk::ComputeHash (bytes, outHash);
    CHR (hr);

    {
        std::lock_guard  lock (m_lock);

        m_index[path] = { size, time, outHash };
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordStore::Load
//
//  The record for the image's bytes: from memory, then the cache folder,
//  then the image's stream. A record found only in the stream is copied
//  into the cache. On false, outRecord is empty except for its hash.
//
////////////////////////////////////////////////////////////////////////////////

bool ImageRecordStore::Load (const std::wstring & path, Record & outRecord)
{
    HRESULT      hr        = S_OK;
    std::string  hash;
    std::string  text;
    bool         isFound   = false;
    bool         isInCache = false;



    outRecord = Record();

    hr = GetHash (path, hash);
    CHR (hr);

    {
        std::lock_guard  lock (m_lock);
        auto             found = m_records.find (hash);

        isFound   = found != m_records.end();
        outRecord = isFound ? found->second : Record();
    }

    BAIL_OUT_IF (isFound, S_OK);

    isInCache = ReadText (GetCachePath (hash).wstring(), text);
    isFound   = isInCache && ImageRecordCodec::Parse (text, outRecord) && outRecord.hash == hash;

    if (!isFound)
    {
        isFound = ReadText (path + kStreamName, text) && ImageRecordCodec::Parse (text, outRecord) && outRecord.hash == hash;

        if (isFound)
        {
            hr = WriteText (GetCachePath (hash).wstring(), text);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }

    if (isFound)
    {
        std::lock_guard  lock (m_lock);

        m_records[hash] = outRecord;
    }

Error:
    if (!isFound)
    {
        outRecord      = Record();
        outRecord.hash = hash;
    }

    return isFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordStore::Save
//
//  The record into memory, the cache folder and the image's stream. A
//  record of the path's earlier bytes is deleted from the cache, so each
//  image has the one record for what it contains now.
//
////////////////////////////////////////////////////////////////////////////////

void ImageRecordStore::Save (const std::wstring & path, const Record & record)
{
    HRESULT      hr       = S_OK;
    std::string  text;
    std::string  earlier;



    BAIL_OUT_IF (record.hash.empty(), S_OK);

    text = ImageRecordCodec::Format (record);

    {
        std::lock_guard  lock (m_lock);
        auto             found = m_previous.find (path);

        earlier = (found != m_previous.end() && found->second != record.hash) ? found->second : std::string();

        m_records[record.hash] = record;
        m_previous[path]       = record.hash;
    }

    if (!earlier.empty())
    {
        std::error_code  error;

        std::filesystem::remove (GetCachePath (earlier), error);
    }

    hr = WriteText (GetCachePath (record.hash).wstring(), text);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = WriteStream (path, text);
    IGNORE_RETURN_VALUE (hr, S_OK);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordStore::GetCachePath
//
////////////////////////////////////////////////////////////////////////////////

std::filesystem::path ImageRecordStore::GetCachePath (const std::string & hash) const
{
    return m_folder / (TextEncoding::NarrowToWide (hash) + L".json");
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordStore::ReadText
//
////////////////////////////////////////////////////////////////////////////////

bool ImageRecordStore::ReadText (const std::wstring & path, std::string & outText)
{
    std::ifstream  in (path, std::ios::binary);
    bool           isOpen = in.is_open();



    outText.clear();

    if (isOpen)
    {
        outText.assign (std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>());
    }

    return isOpen && !in.bad();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordStore::WriteText
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ImageRecordStore::WriteText (const std::wstring & path, const std::string & text)
{
    HRESULT          hr     = S_OK;
    std::error_code  error;
    std::ofstream    out;
    bool             isGood = false;



    std::filesystem::create_directories (std::filesystem::path (path).parent_path(), error);

    out.open (path, std::ios::binary | std::ios::trunc);
    out.write (text.data(), (std::streamsize) text.size());

    isGood = out.good();
    CBREx (isGood, HRESULT_FROM_WIN32 (ERROR_WRITE_FAULT));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordStore::WriteStream
//
//  The record into the image's stream, with the file's times and read-only
//  attribute as they were: a write to any stream of a file sets its last
//  write time, by which a backup or a sync would take the image for a
//  changed one. A stream with the same text isn't written again, and a
//  drive with no streams isn't touched.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ImageRecordStore::WriteStream (const std::wstring & image, const std::string & text)
{
    HRESULT                    hr               = S_OK;
    WIN32_FILE_ATTRIBUTE_DATA  before           = {};
    BOOL                       hasTimes         = ::GetFileAttributesExW (image.c_str(), GetFileExInfoStandard, &before);
    std::wstring               stream           = image + kStreamName;
    wchar_t                    volume[MAX_PATH] = {};
    DWORD                      flags            = 0;
    BOOL                       hasVolume        = ::GetVolumePathNameW (image.c_str(), volume, MAX_PATH);
    bool                       isReadOnly       = hasTimes && (before.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0;
    bool                       isCleared        = false;
    bool                       isTouched        = false;
    std::string                existing;
    HANDLE                     file             = INVALID_HANDLE_VALUE;
    HANDLE                     whole            = INVALID_HANDLE_VALUE;
    DWORD                      written          = 0;
    BOOL                       isDone           = FALSE;



    CBREx (hasTimes, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    hasVolume = hasVolume && ::GetVolumeInformationW (volume, nullptr, 0, nullptr, nullptr, &flags, nullptr, 0);
    BAIL_OUT_IF (!hasVolume || (flags & FILE_NAMED_STREAMS) == 0, S_OK);

    BAIL_OUT_IF (ReadText (stream, existing) && existing == text, S_OK);

    if (isReadOnly)
    {
        isCleared = ::SetFileAttributesW (image.c_str(), before.dwFileAttributes & ~FILE_ATTRIBUTE_READONLY) != FALSE;
        CBREx (isCleared, HRESULT_FROM_WIN32 (::GetLastError()));
    }

    file = ::CreateFileW (stream.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    CBREx (file != INVALID_HANDLE_VALUE, HRESULT_FROM_WIN32 (::GetLastError()));

    isTouched = true;
    isDone    = ::WriteFile (file, text.data(), (DWORD) text.size(), &written, nullptr) && written == (DWORD) text.size();
    ::CloseHandle (file);

    CBREx (isDone, HRESULT_FROM_WIN32 (ERROR_WRITE_FAULT));

Error:
    if (isTouched)
    {
        whole = ::CreateFileW (image.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    }

    if (whole != INVALID_HANDLE_VALUE)
    {
        ::SetFileTime (whole, &before.ftCreationTime, &before.ftLastAccessTime, &before.ftLastWriteTime);
        ::CloseHandle (whole);
    }

    if (isCleared)
    {
        ::SetFileAttributesW (image.c_str(), before.dwFileAttributes);
    }

    return hr;
}
