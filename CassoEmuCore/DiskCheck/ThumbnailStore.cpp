#include "Pch.h"

#include "DiskCheck/ThumbnailStore.h"
#include "CassoLink/FullSend/FullSendDisk.h"
#include "Core/JsonParser.h"
#include "Core/JsonValue.h"
#include "Core/JsonWriter.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailStore::ThumbnailStore
//
////////////////////////////////////////////////////////////////////////////////

ThumbnailStore::ThumbnailStore (std::filesystem::path folder) :
    m_folder (std::move (folder))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailStore::GetDefaultFolder
//
////////////////////////////////////////////////////////////////////////////////

std::filesystem::path ThumbnailStore::GetDefaultFolder()
{
    wchar_t  local[MAX_PATH] = {};
    DWORD    length          = ::GetEnvironmentVariableW (L"LOCALAPPDATA", local, MAX_PATH);



    return (length == 0 || length >= MAX_PATH) ? std::filesystem::path() : std::filesystem::path (local) / L"Casso" / L"Thumbnails";
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailStore::GetPath
//
////////////////////////////////////////////////////////////////////////////////

std::filesystem::path ThumbnailStore::GetPath (const std::string & hash, const wchar_t * suffix) const
{
    return m_folder / (TextEncoding::NarrowToWide (hash) + suffix);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailStore::ReadFile
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThumbnailStore::ReadFile (const std::filesystem::path & path, std::vector<Byte> & outBytes)
{
    HRESULT        hr = S_OK;
    std::ifstream  in (path, std::ios::binary);
    bool           isOpen = in.is_open();



    outBytes.clear();

    CBREx (isOpen, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    outBytes.assign (std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailStore::WriteFile
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThumbnailStore::WriteFile (const std::filesystem::path & path, std::span<const Byte> bytes)
{
    HRESULT          hr     = S_OK;
    std::error_code  error;
    std::ofstream    out;
    bool             isGood = false;



    std::filesystem::create_directories (path.parent_path(), error);

    out.open (path, std::ios::binary | std::ios::trunc);
    out.write ((const char *) bytes.data(), (std::streamsize) bytes.size());

    isGood = out.good();
    CBREx (isGood, HRESULT_FROM_WIN32 (ERROR_WRITE_FAULT));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailStore::WriteStream
//
//  The PNG into the image file's stream, and the file's times as they were:
//  a write to any stream of a file sets its last write time, by which a
//  backup or a sync would take the image for a changed one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThumbnailStore::WriteStream (const std::filesystem::path & image, std::span<const Byte> png)
{
    HRESULT                    hr       = S_OK;
    WIN32_FILE_ATTRIBUTE_DATA  before   = {};
    BOOL                       hasTimes = ::GetFileAttributesExW (image.c_str(), GetFileExInfoStandard, &before);
    std::wstring               stream   = image.wstring() + kStreamName;
    HANDLE                     file     = INVALID_HANDLE_VALUE;
    HANDLE                     whole    = INVALID_HANDLE_VALUE;
    DWORD                      written  = 0;
    BOOL                       isDone   = FALSE;



    CBREx (hasTimes, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    file = ::CreateFileW (stream.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    CBREx (file != INVALID_HANDLE_VALUE, HRESULT_FROM_WIN32 (::GetLastError()));

    isDone = ::WriteFile (file, png.data(), (DWORD) png.size(), &written, nullptr) && written == (DWORD) png.size();
    CBREx (isDone, HRESULT_FROM_WIN32 (ERROR_WRITE_FAULT));

Error:
    if (file != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle (file);
    }

    if (hasTimes)
    {
        whole = ::CreateFileW (image.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    }

    if (whole != INVALID_HANDLE_VALUE)
    {
        ::SetFileTime (whole, &before.ftCreationTime, &before.ftLastAccessTime, &before.ftLastWriteTime);
        ::CloseHandle (whole);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailStore::CopyStream
//
//  A write that renames a temporary file over an image replaces the image's
//  streams with the temporary's, so the thumbnail is copied to it first. The
//  image's hash changes with the write; Find takes the stream into the cache
//  for the new hash.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThumbnailStore::CopyStream (const std::filesystem::path & from, const std::filesystem::path & to)
{
    HRESULT            hr        = S_OK;
    HRESULT            hrRead    = S_OK;
    std::vector<Byte>  png;
    bool               hasStream = false;



    hrRead    = ReadFile (std::filesystem::path (from.wstring() + kStreamName), png);
    hasStream = SUCCEEDED (hrRead) && !png.empty();
    BAIL_OUT_IF (!hasStream, S_OK);

    hr = WriteStream (to, png);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailStore::GetHash
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThumbnailStore::GetHash (const std::filesystem::path & image, std::string & outHash)
{
    HRESULT                    hr       = S_OK;
    WIN32_FILE_ATTRIBUTE_DATA  data     = {};
    BOOL                       hasData  = ::GetFileAttributesExW (image.c_str(), GetFileExInfoStandard, &data);
    uint64_t                   size     = ((uint64_t) data.nFileSizeHigh << 32) | data.nFileSizeLow;
    int64_t                    time     = (int64_t) (((uint64_t) data.ftLastWriteTime.dwHighDateTime << 32) | data.ftLastWriteTime.dwLowDateTime);
    std::wstring               key      = image.wstring();
    std::vector<Byte>          bytes;
    bool                       isKnown  = false;



    outHash.clear();

    CBREx (hasData, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    {
        std::lock_guard  lock (m_lock);
        auto             found = m_index.find (key);

        isKnown = found != m_index.end() && found->second.size == size && found->second.time == time;
        outHash = isKnown ? found->second.hash : std::string();
    }

    BAIL_OUT_IF (isKnown, S_OK);

    hr = ReadFile (image, bytes);
    CHR (hr);

    hr = FullSendDisk::ComputeHash (bytes, outHash);
    CHR (hr);

    {
        std::lock_guard  lock (m_lock);

        m_index[key] = { size, time, outHash };
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailStore::ReadRecord
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThumbnailStore::ReadRecord (const std::string & hash, ThumbnailRecord & outRecord) const
{
    HRESULT            hr = S_OK;
    std::vector<Byte>  bytes;
    JsonValue          root;
    JsonParseError     parseError;



    outRecord = ThumbnailRecord();

    hr = ReadFile (GetPath (hash, s_kRecord), bytes);
    CHR (hr);

    hr = JsonParser::Parse (std::string (bytes.begin(), bytes.end()), root, parseError);
    CHR (hr);

    root.HasString ("verdict", outRecord.verdict);
    root.HasString ("machine", outRecord.machine);
    root.HasString ("reason",  outRecord.reason);
    root.HasString ("title",   outRecord.title);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailStore::WriteRecord
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThumbnailStore::WriteRecord (const std::string & hash, const ThumbnailRecord & record) const
{
    HRESULT                                         hr = S_OK;
    std::vector<std::pair<std::string, JsonValue>>  fields;
    JsonWriter::Options                             options;
    std::string                                     text;



    fields.emplace_back ("verdict", JsonValue (record.verdict));
    fields.emplace_back ("machine", JsonValue (record.machine));
    fields.emplace_back ("reason",  JsonValue (record.reason));
    fields.emplace_back ("title",   JsonValue (record.title));

    hr = JsonWriter::Write (JsonValue (std::move (fields)), options, text);
    CHR (hr);

    hr = WriteFile (GetPath (hash, s_kRecord), std::span<const Byte> ((const Byte *) text.data(), text.size()));
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailStore::Find
//
//  The thumbnail the user set, or the booted one, or the stream's when the
//  cache has neither, which is then taken into the cache. A stream that
//  isn't the thumbnail returned is written again; a failure to write it,
//  as on a FAT volume or a read-only file, isn't an error.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThumbnailStore::Find (const std::filesystem::path & image, std::vector<Byte> & outPng, ThumbnailRecord & outRecord, bool & outIsUserSet)
{
    HRESULT            hr       = S_OK;
    HRESULT            hrRead   = S_OK;
    HRESULT            hrStream = S_OK;
    std::string        hash;
    std::vector<Byte>  stream;



    outPng.clear();
    outRecord    = ThumbnailRecord();
    outIsUserSet = false;

    hr = GetHash (image, hash);
    CHR (hr);

    hrRead       = ReadFile (GetPath (hash, s_kByUser), outPng);
    outIsUserSet = SUCCEEDED (hrRead);

    if (!outIsUserSet)
    {
        hrRead = ReadFile (GetPath (hash, s_kBooted), outPng);
    }

    hrStream = ReadFile (std::filesystem::path (image.wstring() + kStreamName), stream);

    if (FAILED (hrRead) && SUCCEEDED (hrStream) && !stream.empty())
    {
        outPng = stream;

        hrRead = WriteFile (GetPath (hash, s_kBooted), outPng);
    }

    BAIL_OUT_IF (outPng.empty(), S_OK);

    if (FAILED (hrStream) || stream != outPng)
    {
        hrStream = WriteStream (image, outPng);
        IGNORE_RETURN_VALUE (hrStream, S_OK);
    }

    hrRead = ReadRecord (hash, outRecord);
    IGNORE_RETURN_VALUE (hrRead, S_OK);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailStore::Store
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThumbnailStore::Store (const std::filesystem::path & image, std::span<const Byte> png, const ThumbnailRecord & record)
{
    HRESULT          hr       = S_OK;
    HRESULT          hrStream = S_OK;
    std::string      hash;
    std::error_code  error;
    bool             hasUsers = false;



    hr = GetHash (image, hash);
    CHR (hr);

    hr = WriteFile (GetPath (hash, s_kBooted), png);
    CHR (hr);

    hr = WriteRecord (hash, record);
    CHR (hr);

    hasUsers = std::filesystem::exists (GetPath (hash, s_kByUser), error);

    if (!hasUsers)
    {
        hrStream = WriteStream (image, png);
        IGNORE_RETURN_VALUE (hrStream, S_OK);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailStore::SetByUser
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThumbnailStore::SetByUser (const std::filesystem::path & image, std::span<const Byte> png)
{
    HRESULT      hr       = S_OK;
    HRESULT      hrStream = S_OK;
    std::string  hash;



    hr = GetHash (image, hash);
    CHR (hr);

    hr = WriteFile (GetPath (hash, s_kByUser), png);
    CHR (hr);

    hrStream = WriteStream (image, png);
    IGNORE_RETURN_VALUE (hrStream, S_OK);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailStore::Remove
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThumbnailStore::Remove (const std::filesystem::path & image)
{
    HRESULT            hr       = S_OK;
    HRESULT            hrBooted = S_OK;
    HRESULT            hrStream = S_OK;
    std::string        hash;
    std::error_code    error;
    std::vector<Byte>  booted;



    hr = GetHash (image, hash);
    CHR (hr);

    std::filesystem::remove (GetPath (hash, s_kByUser), error);

    hrBooted = ReadFile (GetPath (hash, s_kBooted), booted);

    if (SUCCEEDED (hrBooted))
    {
        hrStream = WriteStream (image, booted);
        IGNORE_RETURN_VALUE (hrStream, S_OK);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailStore::RemoveForDeleted
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThumbnailStore::RemoveForDeleted (const std::string & hash)
{
    std::error_code  error;



    for (const wchar_t * suffix : { s_kBooted, s_kByUser, s_kRecord })
    {
        std::filesystem::remove (GetPath (hash, suffix), error);
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailStore::CountUserSet
//
////////////////////////////////////////////////////////////////////////////////

int ThumbnailStore::CountUserSet() const
{
    std::error_code  error;
    int              count = 0;



    for (const std::filesystem::directory_entry & entry : std::filesystem::directory_iterator (m_folder, error))
    {
        count += entry.path().filename().wstring().ends_with (s_kByUser) ? 1 : 0;
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailStore::Clear
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThumbnailStore::Clear (bool includeUserSet, int & outUserSet)
{
    std::error_code                     error;
    std::vector<std::filesystem::path>  doomed;



    outUserSet = CountUserSet();

    for (const std::filesystem::directory_entry & entry : std::filesystem::directory_iterator (m_folder, error))
    {
        std::wstring  file     = entry.path().filename().wstring();
        bool          isByUser = file.ends_with (s_kByUser);
        bool          isOurs   = isByUser || file.ends_with (s_kBooted) || file.ends_with (s_kRecord);

        if (isOurs && (includeUserSet || !isByUser))
        {
            doomed.push_back (entry.path());
        }
    }

    for (const std::filesystem::path & path : doomed)
    {
        std::filesystem::remove (path, error);
    }

    return S_OK;
}
