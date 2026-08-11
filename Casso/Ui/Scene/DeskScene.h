#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene
//
//  D3D submission for the desk scene: cached geometry over Dxui3DRenderer,
//  drawn from the main window's before-present hook on the shared device. The
//  thin, untestable edge -- every decision it draws (layout, sub-meshes, UVs,
//  hit results, strip state) is computed by the testable classes it consumes.
//
////////////////////////////////////////////////////////////////////////////////

class DeskScene
{
public:
};
