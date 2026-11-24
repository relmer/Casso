#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/IImageRecords.h"
#include "CassoExplorer/Model/ImageRecordCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordStore
//
//  Image records as JSON files in a cache folder. Each file's name is the
//  SHA-256 of the image file, and a copy is in the image file's alternate
//  data stream. The stream goes with the file through a move or a copy on
//  an NTFS volume; the cache serves an image on a FAT volume, a read-only
//  one, and a copy of its bytes alone. A stream's record has the hash it
//  was made for, and is used only while the file has that hash.
//
//  Only an image up to kMaxImageBytes has a record, since its whole file is
//  read for the hash.
//
//  The hash of a file is stored with its path, size and last write time,
//  so a file with the same three isn't read again. Safe to call from any
//  thread.
//
////////////////////////////////////////////////////////////////////////////////

class ImageRecordStore : public IImageRecords
{
public:
    static constexpr const wchar_t *  kStreamName   = L":casso.record.json";
    static constexpr uint64_t         kMaxImageBytes = 1024 * 1024;

    explicit ImageRecordStore (std::filesystem::path folder);

    //  The folder under the user's local application data.
    static std::filesystem::path  GetDefaultFolder ();

    bool       FindContents   (const std::wstring & path, ImageContents & outContents) override;
    void       StoreContents  (const std::wstring & path, const ImageContents & contents) override;
    SavedGame  FindSavedGame  (const std::wstring & path) override;
    void       StoreSavedGame (const std::wstring & path, bool hasSavedGame) override;

private:
    using Record = ImageRecordCodec::Record;

    struct Indexed
    {
        uint64_t     size = 0;
        int64_t      time = 0;
        std::string  hash;
    };

    HRESULT                GetHash      (const std::wstring & path, std::string & outHash);
    bool                   Load         (const std::wstring & path, Record & outRecord);
    void                   Save         (const std::wstring & path, const Record & record);
    std::filesystem::path  GetCachePath (const std::string & hash) const;
    static bool            ReadText     (const std::wstring & path, std::string & outText);
    static HRESULT         WriteText    (const std::wstring & path, const std::string & text);
    static HRESULT         WriteStream  (const std::wstring & image, const std::string & text);

    std::filesystem::path                 m_folder;
    std::mutex                            m_lock;
    std::map<std::wstring, Indexed>       m_index;
    std::map<std::string, Record>         m_records;
    std::map<std::wstring, std::string>   m_previous;   // the hash each path's record was last saved for
};
