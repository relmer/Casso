#pragma once

#include "Pch.h"

#include "CassoLink/FullSend/FullSendLink.h"
#include "CassoLink/FullSend/FullSendManifest.h"

class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendImage
//
//  A disk as a Full Send read it: its sectors and what the read found, and
//  its raw capture when it was captured raw.
//
////////////////////////////////////////////////////////////////////////////////

struct FullSendImage
{
    std::vector<Byte>                 sectors;
    std::vector<std::pair<int, int>>  unreadable;
    FullSendReadReport                report;
    std::vector<Byte>                 raw;                      // a raw capture's WOZ, or empty
    std::vector<Byte>                 reads;                    // the raw capture's reads, when they are saved
    std::string                       marks;                    // what the raw capture marked
    bool                              writeProtected = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendFolder
//
//  The folder a Full Send writes to: its manifest and its images.
//
////////////////////////////////////////////////////////////////////////////////

class FullSendFolder
{
public:
    using RowMatch = std::function<bool (const FullSendRow & row)>;
    using Renames  = std::vector<std::pair<std::string, std::string>>;

    static constexpr const char *  kRawExtension = ".woz";      // a raw capture's
    static constexpr const char *  kReadsSuffix  = ".reads";    // added to a raw capture's file for its reads

    FullSendFolder (IDiskFileIo & fileIo, std::string path, std::string extension);

    HRESULT                    Load            ();
    HRESULT                    Reload          ();
    HRESULT                    WriteManifest   ();
    HRESULT                    WriteImage      (const std::string & title, const FullSendImage & image, bool withRaw, std::string & outFile);
    HRESULT                    FindMovedImages (const RowMatch & matches, Renames & outRenames);
    const FullSendRow        * FindWithImage   (const RowMatch & matches);
    std::string                GetPath         (const std::string & file) const;
    std::mutex               & GetLock         () const { return GetLock (m_path); }
    const std::string        & GetExtension    () const { return m_extension; }
    FullSendManifest         & GetManifest     ()       { return m_manifest; }
    const FullSendManifest   & GetManifest     () const { return m_manifest; }
    bool                       IsDamaged       () const { return m_damaged; }

    static std::mutex        & GetLock         (const std::string & folder);

private:
    HRESULT                    ReadManifest    (const std::string & path);
    HRESULT                    BuildImageFile  (const FullSendImage & image, bool withRaw, const std::string & extension, std::vector<Byte> & outBytes);
    HRESULT                    WriteBeside     (const std::string & file, const FullSendImage & image, bool withRaw);
    HRESULT                    HashUnlisted    ();
    bool                       HasImage        (const FullSendRow & row);
    std::string                FindFreeName    (const std::string & title, const std::string & extension);

    IDiskFileIo                                     & m_fileIo;
    std::string                                       m_path;
    std::string                                       m_extension;
    FullSendManifest                                  m_manifest;
    uint64_t                                          m_commitTag    = 1;
    bool                                              m_damaged      = false;    // the manifest could not be read

    //  The folder's images that the manifest doesn't list, by file name and
    //  hash, read once the first time a row's image is missing.
    std::vector<std::pair<std::string, std::string>>  m_unlisted;
    bool                                              m_unlistedRead = false;
};
