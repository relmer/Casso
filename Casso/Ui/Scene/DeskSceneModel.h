#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel
//
//  One device kind's loaded mesh and everything discovered from it: sub-meshes
//  split by material color (glass, lamps), the curved display surface built
//  from the glass geometry, synthesized glass UVs, and the model-space
//  interactive region boxes. Parsing and discovery are data-in/data-out over
//  OBJ/MTL text, so the whole load path is unit-testable with synthetic
//  buffers.
//
////////////////////////////////////////////////////////////////////////////////

class DeskSceneModel
{
public:
};
