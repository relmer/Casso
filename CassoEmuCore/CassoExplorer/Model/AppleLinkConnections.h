#pragma once

#include "Pch.h"

#include "AppleLink/AppleLinkSession.h"
#include "AppleLink/AppleLinkSettings.h"
#include "CassoExplorer/Model/TreeModel.h"

class IAppleLinkFolder;
class IByteStreamFactory;
class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections
//
//  Casso Explorer's Apple links: a session for each link the user has
//  started, and what they returned. A request for a drive's listing or a
//  file queues the operation and returns at once; when the session finishes
//  it, a message is posted to the window, and the same request then returns
//  the result. Nothing here waits on a line.
//
//  A link's port is open only while it is started (FR-097): the first
//  request starts it, and Stop, or the link's removal, closes it.
//
////////////////////////////////////////////////////////////////////////////////

class AppleLinkConnections
{
public:
    AppleLinkConnections (IByteStreamFactory & factory, IAppleLinkFolder & folder, IDiskFileIo & fileIo);
    ~AppleLinkConnections ();

    //  Where a finished operation or a change of state is posted.
    void  SetTarget (HWND hwnd, UINT message) { m_hwnd = hwnd; m_message = message; }

    //  The links, as the preferences have them. A link removed or changed is
    //  stopped.
    void                                    SetLinks (const std::vector<AppleLinkSettings> & links);
    const std::vector<AppleLinkSettings> &  GetLinks () const { return m_links; }

    HRESULT  Start     (const std::string & name);
    void     Stop      (const std::string & name);
    bool     IsStarted (const std::string & name) const;

    //  The session of a started link, or null.
    AppleLinkSession *  GetSession (const std::string & name);

    //  The tree's entries for the links: a started link's drives, and for
    //  any other its state and the reason.
    std::vector<LinkTreeEntry>  BuildTreeEntries () const;

    //  A directory's listing on a drive, or a file's contents, once read:
    //  false while it is being read, starting the read, and the link with it,
    //  on the first request. A failed read arrives as its result
    //  and message.
    bool  TryGetListing (const std::string & name, int drive, const std::string & directory,
                         VolumeListing & outListing, VolumeKind & outKind, HRESULT & outResult, std::string & outMessage);
    bool  TryGetFile    (const std::string & name, int drive, const std::string & path,
                         FilePayload & outPayload, HRESULT & outResult, std::string & outMessage);

    //  Drops what was read from a drive, so it is read again.
    void  Invalidate (const std::string & name, int drive);

    //  Whether anything arrived since the last call.
    bool  TakeChanged ();

    //  The text under a link's root in place of its drives.
    static std::wstring  DescribeState (LinkState state, bool servingAdtPro);

private:
    class Sink : public IAppleLinkSessionSink
    {
    public:
        explicit Sink (AppleLinkConnections & owner) : m_owner (owner) {}

        void  OnStateChanged (LinkState state) override;
        void  OnProgress     (const std::shared_ptr<LinkOperation> & operation) override;
        void  OnDone         (const std::shared_ptr<LinkOperation> & operation) override;

    private:
        AppleLinkConnections  & m_owner;
    };

    struct Link
    {
        AppleLinkSettings                                          settings;
        std::unique_ptr<Sink>                                      sink;
        std::unique_ptr<AppleLinkSession>                          session;
        std::map<std::string, std::shared_ptr<LinkOperation>>      requests;
    };

    static std::string  MakeKey (int drive, const std::string & path, bool file);

    Link *        FindLink (const std::string & name);
    const Link *  FindLink (const std::string & name) const;
    bool          TryGetResult (const std::string & name, int drive, const std::string & key, LinkOperationKind kind,
                                const std::string & path, std::shared_ptr<LinkOperation> & outOperation);
    void          Notify ();

    IByteStreamFactory                 & m_factory;
    IAppleLinkFolder                   & m_folder;
    IDiskFileIo                        & m_fileIo;
    std::vector<AppleLinkSettings>       m_links;
    std::vector<std::unique_ptr<Link>>   m_started;
    mutable std::mutex                   m_lock;
    std::atomic<bool>                    m_changed { false };
    HWND                                 m_hwnd    = nullptr;
    UINT                                 m_message = 0;
};
