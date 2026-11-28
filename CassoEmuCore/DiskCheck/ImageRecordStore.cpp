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
//  ImageRecordStore::Remove
//
////////////////////////////////////////////////////////////////////////////////

void ImageRecordStore::Remove (const std::wstring & path)
{
    HRESULT          hr = S_OK;
    std::string      hash;
    std::error_code  error;



    hr = GetHash (path, hash);
    CHR (hr);

    {
        std::lock_guard  lock (m_lock);

        m_records.erase (hash);
        m_previous.erase (path);
    }

    std::filesystem::remove (GetCachePath (hash), error);

Error:
    return;
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
//  The record for the image's bytes: from memory, then the cache folder.
//  On false, outRecord is empty except for its hash.
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
//  The record into memory and the cache folder. A
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
