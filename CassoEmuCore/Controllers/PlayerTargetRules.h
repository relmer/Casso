#pragma once

#include "Pch.h"

#include "Controllers/ControllerSelectionPolicy.h"
#include "Controllers/ControllerTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PlayerTargetRules
//
//  What a player's controller reaches on the game port: what the players'
//  modes give each one, which paddles its own PDL0.. bindings land on and
//  which of its button bindings reach which line. Pure lookups, so every row
//  of the wiring can be asserted on its own.
//
////////////////////////////////////////////////////////////////////////////////

class PlayerTargetRules
{
public:

    static constexpr size_t  kButtonCount = static_cast<size_t> (GamePortContribution::kButtonCount);

    // For each of the controller's own button targets, pb0-pb2, the machine
    // line it reaches, or none.
    using ButtonRoute = std::array<std::optional<size_t>, kButtonCount>;

    // For each of the controller's own axis targets, pdl0-pdl3, the machine
    // paddle it lands on, or none; and its buttons as above.
    struct Route
    {
        std::array<std::optional<size_t>, GamePortContribution::kAxisCount>  paddles;
        ButtonRoute                                                          buttons;

        bool operator== (const Route &) const = default;
    };

    // What a player in Joystick, Paddle or Two paddles mode drives, beside
    // Player 1 in any of them. `isPlayerOnePaddle` is Player 1 on a single
    // paddle.
    static PlayerAxisTarget  GetModeTarget      (size_t      player,
                                                 bool        isPaddle,
                                                 bool        isTwoPaddles,
                                                 bool        isPlayerOnePaddle);
    static bool              IsPaddleTarget     (PlayerAxisTarget target);
    static bool              IsPaddlePairTarget (PlayerAxisTarget target);
    static ButtonRoute       GetButtonRoute     (PlayerAxisTarget target);
    static Route             GetSingleRoute     (size_t axisCount);
    static Route             GetLoneRoute       (PlayerAxisTarget target, size_t axisCount);
    static Route             GetTargetRoute     (PlayerAxisTarget target, size_t axisCount);
    static size_t            CountPaddles       (const Route & route);
};
