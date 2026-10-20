#pragma once

#include "Pch.h"

class IByteStream;





////////////////////////////////////////////////////////////////////////////////
//
//  LinkBootstrap
//
//  Starts the link on an Apple with no copy of it, over serial (FR-092,
//  R23). With the //c at the BASIC prompt, the user types IN#2 and then
//  Ctrl-A 14B, which sets port 2 to 9,600 baud and takes the keyboard's
//  input from it. What arrives on the port is then typed: Ctrl-X, which
//  cancels any partial line that stray bytes on the port put in the input
//  buffer, CALL -151 for the monitor, the shim as monitor lines of eight
//  bytes from $3D00, and 3D00G. The shim runs at the bootstrap speed and
//  sends Ready.
//
//  Each line ends with a carriage return and is followed by a pause, as
//  the monitor takes in a line and draws it before reading the next.
//
////////////////////////////////////////////////////////////////////////////////

class LinkBootstrap
{
public:
    static constexpr DWORD   kLinePauseMs  = 250;
    static constexpr size_t  kBytesPerLine = 8;

    //  Ctrl-X, which GETLN takes as cancel line, in Applesoft and the monitor.
    static constexpr const char *  kCancelLine = "\x18";

    //  The lines to type, each with its carriage return, for a shim starting
    //  at the given speed.
    static HRESULT  BuildLines (DWORD baud, std::vector<std::string> & outLines);

    //  Types the lines into the stream, pausing after each. Stops early, with
    //  ERROR_CANCELLED, when `canceled` returns true.
    static HRESULT  Send (IByteStream & stream, const std::vector<std::string> & lines, DWORD pauseMs,
                          const std::function<bool()> & canceled = {});
};
