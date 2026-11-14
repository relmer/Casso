#pragma once

#include "Pch.h"

#include "Core/JsonValue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailRecord
//
//  What is stored beside a disk image's thumbnail: its boot result as the
//  manifest has it, the machine it was booted on, the reason, and the title
//  found on its text screen.
//
////////////////////////////////////////////////////////////////////////////////

struct ThumbnailRecord
{
    std::string  verdict;
    std::string  machine;
    std::string  reason;
    std::string  title;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailStore
//
//  A disk image's thumbnail, found by the image's content and not by its
//  path: a PNG in a cache folder whose file name is the SHA-256 of the image
//  file, with a JSON record beside it. A thumbnail the user set is a second
//  PNG for that hash, and is the one returned while it exists.
//
//  A copy of the thumbnail returned is in the image file's alternate data
//  stream, and its record in a second stream, with whether the user set it
//  and the cache's clear count when it was written. The streams go with the
//  file through a move or a copy on an NTFS volume, and through a write that
//  replaces the file, which changes its hash. A copy of a file's bytes alone,
//  a zip and a FAT volume have no streams; Find writes them again from the
//  cache, and takes streams the cache lacks into the cache, unless they were
//  written before the cache was last cleared. The file's last write time is
//  restored after a write to the streams.
//
//  The hash of a file is stored with its path, size and last write time,
//  so a file with the same three isn't read again.
//
////////////////////////////////////////////////////////////////////////////////

class ThumbnailStore
{
public:
    static constexpr const wchar_t *  kStreamName       = L":casso.thumbnail";
    static constexpr const wchar_t *  kRecordStreamName = L":casso.thumbnail.json";

    explicit ThumbnailStore (std::filesystem::path folder);

    //  The folder under the user's local application data.
    static std::filesystem::path  GetDefaultFolder ();

    //  The SHA-256 of the image file, as lowercase hex.
    HRESULT  GetHash          (const std::filesystem::path & image, std::string & outHash);

    //  outPng is empty with no thumbnail in the cache or the stream.
    HRESULT  Find             (const std::filesystem::path & image, std::vector<Byte> & outPng, ThumbnailRecord & outRecord, bool & outIsUserSet);

    //  The thumbnail made by booting, and its record.
    HRESULT  Store            (const std::filesystem::path & image, std::span<const Byte> png, const ThumbnailRecord & record);

    //  The thumbnail the user set, returned in place of the booted one.
    HRESULT  SetByUser        (const std::filesystem::path & image, std::span<const Byte> png);

    //  Removes the thumbnail the user set; the booted one is returned again.
    HRESULT  Remove           (const std::filesystem::path & image);

    //  Everything for a hash, for an image that was deleted.
    HRESULT  RemoveForDeleted (const std::string & hash);

    //  Every booted thumbnail and record, and with includeUserSet those the
    //  user set too. outUserSet is the count of those the user set.
    HRESULT  Clear            (bool includeUserSet, int & outUserSet);

    int      CountUserSet     () const;

    //  Whether a change to a file of the cache folder is to a thumbnail the
    //  user set, or a clear, which another process can make: a booted
    //  thumbnail is written only by the process that has it in view.
    static bool     IsUserChange (const std::filesystem::path & file);

    //  The streams of one file written to another, for a write that replaces
    //  an image with a temporary file. A file with no stream isn't an error.
    static HRESULT  CopyStream (const std::filesystem::path & from, const std::filesystem::path & to);

private:
    struct Indexed
    {
        uint64_t     size = 0;
        int64_t      time = 0;
        std::string  hash;
    };

    using Fields = std::vector<std::pair<std::string, JsonValue>>;

    std::filesystem::path  GetPath            (const std::string & hash, const wchar_t * suffix) const;
    int                    GetClears          () const;
    std::string            FormatStreamRecord (const ThumbnailRecord & record, bool isUserSet) const;
    static HRESULT         ReadFile           (const std::filesystem::path & path, std::vector<Byte> & outBytes);
    static HRESULT         WriteFile          (const std::filesystem::path & path, std::span<const Byte> bytes);
    static HRESULT         WriteStreams       (const std::filesystem::path & image, std::span<const Byte> png, const std::string & record);
    static HRESULT         ParseRecord        (std::span<const Byte> bytes, ThumbnailRecord & outRecord, bool & outIsUserSet, int & outClears);
    static Fields          GetRecordFields    (const ThumbnailRecord & record);
    static bool            IsHashName         (const std::wstring & file);
    HRESULT                ReadRecord         (const std::string & hash, ThumbnailRecord & outRecord) const;
    HRESULT                WriteRecord        (const std::string & hash, const ThumbnailRecord & record) const;
    HRESULT                RefreshStreams     (const std::filesystem::path & image, std::span<const Byte> png, const ThumbnailRecord & record, bool isUserSet) const;

    static constexpr const wchar_t *  s_kBooted    = L".png";
    static constexpr const wchar_t *  s_kByUser    = L".user.png";
    static constexpr const wchar_t *  s_kRecord    = L".json";
    static constexpr const wchar_t *  s_kClears    = L"clears.txt";
    static constexpr size_t           s_kHashChars = 64;

    std::filesystem::path                m_folder;
    mutable std::mutex                   m_lock;
    std::map<std::wstring, Indexed>      m_index;
};
