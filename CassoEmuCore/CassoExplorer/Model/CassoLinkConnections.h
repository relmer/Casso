#pragma once

#include "Pch.h"

#include "CassoLink/CassoLinkSession.h"
#include "CassoLink/CassoLinkSettings.h"
#include "CassoLink/LinkProgram.h"
#include "CassoLink/RunningCassoRegistry.h"
#include "CassoExplorer/Model/TreeModel.h"

class ICassoLinkFolder;
class IByteStreamFactory;
class IDiskFileIo;

struct NetworkAdapter;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections
//
//  Casso Explorer's CassoLink connections: a session for each link the user has
//  started, and what they returned. A request for a drive's listing or a
//  file queues the operation and returns at once; when the session finishes
//  it, a message is posted to the window, and the same request then returns
//  the result. Nothing here waits on a line.
//
//  A link's port is open only while it is started (FR-097): the first
//  request starts it, and Stop, or the link's removal, closes it.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkConnections
{
public:
    CassoLinkConnections (IByteStreamFactory & factory, ICassoLinkFolder & folder, IDiskFileIo & fileIo);
    ~CassoLinkConnections ();

    //  Where a finished operation or a change of state is posted.
    void  SetTarget (HWND hwnd, UINT message) { m_hwnd = hwnd; m_message = message; }

    //  The links, as the preferences have them. A link removed or changed is
    //  stopped.
    void                                    SetLinks (const std::vector<CassoLinkSettings> & links);

    //  The running Cassos with a serial port on TCP, each an Emulated link
    //  with its machine and port as its name, never saved (FR-083b). One whose
    //  address a saved link has is left out, and one on port 1 is listed with
    //  the reason it cannot connect. A Casso that exits stops its link.
    void                                    SetRunning (const std::vector<RunningCasso> & running);

    //  Every link: the saved ones, then the running Cassos.
    const std::vector<CassoLinkSettings> &  GetLinks () const { return m_links; }

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

    //  Stops a link the user disconnected; it stays stopped until Start or
    //  Bootstrap. Opening its root or a listing request does not start it
    //  again, and its root has that it is disconnected.
    void     Disconnect (const std::string & name);
    bool     IsHeld     (const std::string & name) const;

    //  The session of a started link, or null.
    CassoLinkSession *  GetSession (const std::string & name);

    //  The tree's entries for the links: a started link's drives, and for
    //  any other its state and the reason. A drive is listed once its
    //  catalog has been read, so no drive is drawn, selected or opened before
    //  anything is known about its disk.
    std::vector<LinkTreeEntry>  BuildTreeEntries () const;

    //  Each connected Apple's drives read as soon as it replies: a catalog
    //  read is queued for each drive that has none.
    void  ProbeDrives ();

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
    using Queuer     = std::function<std::shared_ptr<LinkOperation> (CassoLinkSession & session)>;
    using Completion = std::function<std::wstring (const LinkOperation & operation)>;

    HRESULT  Submit (const std::string & name, const Queuer & queue, Completion completion);

    //  The completions of the operations that finished, run on the window's
    //  thread; their messages are returned.
    std::vector<std::wstring>  RunCompletions ();

    //  Operations submitted and not yet finished, in the order they run.
    std::vector<std::shared_ptr<LinkOperation>>  GetPendingJobs () const;

    //  The activity log: each connection's connects and drops, its finished
    //  operations, and ADTPro's requests, oldest first, the latest kLogLength.
    struct Activity
    {
        SYSTEMTIME    time = {};
        std::wstring  connection;
        std::wstring  text;
    };

    std::vector<Activity>  GetActivity () const;

    static constexpr size_t  kLogLength = 500;

    //  Whether an image is being written to a drive, or waits to be.
    bool  IsWritingDisk (const std::string & name, int drive) const;

    //  A file's contents, waiting up to timeoutMs for them. Only for a drop,
    //  where the target requests the bytes and cannot finish without them.
    //  pause is called between checks, so the window can paint meanwhile.
    HRESULT  WaitForFile (const std::string & name, int drive, const std::string & path, DWORD timeoutMs, const std::function<void (DWORD)> & pause,
                          FilePayload & outPayload);

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
    static std::wstring  DescribeState (LinkState state, bool servingAdtPro, const CassoLinkSettings & settings, const std::wstring & pcName);

    //  A speed with its thousands separated, as 115,200.
    static std::wstring  FormatBaud    (DWORD baud);

    //  The command a Wi-Fi modem dials for a link: ATDT, this PC's name with
    //  .local, and the link's TCP port (FR-083c).
    static std::wstring  DescribeDial  (const std::wstring & pcName, const CassoLinkSettings & settings);

    //  The dial command itself, as the shim on a boot disk sends it to the
    //  modem; empty for any transport but the Wi-Fi modem.
    static std::string   MakeDialCommand (const std::wstring & pcName, const CassoLinkSettings & settings);

    //  What is written into the shim on a link's boot disk: the link's speed,
    //  slot and dial command, or for an Uthernet II, the W5100 build, with the
    //  network for the link's address and the PC's adapters.
    static LinkShimOptions  MakeShimOptions (const std::wstring & pcName, const CassoLinkSettings & settings);

    //  An Uthernet II's network for a link's address: the address and port;
    //  the mask and gateway of the PC's adapter on its subnet, or
    //  255.255.255.0 and the subnet's .1; and a locally administered MAC
    //  address ending in the address's last byte. Fails for a host that is
    //  not an IPv4 address.
    static HRESULT  MakeNetworkConfig (const CassoLinkSettings & settings, const std::vector<NetworkAdapter> & adapters, LinkNetworkConfig & outConfig);

    //  The slot the Uthernet II ships in, which the link dialog starts it in.
    static constexpr int  kUthernetSlot = 3;

    //  A connection's name when none is given: COM3, Casso on port 6502,
    //  Wi-Fi modem on port 6400, or TCP and the address, with no colon.
    static std::string   DescribeConnection (const CassoLinkSettings & settings);

    //  This PC's DNS host name.
    static std::wstring  GetPcName ();

    static constexpr DWORD  kWaitPollMs = 20;

private:
    class Sink : public ICassoLinkSessionSink
    {
    public:
        Sink (CassoLinkConnections & owner, std::string name) : m_owner (owner), m_name (std::move (name)) {}

        void  OnStateChanged (LinkState state) override;
        void  OnProgress     (const std::shared_ptr<LinkOperation> & operation) override;
        void  OnDone         (const std::shared_ptr<LinkOperation> & operation) override;
        void  OnActivity     (const std::string & text) override;

    private:
        CassoLinkConnections  & m_owner;
        std::string             m_name;
    };

    struct Link
    {
        CassoLinkSettings                                          settings;
        std::unique_ptr<Sink>                                      sink;
        std::unique_ptr<CassoLinkSession>                          session;
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
    ICassoLinkFolder                   & m_folder;
    IDiskFileIo                        & m_fileIo;
    std::vector<CassoLinkSettings>       m_saved;
    std::vector<RunningCasso>            m_running;
    std::vector<CassoLinkSettings>       m_links;
    std::map<std::string, std::wstring>  m_unusable;
    std::set<std::string>                m_held;
    std::vector<std::unique_ptr<Link>>   m_started;

    struct Job
    {
        std::string                     link;
        std::shared_ptr<LinkOperation>  operation;
        Completion                      completion;
    };

    std::vector<Job>                     m_jobs;
    std::deque<Activity>                 m_activity;
    mutable std::mutex                   m_lock;
    std::atomic<bool>                    m_changed { false };
    HWND                                 m_hwnd    = nullptr;
    UINT                                 m_message = 0;
};
