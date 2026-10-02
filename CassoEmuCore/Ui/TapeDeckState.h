#pragma once

#include "Devices/Tape/TapeDeck.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeckView
//
//  What the tape deck shows, on the UI thread: the inserted file, the
//  transport, and position and length in seconds. Built from the deck's
//  thread-safe snapshot plus the path the UI thread inserted.
//
////////////////////////////////////////////////////////////////////////////////

struct TapeDeckView
{
    std::wstring   path;
    TapeTransport  transport       = TapeTransport::Empty;
    double         positionSeconds = 0.0;
    double         lengthSeconds   = 0.0;
    bool           isRecordArmed   = false;
    bool           isWritable      = false;
};
