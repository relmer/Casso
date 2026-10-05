#pragma once

#include "Pch.h"

#include "Controllers/GamePortInputMixer.h"
#include "Shell/HostInputGate.h"

class AppleGamePort;
class Apple2eSoftSwitchBank;
class Apple2eKeyboard;
class SiriusJoyport;
class DivergenceGate;





////////////////////////////////////////////////////////////////////////////////
//
//  GamePortTargets
//
//  The devices one machine exposes for the game port. A ][ or ][+ has the
//  game port; a //e or //c has the soft-switch bank's paddles and the
//  keyboard's Apple keys. All null means the machine has no game port.
//
//  axisCount is how many paddle axes the machine exposes: four on the ][,
//  ][+ and //e, two on the //c, whose PDL2 and PDL3 lines carry the mouse.
//
//  joyport is the machine's Sirius Joyport, attached or not; null on the //c.
//
////////////////////////////////////////////////////////////////////////////////

struct GamePortTargets
{
    AppleGamePort          * gamePort    = nullptr;
    Apple2eSoftSwitchBank  * iieSwitches = nullptr;
    Apple2eKeyboard        * iieKeyboard = nullptr;
    SiriusJoyport          * joyport     = nullptr;
    size_t                   axisCount   = GamePortContribution::kAxisCount;
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

    void SetInputGate (HostInputGate * inputGate) { m_inputGate = inputGate; }

    // Behind live, a write the input gate refuses is judged here, and
    // requestAsk is called when it would change what the machine reads.
    void SetDivergenceGate (DivergenceGate * divergenceGate, std::function<void()> requestAsk);

    bool TryApply (const GamePortState & target, const GamePortState * lastApplied) override;

private:

    static void           WritePaddles (const GamePortTargets & targets, const GamePortState & target, const GamePortState * lastApplied);
    static void           WriteButtons (const GamePortTargets & targets, const GamePortState & target, const GamePortState * lastApplied);
    static void           WriteJacks   (const GamePortTargets & targets, const GamePortState & target, const GamePortState * lastApplied);
    static GamePortState  ReadRecorded (const GamePortTargets & targets);
    static void           MaskUnread   (const GamePortTargets & targets, GamePortState & state);

    void                  JudgeBehindLive (const GamePortTargets & targets, const GamePortState & target, const GamePortState * lastApplied);

    std::shared_mutex      & m_lifetimeLock;
    HostInputGate          * m_inputGate      = nullptr;
    DivergenceGate         * m_divergenceGate = nullptr;
    std::function<void()>    m_requestAsk;
    TargetsFn                m_getTargets;
};
