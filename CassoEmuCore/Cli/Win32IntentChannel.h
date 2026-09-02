#pragma once

#include "Pch.h"
#include "IIntentChannel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32IntentChannel
//
//  The platform half of telling a running emulator what a write meant.
//
//  IN CORE, AND NOT NEGOTIABLE. This is the SENDER: its callers run inside
//  CassoCli.exe, which cannot link Casso.exe, so a shim in the emulator's shell
//  would not merely be poor layering -- it would not link. Win32DiskFileIo sits
//  beside it for the same reason the constitution gives: the criterion is
//  testability, not a platform boundary.
//
//  WM_COPYDATA TO EVERY TOP-LEVEL CassoWindow, FOUND BY ENUMERATION. Not
//  HWND_BROADCAST: WM_COPYDATA may not be sent that way, because the receiver
//  has to read memory the message points at, and a broadcast has no one
//  process's address space to read from.
//
//  SendMessage, NEVER PostMessage. The payload has to stay alive for the
//  duration of the call, which posting cannot promise -- and the send carries a
//  timeout, so a hung emulator cannot hang a build.
//
////////////////////////////////////////////////////////////////////////////////

class Win32IntentChannel : public IIntentChannel
{
public:

    //  What a message this channel sent carries in its `dwData`.
    //
    //  REGISTERED RATHER THAN A CONSTANT, so an unrelated WM_COPYDATA from
    //  anything else in the system cannot collide with it by accident. The
    //  filter that lets the message through the integrity boundary takes a
    //  WINDOW MESSAGE and cannot see this, so it is checked in the handler.
    static ULONG_PTR  GetMessageId();

    //  The window class every Casso emulator window is registered under.
    static constexpr const wchar_t *  kWindowClass = L"CassoWindow";

    //  How long to wait on one emulator before giving up on it.
    //
    //  A BUILD MUST NOT BE HELD UP BY A WEDGED EMULATOR. The receiver does
    //  nothing but record a pending change, so anything approaching this
    //  timeout means the other process is not answering at all.
    static constexpr UINT  kSendTimeoutMs = 2000;

    //  Everything a stated intent puts on the wire.
    struct Payload
    {
        PickUpIntent  intent = PickUpIntent::Unstated;
        std::string   imagePath;
    };

    //  Packs an intent and a path into the bytes a message carries.
    //
    //  THE PATH GOES AS UTF-8 and absolute, as the writer resolved it. A
    //  receiver holds its own spelling of the same file and matches after
    //  normalizing, so a relative path would name nothing on the other side.
    static std::vector<Byte>  Encode (const std::string & imagePath, PickUpIntent intent);

    //  Reads bytes back, refusing anything that is not a whole valid payload.
    //
    //  A FUNCTION RATHER THAN CODE INSIDE A WINDOW PROCEDURE, because deciding
    //  whether a payload is well formed is assertable logic and a message
    //  handler is where no test can reach it. Truncation, an implausible
    //  length and an intent value this build does not know are all things
    //  another process can send, and all three must be refused rather than
    //  read past.
    static bool  Decode (const Byte * bytes, size_t byteCount, Payload & outPayload);

    //  The largest payload worth reading. A path cannot approach this, and a
    //  length that does is a message this channel did not send.
    static constexpr size_t  kMaxPayloadBytes = 4096;

    void  StateIntent (const std::string & imagePath, PickUpIntent intent) override;
};
