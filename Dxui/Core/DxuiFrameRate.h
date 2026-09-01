#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFrameRate
//
//  Frames per second, averaged over a window rather than taken from the last
//  frame.
//
//  Ported from MatrixRain's FPSCounter, which is the same three lines: sum the
//  deltas, count the frames, and divide when a window's worth has gone by.
//
//  A WINDOW, NOT AN INSTANT. The reciprocal of one frame's delta is a number
//  that changes every frame and reads as noise; what a readout is for is
//  whether the frame rate is where it should be, which needs a figure that
//  holds still long enough to be read. A second is long enough to be steady
//  and short enough to show a stall.
//
//  Takes the delta rather than reading a clock, so the arithmetic is testable
//  without one. The caller owns the clock; DxuiHwndSource ticks this from
//  PresentFrame, which counts the frames that actually reached the screen
//  rather than the ones the shell considered drawing.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiFrameRate
{
public:

    // How much elapsed time one average covers.
    static constexpr float  kWindowSeconds = 1.0f;

    // One presented frame, `deltaSeconds` after the one before it. A
    // non-positive delta is ignored: a clock that did not move cannot be
    // divided by, and on a paused or resumed process it can go backwards.
    void  Tick (float deltaSeconds);

    // Frames per second over the last completed window, or zero until the
    // first one closes.
    float  GetFramesPerSecond () const { return m_framesPerSecond; }

    // Forgets the window in progress, for a caller resuming after a stall it
    // does not want averaged in -- a modal loop, a minimized spell.
    void  Reset ();

private:
    float  m_framesPerSecond = 0.0f;
    float  m_elapsed         = 0.0f;
    int    m_frames          = 0;
};
