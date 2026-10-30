#pragma once

#include "Pch.h"

class IByteStream;





////////////////////////////////////////////////////////////////////////////////
//
//  LinkBootstrap
//
//  Starts the link on an Apple with no copy of it, over serial. At the
//  BASIC prompt, the user types IN#2 on a //c, or IN# and a
//  Super Serial Card's slot, and then Ctrl-A 14B, which sets the port to
//  9,600 baud and takes the keyboard's input from it; on the card, Return
//  follows. The //c's firmware and the card's have the same commands. What
//  arrives on the port is then typed: Ctrl-X, which cancels any partial line
//  that stray bytes on the port put in the input buffer, CALL -151 for the
//  monitor, the serial loader as monitor lines from $3A50, and 3A50G. The
//  loader takes the slot IN# set and sends kLoaderReady. The shim follows as
//  raw bytes, and runs at the bootstrap speed and sends Ready.
//
//  Each typed line ends with a carriage return and is followed by a pause,
//  as the monitor takes in a line and draws it before reading the next.
//
////////////////////////////////////////////////////////////////////////////////

class LinkBootstrap
{
public:
    //  A line of 8 bytes fits on one screen row. A longer line wraps, and a //c
    //  drops characters that arrive while the monitor scrolls.
    static constexpr DWORD   kLinePauseMs  = 250;
    static constexpr size_t  kBytesPerLine = 8;
    static constexpr DWORD   kCharPauseUs  = 3000;

    //  The loader's byte when it is running and waiting for the shim, and how
    //  long the host waits for it after typing the command that runs it.
    static constexpr Byte    kLoaderReady       = 0xA5;
    static constexpr DWORD   kLoaderReadyWaitMs = 3000;

    //  Ctrl-X, which GETLN takes as cancel line, in Applesoft and the monitor.
    static constexpr const char *  kCancelLine = "\x18";

    //  The lines to type: Ctrl-X, CALL -151, the serial loader, and the
    //  command that runs it, each with its carriage return.
    static HRESULT  BuildLines (std::vector<std::string> & outLines);

    //  What the loader reads: the page count, the shim starting at the given
    //  speed padded with zeros to whole pages, and the XOR of those pages.
    static HRESULT  BuildShimTransfer (DWORD baud, std::vector<Byte> & outBytes);

    //  Waits for the loader's ready byte, skipping anything before it, then
    //  sends the transfer.
    static HRESULT  SendShim (IByteStream & stream, const std::vector<Byte> & transfer);

    //  Types the lines into the stream a character at a time, waiting
    //  charPauseUs after each character and pauseMs after each line. Stops
    //  early, with ERROR_CANCELLED, when `canceled` returns true.
    static HRESULT  Send (IByteStream & stream, const std::vector<std::string> & lines, DWORD pauseMs,
                          const std::function<bool()> & canceled = {}, DWORD charPauseUs = 0);

private:
    static void     WaitMicroseconds (DWORD microseconds);
};
