#pragma once

#include "Pch.h"

#include "Devices/Disk/DiskImage.h"
#include "Shell/InspectorRequestQueue.h"
#include "Ui/DiskInspector/IDiskInspectorHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInspectorHost
//
//  Casso Explorer's side of one disk inspector window: an image file in
//  place of a drive. The file is read the way a drive mounts it, on the
//  window's first request for a copy and again after Reload, always as the
//  same disk, as drive 0 of a host with no drives (FR-001). Requests are
//  drained on the UI thread as they are posted, and the replies are
//  collected on the window's next frame. There is no head, no motion and
//  no machine.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerInspectorHost : public IDiskInspectorHost,
                              private IInspectorDiskSource
{
public:
    explicit ExplorerInspectorHost (const std::wstring & path);

    uint64_t  PostInspectorRequest (const InspectorRequest & request)   override;
    void      TakeInspectorReplies (vector<InspectorReply> & outReplies) override;
    int       GetDriveCount        () const                               override { return 0; }
    void      OnInspectorClosed    ()                                     override { if (m_onClosed) { m_onClosed(); } }

    std::shared_ptr<const DriveHeadState>  GetHeadState       (int drive) const override { (void) drive; return nullptr; }
    void                                   SetHeadStateWanted (bool isWanted)  override { (void) isWanted; }

    void                  SetOnClosed (std::function<void ()> onClosed) { m_onClosed = std::move (onClosed); }
    const std::wstring  & GetPath     () const { return m_path; }

    //  The file is read again for the next copy, as a new disk.
    void  Reload ();

private:
    const DiskImage  * GetImage     (int drive, InspectorDiskIdentity & outIdentity) override;
    std::wstring       GetOpenError (int drive) override;

    void  Load ();

    std::wstring                m_path;
    std::string                 m_utf8Path;
    uint64_t                    m_mediaId   = 0;
    InspectorRequestQueue       m_queue;
    std::unique_ptr<DiskImage>  m_image;
    InspectorDiskIdentity       m_identity;
    std::wstring                m_openError;
    bool                        m_isRead    = false;
    std::function<void ()>      m_onClosed;
};
