#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeldHostInputs
//
//  The host keys and mouse button the machine has down, and the ones the user
//  let go of while the machine was behind live, when the host input gate kept
//  the release from reaching it. Going live restores the machine to where it
//  stood when it left live, with those inputs still down; the shell injects a
//  release for each one TakeReleases gives, so none stays stuck. An input
//  still held is left alone, as is one the machine never had down.
//
//  A key is its virtual-key code; the mouse button is kMouseButton. UI thread
//  only.
//
////////////////////////////////////////////////////////////////////////////////

class HeldHostInputs
{
public:
    static constexpr WPARAM  kMouseButton = VK_LBUTTON;

    //  isLive: whether the host input gate let the press or release through.
    void                 OnPress      (WPARAM input, bool isLive);
    void                 OnRelease    (WPARAM input, bool isLive);
    void                 OnReleaseAll (bool isLive);
    std::vector<WPARAM>  TakeReleases ();

private:
    std::set<WPARAM>  m_held;
    std::set<WPARAM>  m_released;
};
