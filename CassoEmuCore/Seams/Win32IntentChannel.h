#pragma once

#include "Pch.h"
#include "Seams/IIntentChannel.h"





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
//  A SECOND DIRECTION, FOR A TOOL THAT WAITS FOR AN ANSWER. A request to insert
//  a disk or describe the machine is sent to one window, with the sender's own
//  window in `wParam`, and the emulator answers with a WM_COPYDATA of its own
//  under a second registered id. The two ids keep the directions apart, and a
//  fire-and-forget writer that passes no window gets no answer, exactly as
//  before.
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

    //  What an answer from the emulator carries in its `dwData`.
    static ULONG_PTR  GetReplyMessageId();

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
        ExternalChangeIntent  intent    = ExternalChangeIntent::Unstated;
        std::string           imagePath;

        //  1 or 2, for InsertDisk; zero for every other intent.
        int                   drive     = 0;
    };

    //  What the emulator can answer.
    enum class ReplyKind : Byte
    {
        MachineDescription = 1,
        InsertDone,
        InsertRefused,
        ReloadDone,
        ReloadConflict,
        ReloadRefused,
    };

    struct Reply
    {
        ReplyKind    kind       = ReplyKind::InsertDone;
        int          driveCount = 0;       // MachineDescription only
        std::string  text;                 // the machine name, or the reason
    };

    //  Packs an intent and a path into the bytes a message carries.
    //
    //  THE PATH GOES AS UTF-8 and absolute, as the writer resolved it. A
    //  receiver holds its own spelling of the same file and matches after
    //  normalizing, so a relative path would name nothing on the other side.
    static std::vector<Byte>  Encode (const std::string & imagePath, ExternalChangeIntent intent);

    //  An insert: the intent, the drive byte, then the path.
    static std::vector<Byte>  EncodeInsert (const std::string & imagePath, int drive);

    //  A describe request: the intent byte and nothing else.
    static std::vector<Byte>  EncodeDescribe();

    //  Reads bytes back, refusing anything that is not a whole valid payload.
    //
    //  A FUNCTION RATHER THAN CODE INSIDE A WINDOW PROCEDURE, because deciding
    //  whether a payload is well formed is assertable logic and a message
    //  handler is where no test can reach it. Truncation, an implausible
    //  length and an intent value this build does not know are all things
    //  another process can send, and all three must be refused rather than
    //  read past.
    static bool  Decode (const Byte * bytes, size_t byteCount, Payload & outPayload);

    //  The answer's bytes: the kind, a drive count for a description, then
    //  the text.
    static std::vector<Byte>  EncodeReply (const Reply & reply);
    static bool               DecodeReply (const Byte * bytes, size_t byteCount, Reply & outReply);

    //  The largest payload worth reading. A path cannot approach this, and a
    //  length that does is a message this channel did not send.
    static constexpr size_t  kMaxPayloadBytes = 4096;

    //  The largest drive count a description can report.
    static constexpr int  kMaxDriveCount = 2;

    void  StateIntent (const std::string & imagePath, ExternalChangeIntent intent) override;

    //  One message to one window, carrying `sender` as its `wParam`. False when
    //  the window did not take it within the timeout.
    static bool  SendTo (HWND target, HWND sender, ULONG_PTR messageId, const std::vector<Byte> & bytes);
};
