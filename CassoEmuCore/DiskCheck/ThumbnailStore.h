#pragma once

#include "Pch.h"





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
//  stream, so it goes with the file through a move or a copy on an NTFS
//  volume. A copy of a file's bytes alone, a zip and a FAT volume have no
//  stream; Find writes it again from the cache, and takes a stream the
//  cache lacks into the cache. The file's last write
//  time is restored after a write to the stream.
//
//  The hash of a file is stored with its path, size and last write time,
//  so a file with the same three isn't read again.
//
////////////////////////////////////////////////////////////////////////////////

class ThumbnailStore
{
public:
    static constexpr const wchar_t *  kStreamName = L":casso.thumbnail";

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

private:
    struct Indexed
    {
        uint64_t     size = 0;
        int64_t      time = 0;
        std::string  hash;
    };

    std::filesystem::path  GetPath     (const std::string & hash, const wchar_t * suffix) const;
    static HRESULT         ReadFile    (const std::filesystem::path & path, std::vector<Byte> & outBytes);
    static HRESULT         WriteFile   (const std::filesystem::path & path, std::span<const Byte> bytes);
    static HRESULT         WriteStream (const std::filesystem::path & image, std::span<const Byte> png);
    HRESULT                ReadRecord  (const std::string & hash, ThumbnailRecord & outRecord) const;
    HRESULT                WriteRecord (const std::string & hash, const ThumbnailRecord & record) const;

    static constexpr const wchar_t *  s_kBooted = L".png";
    static constexpr const wchar_t *  s_kByUser = L".user.png";
    static constexpr const wchar_t *  s_kRecord = L".json";

    std::filesystem::path                m_folder;
    mutable std::mutex                   m_lock;
    std::map<std::wstring, Indexed>      m_index;
};
