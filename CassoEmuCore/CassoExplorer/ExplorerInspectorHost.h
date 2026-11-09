#pragma once

#include "Pch.h"

#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/DiskImageSession.h"
#include "Shell/InspectorRequestQueue.h"
#include "Ui/DiskInspector/IDiskInspectorHost.h"

class IDiskFileIo;
class IIntentChannel;





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
//  no machine, so every moment is a safe point for an apply.
//
//  An applied edit, undo or redo is saved the way Explorer commits its
//  other changes to an image: only when the file has not changed since it
//  was read, by replacing the file whole and durably, and then by telling a
//  running Casso, with the reload intent, that the file changed (FR-105).
//  Casso's reply arrives at the window, and any reply other than a reload
//  becomes a note.
//
//  The bytes of the first successful read are the comparison source "As
//  first opened" (FR-117), whatever the window saves or reads afterward.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerInspectorHost : public IDiskInspectorHost,
                              private IInspectorDiskSource
{
public:
    ExplorerInspectorHost (const std::wstring & path, IDiskFileIo & fileIo, IIntentChannel * intents = nullptr);

    uint64_t  PostInspectorRequest (const InspectorRequest & request)   override;
    void      TakeInspectorReplies (vector<InspectorReply> & outReplies) override;
    int       GetDriveCount        () const                               override { return 0; }
    void      OnInspectorClosed    ()                                     override { if (m_onClosed) { m_onClosed(); } }

    std::shared_ptr<const DriveHeadState>  GetHeadState       (int drive) const override { (void) drive; return nullptr; }
    void                                   SetHeadStateWanted (bool isWanted)  override { (void) isWanted; }

    bool          HandleCopyData (const COPYDATASTRUCT & data) override;
    std::wstring  TakeHostNote   ()                            override;

    void                  SetOnClosed (std::function<void ()> onClosed) { m_onClosed = std::move (onClosed); }
    void                  SetIntents  (IIntentChannel * intents)        { m_intents = intents; }
    const std::wstring  & GetPath     () const { return m_path; }

    //  The file is read again for the next copy, as a new disk.
    void  Reload ();

    //  Whether the file's size and time are as the window last read or
    //  saved it, so a change the window made itself is not read again.
    bool  IsFileCurrent ();

    //  The note for a reply from Casso, or empty for a reload (FR-105).
    static std::wstring  DescribeCassoReply (const Byte * bytes, size_t byteCount);

private:
    const DiskImage     * GetImage        (int drive, InspectorDiskIdentity & outIdentity) override;
    DiskImage           * GetImageForEdit (int drive) override;
    InspectorSaveResult   SaveDrive       (int drive) override;
    vector<std::string>   GetMountedPaths ()          override { return { m_utf8Path }; }
    std::wstring          GetOpenError    (int drive) override;

    std::shared_ptr<const vector<Byte>>  GetAsInsertedBytes (int drive) override { (void) drive; return m_firstBytes; }

    void  Load ();

    std::wstring                   m_path;
    std::string                    m_utf8Path;
    IDiskFileIo                  & m_fileIo;
    IIntentChannel               * m_intents   = nullptr;
    uint64_t                       m_mediaId   = 0;
    InspectorRequestQueue          m_queue;
    std::unique_ptr<DiskImage>     m_image;
    InspectorDiskIdentity          m_identity;
    DiskImageSession::OpenedImage  m_opened;

    //  The file as the window first read it, for "As first opened" (FR-117).
    std::shared_ptr<const vector<Byte>>  m_firstBytes;
    std::wstring                         m_openError;
    std::wstring                         m_hostNote;
    bool                                 m_isRead     = false;
    std::function<void ()>         m_onClosed;
};
