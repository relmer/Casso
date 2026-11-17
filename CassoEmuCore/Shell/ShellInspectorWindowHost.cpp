#include "Pch.h"

#include "Shell/ShellInspectorWindowHost.h"
#include "CassoExplorer/ExplorerInspectorHost.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorWindowHost::ShellInspectorWindowHost
//
////////////////////////////////////////////////////////////////////////////////

ShellInspectorWindowHost::ShellInspectorWindowHost (ShellInspectorHost & shared, int drive, const std::wstring & path, std::function<int (const std::wstring &)> findDrive, std::function<std::wstring()> machineName) :
    m_shared      (shared),
    m_findDrive   (std::move (findDrive)),
    m_machineName (std::move (machineName)),
    m_path        (path),
    m_drive       (drive)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorWindowHost::~ShellInspectorWindowHost
//
////////////////////////////////////////////////////////////////////////////////

ShellInspectorWindowHost::~ShellInspectorWindowHost()
{
    m_shared.SetHeadStateWantedBy (this, false);
    m_shared.DropRepliesFor ([this] (uint64_t id) { return m_toShell.contains (id); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorWindowHost::PostInspectorRequest
//
////////////////////////////////////////////////////////////////////////////////

uint64_t ShellInspectorWindowHost::PostInspectorRequest (const InspectorRequest & request)
{
    uint64_t  id    = ++m_nextId;
    uint64_t  inner = 0;



    if (IsInDrive())
    {
        inner            = m_shared.PostInspectorRequest (request);
        m_toShell[inner] = { id, request };
    }
    else
    {
        PostToFile (id, request);
    }

    return id;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorWindowHost::TakeInspectorReplies
//
//  The mount is checked first, so a copy of a disk that is no longer in
//  its drive is read from the disk's file.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInspectorWindowHost::TakeInspectorReplies (vector<InspectorReply> & outReplies)
{
    outReplies.clear();

    FollowMount();
    TakeFromShell (outReplies);
    TakeFromFile  (outReplies);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorWindowHost::OnInspectorClosed
//
////////////////////////////////////////////////////////////////////////////////

void ShellInspectorWindowHost::OnInspectorClosed()
{
    m_shared.SetHeadStateWantedBy (this, false);
    m_shared.DropRepliesFor ([this] (uint64_t id) { return m_toShell.contains (id); });
    m_toShell.clear();

    if (m_onClosed)
    {
        m_onClosed();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorWindowHost::GetHeadState
//
//  A disk read from its file has no head.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DriveHeadState> ShellInspectorWindowHost::GetHeadState (int drive) const
{
    return IsInDrive() ? m_shared.GetHeadState (drive) : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorWindowHost::SetHeadStateWanted
//
////////////////////////////////////////////////////////////////////////////////

void ShellInspectorWindowHost::SetHeadStateWanted (bool isWanted)
{
    m_isHeadWanted = isWanted;
    m_shared.SetHeadStateWantedBy (this, isWanted);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorWindowHost::GetMountDescription
//
//  "Apple //c drive 1", or empty for a disk read from its file.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ShellInspectorWindowHost::GetMountDescription() const
{
    std::wstring  machine = m_machineName ? m_machineName() : std::wstring();



    return IsInDrive() ? std::format (L"{}{}drive {}", machine, machine.empty() ? L"" : L" ", m_drive + 1) : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorWindowHost::FollowMount
//
//  The disk is in whichever drive has its file mounted. After a reply
//  indicates that the disk is no longer in a drive, that drive is passed
//  over until its path changes, since the path is updated a frame or two
//  after the emulation thread ejects the disk.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInspectorWindowHost::FollowMount()
{
    int  drive = (m_path.empty() || !m_findDrive) ? m_drive : m_findDrive (m_path);



    if (!m_path.empty())
    {
        m_leftDrive = (drive == m_leftDrive) ? m_leftDrive : -1;
        drive       = (drive >= 0 && drive == m_leftDrive) ? -1 : drive;

        if (drive >= 0 && drive != m_drive)
        {
            m_drive = drive;
            m_shared.SetHeadStateWantedBy (this, m_isHeadWanted);
            m_file.reset();
            m_toFile.clear();
            m_generation++;
        }
        else if (drive < 0 && IsInDrive())
        {
            UseFile();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorWindowHost::UseFile
//
//  From here on the disk is read from its file. The emulation thread wrote
//  the drive's changes to the file before it ejected the disk.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInspectorWindowHost::UseFile()
{
    if (IsInDrive())
    {
        m_shared.SetHeadStateWantedBy (this, false);
        m_drive = -1;
        m_file  = std::make_unique<ExplorerInspectorHost> (m_path, m_fileIo);
        m_toFile.clear();
        m_generation++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorWindowHost::IsDiskGone
//
//  Whether a reply about the window's drive indicates that the window's
//  disk is no longer in it: the drive is empty, or it has another disk.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellInspectorWindowHost::IsDiskGone (const InspectorReply & reply, const InspectorRequest & request) const
{
    bool  isCopy  = request.kind == InspectorRequestKind::CopyDisk || request.kind == InspectorRequestKind::CopyTracks;
    bool  isOwn   = IsInDrive() && !m_path.empty() && request.drive == m_drive && isCopy;
    bool  isEmpty = reply.status == InspectorReplyStatus::NoDisk || reply.status == InspectorReplyStatus::DiskChanged ||
                    (request.kind == InspectorRequestKind::CopyDisk && reply.disk == nullptr && reply.status != InspectorReplyStatus::Unopenable);
    bool  isOther = reply.disk != nullptr && !IsOwnPath (reply.disk->fileName);



    return isOwn && (isEmpty || isOther);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorWindowHost::IsOwnPath
//
////////////////////////////////////////////////////////////////////////////////

bool ShellInspectorWindowHost::IsOwnPath (const std::string & utf8Path) const
{
    return _wcsicmp (TextEncoding::Utf8ToWide (utf8Path).c_str(), m_path.c_str()) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorWindowHost::PostToFile
//
//  The file is the only disk of its host, as drive 0.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInspectorWindowHost::PostToFile (uint64_t id, const InspectorRequest & request)
{
    InspectorRequest  toFile = request;



    if (m_file != nullptr)
    {
        toFile.drive                                    = 0;
        m_toFile[m_file->PostInspectorRequest (toFile)] = id;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorWindowHost::TakeFromShell
//
//  A window opened on an empty drive inspects the first disk put in it. A
//  copy requested from a drive the disk has left is requested from its file
//  instead.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInspectorWindowHost::TakeFromShell (vector<InspectorReply> & outReplies)
{
    vector<InspectorReply>  replies;
    SentRequest             sent;
    bool                    isGone = false;



    m_shared.TakeRepliesFor ([this] (uint64_t id) { return m_toShell.contains (id); }, replies);

    for (InspectorReply & reply : replies)
    {
        sent = m_toShell[reply.requestId];
        m_toShell.erase (reply.requestId);
        reply.requestId = sent.id;

        if (m_path.empty() && sent.request.kind == InspectorRequestKind::CopyDisk && sent.request.drive == m_drive && reply.disk != nullptr)
        {
            m_path = TextEncoding::Utf8ToWide (reply.disk->fileName);
        }

        isGone = IsDiskGone (reply, sent.request);

        if (isGone)
        {
            m_leftDrive = m_drive;
            UseFile();
        }

        if (isGone && sent.request.kind == InspectorRequestKind::CopyDisk)
        {
            PostToFile (sent.id, sent.request);
        }
        else
        {
            outReplies.push_back (std::move (reply));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorWindowHost::TakeFromFile
//
////////////////////////////////////////////////////////////////////////////////

void ShellInspectorWindowHost::TakeFromFile (vector<InspectorReply> & outReplies)
{
    vector<InspectorReply>  replies;



    if (m_file != nullptr)
    {
        m_file->TakeInspectorReplies (replies);

        for (InspectorReply & reply : replies)
        {
            if (auto found = m_toFile.find (reply.requestId); found != m_toFile.end())
            {
                reply.requestId = found->second;
                m_toFile.erase (found);
                outReplies.push_back (std::move (reply));
            }
        }
    }
}
