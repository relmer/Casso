#pragma once

#include "Pch.h"

#include "Controllers/GamePortInputMixer.h"

class AppleGamePort;
class Apple2eSoftSwitchBank;
class Apple2eKeyboard;





////////////////////////////////////////////////////////////////////////////////
//
//  GamePortTargets
//
//  The devices one machine exposes for the game port. A ][ or ][+ has the
//  game port; a //e or //c has the soft-switch bank's paddles and the
//  keyboard's Apple keys. All null means the machine has no game port.
//
////////////////////////////////////////////////////////////////////////////////

struct GamePortTargets
{
    AppleGamePort          * gamePort    = nullptr;
    Apple2eSoftSwitchBank  * iieSwitches = nullptr;
    Apple2eKeyboard        * iieKeyboard = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MachineGamePortSink
//
//  Writes the mixer's game-port state into the running machine's devices.
//
////////////////////////////////////////////////////////////////////////////////

class MachineGamePortSink : public IGamePortSink
{
public:

    using TargetsFn = std::function<GamePortTargets()>;

    MachineGamePortSink (std::shared_mutex & lifetimeLock, TargetsFn getTargets);

    bool TryApply (const GamePortState & target, const GamePortState * lastApplied) override;

private:

    static void WritePaddles (const GamePortTargets & targets, const GamePortState & target, const GamePortState * lastApplied);
    static void WriteButtons (const GamePortTargets & targets, const GamePortState & target, const GamePortState * lastApplied);

    std::shared_mutex  & m_lifetimeLock;
    TargetsFn            m_getTargets;
};
