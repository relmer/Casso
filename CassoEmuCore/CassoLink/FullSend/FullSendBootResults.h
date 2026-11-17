#pragma once

#include "Pch.h"

class FullSendManifest;
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
    const std::vector<std::string>  & GetLines       () const                          { return m_lines; }
    void                              ClearLines     ()                                { m_lines.clear(); }
    std::string                       TakeRetryPrint ();
    void                              NoteRetried    (const std::string & fingerprint) { m_retried.insert (fingerprint); }

private:
    FullSendStatus            & m_status;
    FullSendManifest          & m_manifest;
    std::string                 m_latest;       // the result Apply took last
    std::string                 m_latestCheck;  // and its file-system verdict
    std::vector<std::string>    m_lines;        // a line for each image that did not boot, for the next pause
    std::string                 m_retryPrint;   // the fingerprint of the disk the pause offers to read again
    std::set<std::string>       m_retried;      // the fingerprint of each disk read again
};
