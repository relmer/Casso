#pragma once

#include "Pch.h"

#include "Controllers/GamePortInputMixer.h"

class AppleGamePort;
class Apple2eSoftSwitchBank;
class Apple2eKeyboard;
class SiriusJoyport;
class Apple3AnalogInputs;
class DivergenceGate;
class HostInputGate;





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
//  apple3 is the Apple ///'s A/D and joystick switches: paddles 0 and 1 are
//  port A's X and Y, paddles 2 and 3 port B's, and the three buttons port A
//  switches 0 and 1 and port B switch 0.
//
////////////////////////////////////////////////////////////////////////////////

struct GamePortTargets
{
    AppleGamePort          * gamePort    = nullptr;
    Apple2eSoftSwitchBank  * iieSwitches = nullptr;
    Apple2eKeyboard        * iieKeyboard = nullptr;
    SiriusJoyport          * joyport     = nullptr;
    Apple3AnalogInputs     * apple3      = nullptr;
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

    // Behind live, a write the input gate blocks is held here, line by line,
    // and onHeld is called so the shell can watch for the guest reading it.
    void SetDivergenceGate (DivergenceGate * divergenceGate, std::function<void()> onHeld);

    bool TryApply (const GamePortState & target, const GamePortState * lastApplied) override;

private:

    static void           WritePaddles (const GamePortTargets & targets, const GamePortState & target, const GamePortState * lastApplied);
    static void           WriteButtons (const GamePortTargets & targets, const GamePortState & target, const GamePortState * lastApplied);
    static void           WriteJacks   (const GamePortTargets & targets, const GamePortState & target, const GamePortState * lastApplied);
    static void           MaskUnread   (const GamePortTargets & targets, GamePortState & state);

    void                  JudgeBehindLive (const GamePortTargets & targets, const GamePortState & target, const GamePortState * lastApplied);

    std::shared_mutex      & m_lifetimeLock;
    HostInputGate          * m_inputGate      = nullptr;
    DivergenceGate         * m_divergenceGate = nullptr;
    std::function<void()>    m_onHeld;
    TargetsFn                m_getTargets;
};
