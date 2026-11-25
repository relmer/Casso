#pragma once

#include "Pch.h"

class FullSendManifest;
struct FullSendRow;
class FullSendStatus;





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendBootResults
//
//  The boot checks' results as a session takes them: set in its manifest,
//  and an image that did not boot listed on the Apple at the next pause.
//
////////////////////////////////////////////////////////////////////////////////

class FullSendBootResults
{
public:
    FullSendBootResults (FullSendStatus & status, FullSendManifest & manifest);

    void                              Apply          ();
    void                              ClearLatest    ()                                { m_latest.clear(); m_latestCheck.clear(); }
    const std::string               & GetLatest      () const                          { return m_latest; }
    const std::string               & GetLatestCheck () const                          { return m_latestCheck; }
    std::vector<std::string>          GetLines       () const;
    void                              ClearLines     ()                                { m_failed.clear(); m_stillFailed.clear(); m_retryImage.clear(); m_retryBooted = false; }
    std::string                       TakeRetryPrint ();
    bool                              HasRetry       () const                          { return !m_retryImage.empty(); }
    int                               GetRetryDrive  () const                          { return m_retryDrive; }
    void                              NoteRetried    (const std::string & fingerprint) { m_retried.insert (fingerprint); }

private:
    static constexpr size_t  s_kListedImages = 4;    // the images a pause lists by number before "and N more"

    static std::string  ListImages      (const std::vector<std::string> & images);
    static bool         IsWorthRetrying (const FullSendRow & row);

    FullSendStatus            & m_status;
    FullSendManifest          & m_manifest;
    std::string                 m_latest;       // the result Apply took last
    std::string                 m_latestCheck;  // and its file-system verdict
    std::vector<std::string>    m_failed;       // each image that did not boot, not offered again, for the next pause
    std::vector<std::string>    m_stillFailed;  // and each image of a disk read again that did not boot either
    std::string                 m_retryImage;           // the image the next pause offers to read again
    std::string                 m_retryPrint;           // and its disk's fingerprint
    int                         m_retryDrive  = 0;      // and the drive it was read from
    bool                        m_retryBooted = false;  // and whether it booted, with a bad file system
    std::set<std::string>       m_retried;      // the fingerprint of each disk read again
};
