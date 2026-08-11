#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneLayout
//
//  Computes the desk scene's composition -- device world transforms, the ONE
//  shared camera, and the scene scale -- from the viewport rect, DPI, and the
//  machine's device configuration. Deterministic and GPU-free so composition
//  rules (containment, drive count, position-derived perspective) are
//  unit-testable.
//
////////////////////////////////////////////////////////////////////////////////

class DeskSceneLayout
{
public:
};
