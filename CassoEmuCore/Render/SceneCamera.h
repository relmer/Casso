#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SceneCamera
//
//  Row-vector matrix math for the desk scene (clip = v * view * proj), matching
//  Dxui3DRenderer's row_major cbuffer convention. Pure and system-free so every
//  transform the scene relies on -- including the input inverse-projection path
//  -- is unit-testable with no GPU.
//
////////////////////////////////////////////////////////////////////////////////

class SceneCamera
{
public:
    static void  Identity44 (float out[16]);
};
