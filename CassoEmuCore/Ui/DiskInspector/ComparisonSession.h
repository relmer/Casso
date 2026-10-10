#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskComparison.h"
#include "Shell/BackgroundWorkQueue.h"
#include "Ui/DiskInspector/AnalysisScheduler.h"
#include "Ui/DiskInspector/ComparisonText.h"
#include "Ui/DiskInspector/IDiskInspectorHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LoadedDisk
//
//  A side's disk once its source has been read, or why it could not be.
//
////////////////////////////////////////////////////////////////////////////////

struct LoadedDisk
{
    int                              side = 0;
    std::shared_ptr<const DiskCopy>  copy;
    std::wstring                     reason;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonSession
//
//  One comparison of the inspector window (FR-117 to FR-121). The window
//  keeps side A, its own disk unless the user chose another source for it;
//  the session keeps side B with its own analysis, and reads either side's
//  source: a drive's disk through the host, a file on a background thread.
//  Once both sides are analyzed and either has changed since the last
//  comparison, the two are compared on that same thread, so the window
//  keeps responding. Comparing changes neither disk.
//
////////////////////////////////////////////////////////////////////////////////

class ComparisonSession
{
public:
    static constexpr int  kSideA = 0;
    static constexpr int  kSideB = 1;

    ComparisonSession  ();
    ~ComparisonSession ();

    ComparisonSession (const ComparisonSession &)             = delete;
    ComparisonSession & operator= (const ComparisonSession &) = delete;

    //  Starts reading B's source, and A's when A is not the window's disk.
    void  Begin (const ComparisonSource & a, const ComparisonSource & b, bool isLoadingA, const DecodeSettings & settings, IDiskInspectorHost & host);
    void  End   ();

    //  A host reply to one of the session's reads; false for any other.
    bool  OfferReply (const InspectorReply & reply);

    //  Once a frame: B's reads and analysis results, A's reads handed to the
    //  window, and a comparison started when both sides are ready.
    void  Tick (const DiskAnalysis & a, bool isAReady, vector<LoadedDisk> & outLoadsForA);

    void  MarkAChanged     ();
    void  ApplySettings    (const DecodeSettings & settings);
    void  ApplyOwnSettings (const DecodeSettings & settings);
    void  SetOptions       (const ComparisonOptions & options);

    //  A and B change places: the window's side and the session's swap
    //  each analysis, scheduler and source.
    void  Swap (DiskAnalysis & inOutA, std::unique_ptr<AnalysisScheduler> & inOutScheduler, ComparisonSource & inOutSource);

    bool                        IsComparing    () const { return m_isComparing; }
    bool                        IsBusy         () const;
    bool                        HasResult      () const { return m_hasResult; }
    bool                        HasOwnSettings () const { return m_hasOwnSettings; }
    const ComparisonSource &    GetSourceA     () const { return m_sources[kSideA]; }
    const ComparisonSource &    GetSourceB     () const { return m_sources[kSideB]; }
    const DiskAnalysis &        GetB           () const { return m_b; }
    const AnalysisScheduler &   GetBScheduler  () const { return *m_bScheduler; }
    const DiskComparison &      GetResult      () const { return m_result; }
    const vector<Difference> &  GetListed      () const { return m_listed; }
    const ComparisonOptions &   GetOptions     () const { return m_options; }
    const std::wstring &        GetBError      () const { return m_errors[kSideB]; }
    const std::wstring &        GetAError      () const { return m_errors[kSideA]; }
    uint64_t                    GetVersion     () const { return m_version; }

private:
    struct Read
    {
        ComparisonSource  source;
        uint64_t          hostRequest = 0;
        bool              isActive    = false;
    };

    struct Compared
    {
        uint64_t        generation = 0;
        uint64_t        change     = 0;
        DiskComparison  result;
    };

    void  StartRead   (int side, IDiskInspectorHost & host);
    void  ReadFile    (int side, const std::string & utf8Path);
    void  Deliver     (LoadedDisk loaded, vector<LoadedDisk> & outLoadsForA);
    void  StartB      (std::shared_ptr<const DiskCopy> copy);
    void  TakeBResults ();
    void  StartCompare (const DiskAnalysis & a);
    void  TakeCompared ();
    bool  IsBReady     () const;

    bool                                m_isComparing    = false;
    std::array<ComparisonSource, 2>     m_sources;
    std::array<Read, 2>                 m_reads;
    std::array<std::wstring, 2>         m_errors;
    DiskAnalysis                        m_b;
    std::unique_ptr<AnalysisScheduler>  m_bScheduler;
    DecodeSettings                      m_settings       = DecodeSettings::MakeStandard();
    bool                                m_hasOwnSettings = false;
    ComparisonOptions                   m_options;
    DiskComparison                      m_result;
    vector<Difference>                  m_listed;
    bool                                m_hasResult      = false;
    uint64_t                            m_version        = 0;
    uint64_t                            m_generation     = 0;
    uint64_t                            m_changes        = 0;
    uint64_t                            m_posted         = 0;
    bool                                m_isRunning      = false;

    std::mutex                               m_lock;
    vector<std::pair<uint64_t, LoadedDisk>>  m_fileLoads;
    vector<Compared>                         m_compared;

    BackgroundWorkQueue                 m_queue;
};
