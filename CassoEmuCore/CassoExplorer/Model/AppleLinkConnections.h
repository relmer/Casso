#pragma once

#include "Pch.h"

#include "AppleLink/AppleLinkSession.h"
#include "AppleLink/AppleLinkSettings.h"
#include "AppleLink/RunningCassoRegistry.h"
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

    //  The running Cassos with a serial port on TCP, each an Emulated link
    //  with its machine and port as its name, never saved (FR-083b). One whose
    //  address a saved link has is left out, and one on port 1 is listed with
    //  the reason it cannot connect. A Casso that exits stops its link.
    void                                    SetRunning (const std::vector<RunningCasso> & running);

    //  Every link: the saved ones, then the running Cassos.
    const std::vector<AppleLinkSettings> &  GetLinks () const { return m_links; }

    //  Whether a link is a running Casso's rather than a saved one.
    bool                                    IsDiscovered (const std::string & name) const;

    //  A running Casso's root name: its machine and TCP port.
    static std::string                      MakeRunningName (const RunningCasso & running);

    //  A search starts a serial link by searching the COM ports for the
    //  Apple, as a link with no port set does (FR-083a).
    HRESULT  Start     (const std::string & name, bool search = false);

    //  Starts a link on an Apple with no link program, typing the loader in
    //  first (FR-092). The completion runs when the program is running or the
    //  Apple does not answer.
    HRESULT  Bootstrap (const std::string & name, std::function<std::wstring (const LinkOperation & operation)> completion);
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

    //  An operation queued on a link, the link started first if it was not,
    //  and what to do with its result on the window's thread. A completion
    //  returns the message for the user, or empty for none.
    using Queuer     = std::function<std::shared_ptr<LinkOperation> (AppleLinkSession & session)>;
    using Completion = std::function<std::wstring (const LinkOperation & operation)>;

    HRESULT  Submit (const std::string & name, const Queuer & queue, Completion completion);

    //  The completions of the operations that finished, run on the window's
    //  thread; their messages are returned.
    std::vector<std::wstring>  RunCompletions ();

    //  Operations submitted and not yet finished, in the order they run.
    std::vector<std::shared_ptr<LinkOperation>>  GetPendingJobs () const;

    //  A file's contents, waiting up to timeoutMs for them. Only for a drop,
    //  where the target requests the bytes and nothing else can run until
    //  they arrive.
    HRESULT  WaitForFile (const std::string & name, int drive, const std::string & path, DWORD timeoutMs, FilePayload & outPayload);

    //  The text for an operation that failed.
    static std::wstring  DescribeFailure (const LinkOperation & operation);

    //  The status bar's text for the pending operations: the first one's
    //  progress, and how many follow it. Empty for none.
    static std::wstring  DescribeJobs (const std::vector<std::shared_ptr<LinkOperation>> & jobs);

    //  Drops what was read from a drive, so it is read again.
    void  Invalidate (const std::string & name, int drive);

    //  Whether anything arrived since the last call.
    bool  TakeChanged ();

    //  The text under a link's root in place of its drives.
    static std::wstring  DescribeState (LinkState state, bool servingAdtPro, const AppleLinkSettings & settings, const std::wstring & pcName);

    //  A speed with its thousands separated, as 115,200.
    static std::wstring  FormatBaud    (DWORD baud);

    //  The command a Wi-Fi modem dials for a link: ATDT, this PC's name with
    //  .local, and the link's TCP port (FR-083c).
    static std::wstring  DescribeDial  (const std::wstring & pcName, const AppleLinkSettings & settings);

    //  The dial command itself, as the shim on a boot disk sends it to the
    //  modem; empty for any transport but the Wi-Fi modem.
    static std::string   MakeDialCommand (const std::wstring & pcName, const AppleLinkSettings & settings);

    //  This PC's DNS host name.
    static std::wstring  GetPcName ();

    static constexpr DWORD  kWaitPollMs = 20;

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
    HRESULT       StartLink (const std::string & name, bool search, std::shared_ptr<LinkOperation> * outBootstrap);
    void          Merge ();

    IByteStreamFactory                 & m_factory;
    IAppleLinkFolder                   & m_folder;
    IDiskFileIo                        & m_fileIo;
    std::vector<AppleLinkSettings>       m_saved;
    std::vector<RunningCasso>            m_running;
    std::vector<AppleLinkSettings>       m_links;
    std::map<std::string, std::wstring>  m_unusable;
    std::vector<std::unique_ptr<Link>>   m_started;

    struct Job
    {
        std::string                     link;
        std::shared_ptr<LinkOperation>  operation;
        Completion                      completion;
    };

    std::vector<Job>                     m_jobs;
    mutable std::mutex                   m_lock;
    std::atomic<bool>                    m_changed { false };
    HWND                                 m_hwnd    = nullptr;
    UINT                                 m_message = 0;
};
