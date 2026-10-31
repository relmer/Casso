#pragma once

#include "Devices/Tape/ITapeAudioDecoder.h"
#include "Devices/Tape/TapeDeck.h"
#include "Shell/CpuCommandTargets.h"

class IDiskFileIo;
class IFileSystem;
class UserConfigStore;





////////////////////////////////////////////////////////////////////////////////
//
//  TapeManager
//
//  The shell's side of the cassette recorder. Reading and decoding a tape file
//  runs on a background thread, so neither the UI nor the emulation waits on
//  it; the decoded tape and everything else reach the deck as posted commands,
//  which the CPU thread runs through Execute. Inserting saves the path from the
//  CPU thread, as a disk mount does.
//
//  It also saves the inserted tape's path per machine and reinserts the tape,
//  rewound, at the next launch.
//
////////////////////////////////////////////////////////////////////////////////

class TapeManager
{
public:
    using PostFn        = std::function<void (WORD, const std::string &)>;
    using MachineNameFn = std::function<std::wstring ()>;
    using RunFn         = std::function<void (std::function<void ()>)>;

    TapeManager (IDiskFileIo        & fileIo,
                 IFileSystem        & fileSystem,
                 UserConfigStore    & configStore,
                 ITapeAudioDecoder  & compressedDecoder,
                 PostFn               post,
                 MachineNameFn        machineName,
                 RunFn                runInBackground);

    void     Insert            (const std::string & path);
    void     CreateBlank       (const std::string & path);
    void     Eject             ();
    void     Play              ();
    void     Stop              ();
    void     Rewind            ();
    void     FastForward       ();
    void     Seek              (double seconds);
    void     SetRecordArmed    (bool isArmed);
    HRESULT  RestoreSavedTape  ();
    HRESULT  RestoreTape       (const std::wstring & savedPath);
    void     OnMachineSwitched ();

    std::string  GetInsertedPath () const;

    //  The tape being read and decoded right now and how long ago that began,
    //  or an empty path when nothing is loading.
    std::string  GetLoadingPath  (int64_t & elapsedMs) const;

    void     Execute           (TapeCommand command, TapeDeck & deck, uint64_t nowCycle);
    HRESULT  CommitPendingRecording (TapeDeck & deck);

    //  Where a tape that could not be read, created or recorded onto is
    //  reported. Called on the background or CPU thread, so the shell posts it
    //  on.
    void     SetNotifyFn       (std::function<void (const std::wstring &)> notify) { m_notify = std::move (notify); }

    //  New blank tapes are 8-bit when set, 16-bit otherwise. A tape recorded
    //  onto keeps its own.
    void     SetBlankEightBit  (bool isEightBit) { m_blankEightBit.store (isEightBit, std::memory_order_relaxed); }

    static constexpr uint32_t        kBlankSampleRate = 44100;
    static constexpr const char    * kKeepSavedPath   = "keep";   // IDM_TAPE_EJECT payload for an unload

private:
    HRESULT  SaveTapePath    (const std::string & path);

    std::atomic<double>  m_seekSeconds   { 0.0 };     // where the next Seek command moves to
    std::atomic<bool>    m_blankEightBit { false };   // new blank tapes are 8-bit
    HRESULT  LoadAndPost     (const std::string & path, uint64_t request);
    HRESULT  WriteBlank      (const std::string & path);
    void     Notify          (const std::wstring & text) const { if (m_notify) { m_notify (text); } }

    IDiskFileIo        & m_fileIo;
    IFileSystem        & m_fileSystem;
    UserConfigStore    & m_configStore;
    ITapeAudioDecoder  & m_compressedDecoder;
    PostFn               m_post;
    MachineNameFn        m_machineName;
    RunFn                m_runInBackground;
    std::function<void (const std::wstring &)>  m_notify;

    mutable std::mutex                     m_pendingLock;
    std::optional<TapeImage>               m_pending;
    std::string                            m_insertedPath;
    uint64_t                               m_request      = 0;   // the newest insert or eject asked for
    std::string                            m_loadingPath;
    std::chrono::steady_clock::time_point  m_loadStarted;
};
