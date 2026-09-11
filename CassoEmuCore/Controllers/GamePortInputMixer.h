#pragma once

#include "Pch.h"

#include "Controllers/ControllerTypes.h"





enum class GamePortSource
{
    ArrowKeys,
    FireKeys,
    AppleModifierKeys,
    MousePaddle,
    Controller,
};





enum class AxisOwner
{
    None,
    ArrowKeys,
    MousePaddle,
    Controller,
};





////////////////////////////////////////////////////////////////////////////////
//
//  GamePortState
//
//  The final values the machine should read: both paddle axes and PB0-PB2.
//
////////////////////////////////////////////////////////////////////////////////

struct GamePortState
{
    static constexpr Byte  kPaddleCenter = 127;

    std::array<Byte, 2>                               paddle  = { kPaddleCenter, kPaddleCenter };
    std::bitset<GamePortContribution::kButtonCount>   buttons;

    bool operator== (const GamePortState &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IGamePortSink
//
//  Writes a game-port state into a machine. Called only on the mixer's apply
//  thread. lastApplied is null when every field must be written.
//
////////////////////////////////////////////////////////////////////////////////

class IGamePortSink
{
public:

    virtual ~IGamePortSink () = default;

    virtual bool TryApply (const GamePortState & target, const GamePortState * lastApplied) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  GamePortInputMixer
//
//  The single owner of the paddle and pushbutton values every host input
//  source drives.
//
////////////////////////////////////////////////////////////////////////////////

class GamePortInputMixer
{
public:

    void           SetSink              (IGamePortSink * sink);
    void           SetApplyThread       (std::thread::id applyThread, std::function<void()> requestFlush);
    void           SetAxisOwner         (AxisOwner owner);
    void           Submit               (GamePortSource source, const GamePortContribution & contribution);
    void           ReleaseSource        (GamePortSource source);
    void           NotifyMachineRebuilt ();
    bool           FlushPending         ();
    bool           HasPendingWrite      () const;
    GamePortState  GetTargetState       () const;

private:

    static constexpr size_t  kSourceCount = 5;

    void                          ScheduleApply             ();
    GamePortState                 ComputeTargetLocked       () const;
    const GamePortContribution *  GetAxisContributionLocked () const;

    mutable std::mutex                                 m_mutex;
    std::array<GamePortContribution, kSourceCount>     m_contributions;
    AxisOwner                                          m_owner          = AxisOwner::None;
    IGamePortSink                                    * m_sink           = nullptr;
    std::thread::id                                    m_applyThread;
    std::function<void()>                              m_requestFlush;
    GamePortState                                      m_lastApplied;
    bool                                               m_hasApplied     = false;
    bool                                               m_flushRequested = false;
};
