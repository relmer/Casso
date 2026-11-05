#pragma once

#include "Pch.h"

#include "Debugger/Events/DebugEventCache.h"
#include "Debugger/IoEventLog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IoLogKind, IoLogEntries
//
//  The video log or the sound log, and a listing of one: its entries, oldest
//  first, and how far back they go. isComplete is set when they are every
//  entry since the log was cleared, or since it began; isTimeLimited when
//  older history was not searched because the rebuild took too long.
//
////////////////////////////////////////////////////////////////////////////////

enum class IoLogKind
{
    Video,
    Sound,
    Serial,
    Network,
};

struct IoLogEntries
{
    std::vector<DebugEventEntry<IoEvent>>  entries;
    bool                                   isComplete    = false;
    bool                                   wasCleared    = false;
    bool                                   isTimeLimited = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IIoEventSource
//
//  The video and sound logs as VIDEOLOG and SOUNDLOG use them, on the thread
//  that runs the machine. Entries are added to both all the time;
//  ClearIoLog hides the entries before the machine's position, and
//  GetIoEntries returns the newest, from the event cache and then from
//  history rebuilt until there are maxCount.
//
////////////////////////////////////////////////////////////////////////////////

class IIoEventSource
{
public:
    virtual          ~IIoEventSource() = default;

    virtual void     ClearIoLog   (IoLogKind kind) = 0;
    virtual HRESULT  GetIoEntries (IoLogKind kind, size_t maxCount, IoLogEntries & outEntries) = 0;
};
