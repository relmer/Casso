#pragma once

#include "Pch.h"

#include "Devices/Tape/MfTapeAudioDecoder.h"
#include "Devices/Tape/TapeDeck.h"
#include "Shell/CpuCommandTargets.h"
#include "Ui/Chrome/TapeDeckWidget.h"



class BackgroundWorkQueue;
class EmulatorShell;
class IDiskFileIo;
class IFileSystem;
class JsonValue;
class TapeManager;
class UserConfigStore;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellTapeDeck
//
//  The cassette recorder as the emulator presents it: the tape manager and
//  the worker that reads and decodes tape files, the flat recorder widget,
//  the recorder's key latches, and the tape settings. The deck itself -- the
//  transport and the samples -- is the machine's. The queued tape commands
//  are carried out here, on the CPU thread.
//
////////////////////////////////////////////////////////////////////////////////

class ShellTapeDeck : public ICpuTapeCommands
{
public:
    explicit ShellTapeDeck (EmulatorShell & shell);
    ~ShellTapeDeck();

    // Startup: the manager, its file access and its loader, once the config
    // store exists; then the tape the command line gave or the one saved.
    void     Initialize        (UserConfigStore & configStore, IFileSystem & fileSystem);
    void     InsertStartupTape (const std::string & tapePath);

    // Null before Initialize.
    TapeManager *     GetManager () const { return m_tapeManager.get(); }
    TapeDeckWidget &  GetWidget  ()       { return m_tapeChrome; }

    // ICpuTapeCommands: one tape-deck command, against the recorder the
    // machine host owns, timed at the current bus cycle. CPU thread.
    void  ControlTape (TapeCommand command) override;

    // Shown when the machine has a cassette port and the recorder is
    // connected. The flag is per machine, saved in $cassoUiPrefs.
    bool          MachineHasCassettePort () const;
    bool          IsTapeRecorderShown    () const { return MachineHasCassettePort() && m_tapeRecorderConnected; }
    bool          IsRecorderConnected    () const { return m_tapeRecorderConnected; }
    void          SetRecorderConnected   (bool connected) { m_tapeRecorderConnected = connected; }
    void          LoadRecorderConnected  (const JsonValue & uiPrefs);

    TapeDeckView  GetTapeView            () const;
    void          SyncTapeChrome         ();
    void          RegisterTapeDropTarget ();

    // Where the drive row placed the flat recorder, and at what DPI.
    void          SetAnchor              (const RECT & anchor, UINT dpi) { m_tapeAnchor = anchor; m_tapeAnchorDpi = dpi; }
    RECT          GetAnchor              () const { return m_tapeAnchor; }

    void          HandleTapeClick        (TapeDeckRegion region);
    void          PickTape               ();
    void          BrowseForTape          ();
    void          InsertTape             (const std::wstring & path);
    void          CreateBlankTape        ();

    // The desk recorder's keys. Record, Rewind, Fast-forward and Play stay
    // down once pressed, as the RQ-309DS's do, and only Stop, Eject or a
    // reset releases them; ReleaseAtMs is when a pressed Stop or Eject
    // bottoms out and trips the latch.
    std::array<bool, 6> &  GetKeyLatches     ()       { return m_recorderKeyLatched; }
    int64_t                GetKeyReleaseAtMs () const { return m_recorderReleaseAtMs; }
    void                   SetKeyReleaseAtMs (int64_t ms) { m_recorderReleaseAtMs = ms; }

    // How long a key takes to go all the way down.
    static constexpr int64_t  kRecorderKeyDownMs = 120;

    // The drop tag of the tape, after the drives' 0 and 1.
    static constexpr int      kTapeDropTag = 2;

    // Whether tape loads run at Maximum speed. Read by the CPU thread each
    // slice; written by Settings and at startup.
    void  SetFastTapeLoading (bool enabled) { m_fastTapeLoading.store (enabled, std::memory_order_relaxed); }
    bool  IsFastTapeLoading  () const       { return m_fastTapeLoading.load (std::memory_order_relaxed); }
    void  SetTapeAutoStop    (bool enabled);
    void  SetTapeIdleStop    (bool enabled);
    void  SetTapeEightBit    (bool enabled);

    // Seeds the tape settings from the machine's saved $cassoUiPrefs block.
    // A key that is absent keeps its default.
    void  ApplyMachinePrefs  (const JsonValue & uiPrefs);

private:
    void  PromptTapePosition       ();
    void  LatchRecorderKeys        (TapeDeckRegion region);
    bool  ReleaseOtherRecorderKeys (TapeDeckRegion region);
    void  EjectAndPickTape         ();

    EmulatorShell                         & m_shell;

    std::unique_ptr<IDiskFileIo>            m_tapeFileIo;
    MfTapeAudioDecoder                      m_tapeAudioDecoder;
    std::unique_ptr<TapeManager>            m_tapeManager;
    std::unique_ptr<BackgroundWorkQueue>    m_tapeLoader;   // reads and decodes tape files
    std::atomic<bool>                       m_fastTapeLoading { true };

    TapeDeckWidget                          m_tapeChrome;
    RECT                                    m_tapeAnchor    = {};   // where the drive row placed it
    UINT                                    m_tapeAnchorDpi = 0;   // and at what DPI; 0 until it has

    // Whether the cassette recorder is connected, where the machine has a
    // cassette port. Per machine, saved in $cassoUiPrefs.tapeRecorderConnected,
    // and connected by default so new users find it. Disconnecting hides it
    // everywhere and ejects its tape.
    bool                                    m_tapeRecorderConnected = true;

    std::array<bool, 6>                     m_recorderKeyLatched    = {};
    int64_t                                 m_recorderReleaseAtMs   = 0;
    TapeTransport                           m_shownTapeTransport    = TapeTransport::Empty;   // last drawn, to repaint on a change
};
