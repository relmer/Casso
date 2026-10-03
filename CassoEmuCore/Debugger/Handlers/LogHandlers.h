#pragma once

#include "Core/IWatchSink.h"
#include "Debugger/IDebugCommandHandler.h"
#include "Debugger/IoEventLog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LogHandlers
//
//  VIDEOLOG and SOUNDLOG: the video mode changes, and the speaker toggles
//  and Mockingboard writes, each with the cycle it happened at. While either
//  log is on, the I/O page (and, for SOUNDLOG, the Mockingboard's page) take
//  the bus's watched path and every access to them is offered here; while
//  both are off, those pages go back to the fast path and the logs cost
//  nothing.
//
////////////////////////////////////////////////////////////////////////////////

class LogHandlers : public IDebugCommandHandler
{
public:
    bool  TryExecute  (DebugSession & session, const DebugCommand & command, Reply & reply) override;

    //  One access to a logged page, after the device has seen it.
    void  OnIoAccess  (DebugSession & session, Word address, Byte value, BusAccess access);

    const IoEventLog & GetVideoLog () const { return m_video; }
    const IoEventLog & GetSoundLog () const { return m_sound; }

private:
    static constexpr size_t        kDefaultListCount = 100;
    static constexpr size_t        kMaxCountDigits   = 9;
    static constexpr int           kIoPage           = 0xC0;
    static constexpr int           kPageShift        = 8;
    static constexpr const char  * kDefaultVideoFile = "VideoLog.txt";
    static constexpr const char  * kDefaultSoundFile = "SoundLog.txt";

    void  Execute      (DebugSession & session, const DebugCommand & command, IoEventLog & log, bool isVideo, Reply & reply);
    void  TurnOn       (DebugSession & session, IoEventLog & log, bool isVideo);
    void  UpdateSink   (DebugSession & session);
    void  Save         (DebugSession & session, const IoEventLog & log, const std::string & name, Reply & reply) const;

    IoEventLog               m_video;
    IoEventLog               m_sound;
    uint16_t                 m_videoBits = 0;
    std::vector<SoftSwitch>  m_switches;
};
