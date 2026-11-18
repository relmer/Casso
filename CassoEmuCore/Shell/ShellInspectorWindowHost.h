#pragma once

#include "Pch.h"

#include "Seams/Win32DiskFileIo.h"
#include "Shell/ShellInspectorHost.h"
#include "Ui/DiskInspector/IDiskInspectorHost.h"

class ExplorerInspectorHost;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorWindowHost
//
//  One Casso disk inspector window's host. The window inspects one disk:
//  the disk in its drive when the window opened, or the first disk put in
//  that drive when it was empty. While the disk is in one of slot 6's
//  drives, requests go to the emulation thread through the shared
//  ShellInspectorHost. After the disk is ejected, it is read from its
//  file, as Casso Explorer reads an image, so the unsaved edits stay and
//  Save writes the file. Putting the file in a drive again makes the
//  window's disk live again.
//
//  The window's request ids are this host's, so the replies from the
//  emulation thread and from the file have ids that never collide.
//
////////////////////////////////////////////////////////////////////////////////

class ShellInspectorWindowHost : public IDiskInspectorHost
{
public:
    //  The path is the disk in the drive, or empty for an empty drive.
    //  findDrive returns the slot 6 drive a file is mounted in, or -1.
    //  machineName returns the name of the machine Casso is running.
    ShellInspectorWindowHost (ShellInspectorHost & shared, int drive, const std::wstring & path, std::function<int (const std::wstring &)> findDrive, std::function<std::wstring ()> machineName);
    ~ShellInspectorWindowHost () override;

    uint64_t  PostInspectorRequest (const InspectorRequest & request)   override;
    void      TakeInspectorReplies (vector<InspectorReply> & outReplies) override;
    int       GetDriveCount        () const                               override { return IsInDrive() ? ShellInspectorHost::kDriveCount : 0; }
    void      OnInspectorClosed    ()                                     override;

    std::shared_ptr<const DriveHeadState>  GetHeadState       (int drive) const override;
    void                                   SetHeadStateWanted (bool isWanted)  override;

    bool          IsDriveAttached        (int drive) const override { return !IsInDrive() || m_shared.IsDriveAttached (drive); }
    bool          HasOtherDiskController () const          override { return m_shared.HasOtherDiskController(); }
    int           GetWindowDrive         () const          override { return IsInDrive() ? m_drive : 0; }
    uint64_t      GetSourceGeneration    () const          override { return m_generation; }
    std::wstring  GetMountDescription    () const          override;

    void                  SetOnClosed (std::function<void ()> onClosed) { m_onClosed = std::move (onClosed); }
    const std::wstring  & GetPath     () const { return m_path; }
    int                   GetDrive    () const { return m_drive; }
    bool                  IsInDrive   () const { return m_drive >= 0; }

private:
    //  A request sent to the emulation thread, by the id it got there.
    struct SentRequest
    {
        uint64_t          id = 0;
        InspectorRequest  request;
    };

    void  FollowMount   ();
    void  UseFile       ();
    bool  IsDiskGone    (const InspectorReply & reply, const InspectorRequest & request) const;
    bool  IsOwnPath     (const std::string & utf8Path) const;
    void  PostToFile    (uint64_t id, const InspectorRequest & request);
    void  TakeFromShell (vector<InspectorReply> & outReplies);
    void  TakeFromFile  (vector<InspectorReply> & outReplies);

    ShellInspectorHost                         & m_shared;
    std::function<int (const std::wstring &)>    m_findDrive;
    std::function<std::wstring ()>               m_machineName;
    std::function<void ()>                       m_onClosed;
    Win32DiskFileIo                              m_fileIo;
    std::unique_ptr<ExplorerInspectorHost>       m_file;
    std::map<uint64_t, SentRequest>              m_toShell;
    std::map<uint64_t, uint64_t>                 m_toFile;
    std::wstring                                 m_path;
    int                                          m_drive        = -1;
    int                                          m_leftDrive    = -1;
    uint64_t                                     m_nextId       = 0;
    uint64_t                                     m_generation   = 0;
    bool                                         m_isHeadWanted = false;
};
