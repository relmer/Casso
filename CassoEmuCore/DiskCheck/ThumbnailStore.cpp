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
//  ThumbnailStore::WriteStreams
//
//  The PNG and its record into the image file's streams, and the file's
//  times as they were: a write to any stream of a file sets its last write
//  time, by which a backup or a sync would take the image for a changed one.
//  An empty PNG deletes both streams.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThumbnailStore::WriteStreams (const std::filesystem::path & image, std::span<const Byte> png, const std::string & record)
{
    HRESULT                    hr       = S_OK;
    WIN32_FILE_ATTRIBUTE_DATA  before   = {};
    BOOL                       hasTimes = ::GetFileAttributesExW (image.c_str(), GetFileExInfoStandard, &before);
    std::wstring               picture  = image.wstring() + kStreamName;
    std::wstring               fields   = image.wstring() + kRecordStreamName;
    HANDLE                     whole    = INVALID_HANDLE_VALUE;



    CBREx (hasTimes, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    if (png.empty())
    {
        ::DeleteFileW (picture.c_str());
        ::DeleteFileW (fields.c_str());
        BAIL_OUT_IF (true, S_OK);
    }

    for (const auto & [name, bytes] : { std::pair (picture, png), std::pair (fields, std::span<const Byte> ((const Byte *) record.data(), record.size())) })
    {
        HANDLE  file    = ::CreateFileW (name.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        DWORD   written = 0;
        BOOL    isDone  = FALSE;

        CBREx (file != INVALID_HANDLE_VALUE, HRESULT_FROM_WIN32 (::GetLastError()));

        isDone = ::WriteFile (file, bytes.data(), (DWORD) bytes.size(), &written, nullptr) && written == (DWORD) bytes.size();
        ::CloseHandle (file);

        CBREx (isDone, HRESULT_FROM_WIN32 (ERROR_WRITE_FAULT));
    }

Error:
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
//  streams with the temporary's, so the thumbnail and its record are copied
//  to it first. The image's hash changes with the write; Find takes the
//  streams into the cache for the new hash, a thumbnail the user set as one
//  the user set.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThumbnailStore::CopyStream (const std::filesystem::path & from, const std::filesystem::path & to)
{
    HRESULT            hr        = S_OK;
    HRESULT            hrRead    = S_OK;
    std::vector<Byte>  png;
    std::vector<Byte>  record;
    bool               hasStream = false;



    hrRead    = ReadFile (std::filesystem::path (from.wstring() + kStreamName), png);
    hasStream = SUCCEEDED (hrRead) && !png.empty();
    BAIL_OUT_IF (!hasStream, S_OK);

    hrRead = ReadFile (std::filesystem::path (from.wstring() + kRecordStreamName), record);
    IGNORE_RETURN_VALUE (hrRead, S_OK);

    hr = WriteStreams (to, png, std::string (record.begin(), record.end()));
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
//  ThumbnailStore::GetRecordFields
//
////////////////////////////////////////////////////////////////////////////////

ThumbnailStore::Fields ThumbnailStore::GetRecordFields (const ThumbnailRecord & record)
{
    Fields  fields;



    fields.emplace_back ("verdict", JsonValue (record.verdict));
    fields.emplace_back ("machine", JsonValue (record.machine));
    fields.emplace_back ("reason",  JsonValue (record.reason));
    fields.emplace_back ("title",   JsonValue (record.title));

    return fields;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailStore::ParseRecord
//
//  A record from the cache has neither of the stream's fields, and is not
//  the user's, with a clear count of 0.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThumbnailStore::ParseRecord (std::span<const Byte> bytes, ThumbnailRecord & outRecord, bool & outIsUserSet, int & outClears)
{
    HRESULT         hr     = S_OK;
    JsonValue       root;
    JsonParseError  parseError;
    double          clears = 0;



    outRecord    = ThumbnailRecord();
    outIsUserSet = false;
    outClears    = 0;

    hr = JsonParser::Parse (std::string (bytes.begin(), bytes.end()), root, parseError);
    CHR (hr);

    root.HasString ("verdict", outRecord.verdict);
    root.HasString ("machine", outRecord.machine);
    root.HasString ("reason",  outRecord.reason);
    root.HasString ("title",   outRecord.title);
    root.HasBool   ("userSet", outIsUserSet);

    if (root.HasNumber ("clears", clears))
    {
        outClears = (int) clears;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailStore::FormatStreamRecord
//
////////////////////////////////////////////////////////////////////////////////

std::string ThumbnailStore::FormatStreamRecord (const ThumbnailRecord & record, bool isUserSet) const
{
    HRESULT              hr     = S_OK;
    Fields               fields = GetRecordFields (record);
    JsonWriter::Options  options;
    std::string          text;



    fields.emplace_back ("userSet", JsonValue (isUserSet));
    fields.emplace_back ("clears",  JsonValue ((double) GetClears()));

    hr = JsonWriter::Write (JsonValue (std::move (fields)), options, text);
    IGNORE_RETURN_VALUE (hr, S_OK);

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailStore::GetClears
//
//  The count of clears of the cache, in a separate file in the cache's
//  folder, so every process that uses the cache reads the same count.
//
////////////////////////////////////////////////////////////////////////////////

int ThumbnailStore::GetClears() const
{
    std::vector<Byte>  bytes;
    HRESULT            hr     = ReadFile (m_folder / s_kClears, bytes);
    int                clears = 0;



    if (SUCCEEDED (hr))
    {
        clears = atoi (std::string (bytes.begin(), bytes.end()).c_str());
    }

    return clears;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailStore::IsHashName
//
//  A cache file: an image's hash, then a suffix.
//
////////////////////////////////////////////////////////////////////////////////

bool ThumbnailStore::IsHashName (const std::wstring & file)
{
    return file.size() > s_kHashChars && std::all_of (file.begin(), file.begin() + s_kHashChars, [] (wchar_t ch) { return iswxdigit (ch) != 0; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailStore::IsUserChange
//
////////////////////////////////////////////////////////////////////////////////

bool ThumbnailStore::IsUserChange (const std::filesystem::path & file)
{
    std::wstring  name = file.filename().wstring();



    return (IsHashName (name) && name.ends_with (s_kByUser)) || name == s_kClears;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailStore::RefreshStreams
//
//  The streams written again when they don't have the thumbnail returned,
//  its record and the cache's clear count. A failure to write them, as on a
//  FAT volume or a read-only file, isn't an error.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThumbnailStore::RefreshStreams (const std::filesystem::path & image, std::span<const Byte> png, const ThumbnailRecord & record, bool isUserSet) const
{
    HRESULT            hrPng    = S_OK;
    HRESULT            hrFields = S_OK;
    HRESULT            hrWrite  = S_OK;
    std::vector<Byte>  stream;
    std::vector<Byte>  fields;
    std::string        wanted   = FormatStreamRecord (record, isUserSet);



    hrPng    = ReadFile (std::filesystem::path (image.wstring() + kStreamName), stream);
    hrFields = ReadFile (std::filesystem::path (image.wstring() + kRecordStreamName), fields);

    if (FAILED (hrPng) || FAILED (hrFields) || !std::equal (stream.begin(), stream.end(), png.begin(), png.end()) || std::string (fields.begin(), fields.end()) != wanted)
    {
        hrWrite = WriteStreams (image, png, wanted);
        IGNORE_RETURN_VALUE (hrWrite, S_OK);
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailStore::ReadRecord
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThumbnailStore::ReadRecord (const std::string & hash, ThumbnailRecord & outRecord) const
{
    HRESULT            hr        = S_OK;
    std::vector<Byte>  bytes;
    bool               isUserSet = false;
    int                clears    = 0;



    outRecord = ThumbnailRecord();

    hr = ReadFile (GetPath (hash, s_kRecord), bytes);
    CHR (hr);

    hr = ParseRecord (bytes, outRecord, isUserSet, clears);
    CHR (hr);

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
    HRESULT              hr = S_OK;
    JsonWriter::Options  options;
    std::string          text;



    hr = JsonWriter::Write (JsonValue (GetRecordFields (record)), options, text);
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
//  The thumbnail the user set, or the booted one, or the streams' when the
//  cache has neither, which are then taken into the cache. Streams that
//  don't have the thumbnail returned are written again.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThumbnailStore::Find (const std::filesystem::path & image, std::vector<Byte> & outPng, ThumbnailRecord & outRecord, bool & outIsUserSet)
{
    HRESULT            hr           = S_OK;
    HRESULT            hrRead       = S_OK;
    HRESULT            hrStream     = S_OK;
    std::string        hash;
    std::vector<Byte>  stream;
    std::vector<Byte>  fields;
    ThumbnailRecord    streamRecord;
    bool               streamIsUser = false;
    int                streamClears = 0;
    bool               isTaken      = false;



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

    //  The streams the cache lacks, unless they are from before a clear:
    //  streams written with no record stream are treated as from before one.
    if (FAILED (hrRead))
    {
        hrStream = ReadFile (std::filesystem::path (image.wstring() + kStreamName), stream);

        if (SUCCEEDED (hrStream) && !stream.empty())
        {
            hrStream = ReadFile (std::filesystem::path (image.wstring() + kRecordStreamName), fields);
            hrStream = SUCCEEDED (hrStream) ? ParseRecord (fields, streamRecord, streamIsUser, streamClears) : hrStream;
            isTaken  = SUCCEEDED (hrStream) ? streamClears >= GetClears() : GetClears() == 0;
        }
    }

    if (isTaken)
    {
        outPng       = stream;
        outIsUserSet = streamIsUser;

        hrRead = WriteFile (GetPath (hash, streamIsUser ? s_kByUser : s_kBooted), outPng);
        IGNORE_RETURN_VALUE (hrRead, S_OK);

        if (SUCCEEDED (hrStream))
        {
            hrRead = WriteRecord (hash, streamRecord);
            IGNORE_RETURN_VALUE (hrRead, S_OK);
        }
    }

    BAIL_OUT_IF (outPng.empty(), S_OK);

    hrRead = ReadRecord (hash, outRecord);
    IGNORE_RETURN_VALUE (hrRead, S_OK);

    hr = RefreshStreams (image, outPng, outRecord, outIsUserSet);
    CHR (hr);

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
    HRESULT            hr       = S_OK;
    HRESULT            hrUser   = S_OK;
    std::string        hash;
    std::vector<Byte>  byUser;



    hr = GetHash (image, hash);
    CHR (hr);

    hr = WriteFile (GetPath (hash, s_kBooted), png);
    CHR (hr);

    hr = WriteRecord (hash, record);
    CHR (hr);

    hrUser = ReadFile (GetPath (hash, s_kByUser), byUser);

    hr = SUCCEEDED (hrUser) ? RefreshStreams (image, byUser, record, true) : RefreshStreams (image, png, record, false);
    CHR (hr);

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
    HRESULT          hr       = S_OK;
    HRESULT          hrRecord = S_OK;
    std::string      hash;
    ThumbnailRecord  record;



    hr = GetHash (image, hash);
    CHR (hr);

    hr = WriteFile (GetPath (hash, s_kByUser), png);
    CHR (hr);

    hrRecord = ReadRecord (hash, record);
    IGNORE_RETURN_VALUE (hrRecord, S_OK);

    hr = RefreshStreams (image, png, record, true);
    CHR (hr);

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
    ThumbnailRecord    record;



    hr = GetHash (image, hash);
    CHR (hr);

    std::filesystem::remove (GetPath (hash, s_kByUser), error);

    hrBooted = ReadFile (GetPath (hash, s_kBooted), booted);

    //  With no booted thumbnail, the streams are deleted, so the one the user
    //  set isn't taken from them again.
    if (SUCCEEDED (hrBooted))
    {
        hrStream = ReadRecord (hash, record);
        IGNORE_RETURN_VALUE (hrStream, S_OK);

        hr = RefreshStreams (image, booted, record, false);
        CHR (hr);
    }
    else
    {
        hrStream = WriteStreams (image, {}, std::string());
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
        std::wstring  file = entry.path().filename().wstring();

        count += (IsHashName (file) && file.ends_with (s_kByUser)) ? 1 : 0;
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
    HRESULT                             hr     = S_OK;
    std::error_code                     error;
    std::vector<std::filesystem::path>  doomed;
    std::string                         clears = std::to_string (GetClears() + 1);



    outUserSet = CountUserSet();

    for (const std::filesystem::directory_entry & entry : std::filesystem::directory_iterator (m_folder, error))
    {
        std::wstring  file     = entry.path().filename().wstring();
        bool          isByUser = file.ends_with (s_kByUser);
        bool          isOurs   = IsHashName (file) && (isByUser || file.ends_with (s_kBooted) || file.ends_with (s_kRecord));

        if (isOurs && (includeUserSet || !isByUser))
        {
            doomed.push_back (entry.path());
        }
    }

    for (const std::filesystem::path & path : doomed)
    {
        std::filesystem::remove (path, error);
    }

    //  An image's streams from before this are no longer taken into the cache.
    hr = WriteFile (m_folder / s_kClears, std::span<const Byte> ((const Byte *) clears.data(), clears.size()));
    CHR (hr);

Error:
    return hr;
}
