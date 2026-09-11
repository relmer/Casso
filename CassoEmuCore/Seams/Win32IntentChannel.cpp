#include "Pch.h"

#include "Seams/Win32IntentChannel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32IntentChannel::GetMessageId
//
//  The id every message this channel sends carries in `dwData`.
//
//  REGISTERED ONCE PER PROCESS AND SHARED SYSTEM-WIDE, which is what makes it
//  the same number in the sender and the receiver without either one hard-coding
//  a value that something else might also have chosen.
//
////////////////////////////////////////////////////////////////////////////////

ULONG_PTR Win32IntentChannel::GetMessageId()
{
    static const UINT  s_kId = RegisterWindowMessageW (L"CassoDiskImageIntent");



    return (ULONG_PTR) s_kId;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32IntentChannel::GetReplyMessageId
//
////////////////////////////////////////////////////////////////////////////////

ULONG_PTR Win32IntentChannel::GetReplyMessageId()
{
    static const UINT  s_kId = RegisterWindowMessageW (L"CassoIntentReply");



    return (ULONG_PTR) s_kId;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32IntentChannel::EncodeInsert
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> Win32IntentChannel::EncodeInsert (const std::string & imagePath, int drive)
{
    std::vector<Byte>  bytes;



    bytes.push_back ((Byte) ExternalChangeIntent::InsertDisk);
    bytes.push_back ((Byte) drive);
    bytes.insert (bytes.end(), imagePath.begin(), imagePath.end());

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32IntentChannel::EncodeDescribe
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> Win32IntentChannel::EncodeDescribe()
{
    return std::vector<Byte> { (Byte) ExternalChangeIntent::DescribeMachine };
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32IntentChannel::EncodeReply
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> Win32IntentChannel::EncodeReply (const Reply & reply)
{
    std::vector<Byte>  bytes;



    bytes.push_back ((Byte) reply.kind);

    if (reply.kind == ReplyKind::MachineDescription)
    {
        bytes.push_back ((Byte) reply.driveCount);
    }

    bytes.insert (bytes.end(), reply.text.begin(), reply.text.end());

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32IntentChannel::DecodeReply
//
//  The same suspicion as Decode: the bytes came from another process. A kind
//  this build does not know is refused, a description without its drive
//  count is refused, and a count past what a Disk ][ card can carry is
//  refused rather than believed.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32IntentChannel::DecodeReply (const Byte * bytes, size_t byteCount, Reply & outReply)
{
    bool    wellFormed = false;
    size_t  textAt     = 1;



    outReply = Reply();

    if (bytes == nullptr || byteCount < 1 || byteCount > kMaxPayloadBytes)
    {
        return false;
    }

    switch ((ReplyKind) bytes[0])
    {
        case ReplyKind::MachineDescription:
            wellFormed = byteCount >= 2 && bytes[1] <= kMaxDriveCount;
            textAt     = 2;

            if (wellFormed)
            {
                outReply.driveCount = bytes[1];
            }

            break;

        case ReplyKind::InsertDone:
        case ReplyKind::InsertRefused:
        case ReplyKind::ReloadDone:
        case ReplyKind::ReloadConflict:
        case ReplyKind::ReloadRefused:
            wellFormed = true;
            break;

        default:
            wellFormed = false;
            break;
    }

    if (!wellFormed)
    {
        outReply = Reply();

        return false;
    }

    outReply.kind = (ReplyKind) bytes[0];
    outReply.text.assign (reinterpret_cast<const char *> (bytes + textAt), byteCount - textAt);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32IntentChannel::SendTo
//
//  Sent rather than posted, for the reason every send here is: the buffer has
//  to outlive the call.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32IntentChannel::SendTo (HWND target, HWND sender, ULONG_PTR messageId, const std::vector<Byte> & bytes)
{
    COPYDATASTRUCT  data      = {};
    DWORD_PTR       result    = 0;
    LRESULT         delivered = 0;
    bool            fits      = !bytes.empty() && bytes.size() <= kMaxPayloadBytes;



    if (target == nullptr || !fits)
    {
        return false;
    }

    data.dwData = messageId;
    data.cbData = (DWORD) bytes.size();
    data.lpData = (void *) bytes.data();

    delivered = SendMessageTimeoutW (target, WM_COPYDATA, (WPARAM) sender,
                                     reinterpret_cast<LPARAM> (&data),
                                     SMTO_ABORTIFHUNG | SMTO_NORMAL,
                                     kSendTimeoutMs, &result);

    return delivered != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32IntentChannel::Encode
//
//  Intent first, then the path.
//
//  THE INTENT LEADS BECAUSE IT IS FIXED-WIDTH. A reader can take one byte,
//  check it, and treat everything after it as the path without needing a
//  length or a terminator -- and `cbData` already says where the path ends.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> Win32IntentChannel::Encode (const std::string & imagePath, ExternalChangeIntent intent)
{
    std::vector<Byte>  bytes;



    bytes.push_back ((Byte) intent);
    bytes.insert (bytes.end(), imagePath.begin(), imagePath.end());

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32IntentChannel::Decode
//
//  Reads a payload another process sent, refusing anything malformed.
//
//  EVERY CHECK HERE IS ABOUT A MESSAGE THIS CHANNEL DID NOT SEND. Any process
//  on the desktop can address a WM_COPYDATA at this window, so nothing about the
//  bytes may be assumed: not that there are any, not that the length is
//  plausible, not that the first byte names an intent this build knows.
//
//  AN EMPTY PATH IS REFUSED rather than accepted as a change to nothing. It
//  would match no mounted bay anyway, and refusing it here means the rule is
//  stated once instead of relied upon downstream.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32IntentChannel::Decode (const Byte * bytes, size_t byteCount, Payload & outPayload)
{
    bool    wellFormed = false;
    size_t  pathAt     = 1;



    outPayload = Payload();

    //  At least the intent byte, and not more than a path could plausibly be.
    if (bytes == nullptr || byteCount < 1 || byteCount > kMaxPayloadBytes)
    {
        return false;
    }

    switch ((ExternalChangeIntent) bytes[0])
    {
    case ExternalChangeIntent::Unstated:
    case ExternalChangeIntent::ReloadInPlace:
    case ExternalChangeIntent::Restart:
        outPayload.intent = (ExternalChangeIntent) bytes[0];
        wellFormed        = true;
        break;

    case ExternalChangeIntent::InsertDisk:
        //  A drive byte of 1 or 2 and then the path.
        outPayload.intent = ExternalChangeIntent::InsertDisk;
        wellFormed        = byteCount >= 2 && (bytes[1] == 1 || bytes[1] == 2);
        pathAt            = 2;

        if (wellFormed)
        {
            outPayload.drive = bytes[1];
        }

        break;

    case ExternalChangeIntent::DescribeMachine:
        //  The one intent that carries no path, and so the one exactly one
        //  byte long.
        if (byteCount != 1)
        {
            return false;
        }

        outPayload.intent = ExternalChangeIntent::DescribeMachine;

        return true;

    default:
        //  A value this build does not know. Reading the rest would be reading
        //  a message meant for something else.
        wellFormed = false;
        break;
    }

    if (wellFormed)
    {
        wellFormed = byteCount > pathAt;
    }

    if (wellFormed)
    {
        outPayload.imagePath.assign (reinterpret_cast<const char *> (bytes + pathAt),
                                     byteCount - pathAt);

        //  A path of nothing but padding is not a path.
        wellFormed = !outPayload.imagePath.empty()
                  && outPayload.imagePath.find_first_not_of (' ') != std::string::npos;
    }

    if (!wellFormed)
    {
        outPayload = Payload();
    }

    return wellFormed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SendToOneWindow
//
//  Hands one emulator window the payload, or gives up on it.
//
////////////////////////////////////////////////////////////////////////////////

struct IntentBroadcast
{
    COPYDATASTRUCT  data   = {};
    HWND            sender = nullptr;
};

static BOOL CALLBACK SendToOneWindow (HWND window, LPARAM context)
{
    wchar_t                  className[64] = {};
    const IntentBroadcast *  broadcast     = reinterpret_cast<const IntentBroadcast *> (context);
    DWORD_PTR                result        = 0;
    LRESULT                  delivered     = 0;



    if (GetClassNameW (window, className, (int) std::size (className)) == 0)
    {
        return TRUE;
    }

    if (wcscmp (className, Win32IntentChannel::kWindowClass) != 0)
    {
        return TRUE;
    }

    //  Sent rather than posted: the buffer this points at has to outlive the
    //  call, and posting returns before the receiver has read it. The timeout
    //  is what keeps a wedged emulator from wedging a build with it.
    //
    //  A FAILED SEND IS NOT REPORTED ANYWHERE, and that is the contract: an
    //  emulator that did not take the hint falls back to asking, which is
    //  correct behavior rather than an error a build should care about.
    delivered = SendMessageTimeoutW (window, WM_COPYDATA, (WPARAM) broadcast->sender,
                                     reinterpret_cast<LPARAM> (&broadcast->data),
                                     SMTO_ABORTIFHUNG | SMTO_NORMAL,
                                     Win32IntentChannel::kSendTimeoutMs, &result);

    IGNORE_RETURN_VALUE (delivered, 0);

    return TRUE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32IntentChannel::StateIntent
//
//  Tells every emulator on the desktop what this write meant.
//
//  EVERY ONE OF THEM, AND EACH IGNORES WHAT IT HAS NOT MOUNTED. That is what
//  makes several running emulators need no discovery protocol, no addressing
//  and no registry: the intent attaches to the image, and an emulator holding a
//  different image simply finds nothing to match.
//
//  NO EMULATOR RUNNING IS NOT AN ERROR, and this is where that rule lives. The
//  enumeration finds nothing, nothing is sent, and the function returns exactly
//  as it does after a successful send -- because it returns nothing either way.
//  A build script cannot know whether the developer has the emulator open, and
//  must behave the same regardless.
//
////////////////////////////////////////////////////////////////////////////////

void Win32IntentChannel::StateIntent (const std::string & imagePath, ExternalChangeIntent intent)
{
    std::vector<Byte>  bytes      = Encode (imagePath, intent);
    IntentBroadcast    broadcast;
    BOOL               enumerated = FALSE;



    if (imagePath.empty() || bytes.size() > kMaxPayloadBytes)
    {
        return;
    }

    broadcast.data.dwData = GetMessageId();
    broadcast.data.cbData = (DWORD) bytes.size();
    broadcast.data.lpData = bytes.data();
    broadcast.sender      = m_sender;

    enumerated = EnumWindows (SendToOneWindow, reinterpret_cast<LPARAM> (&broadcast));

    IGNORE_RETURN_VALUE (enumerated, TRUE);

    return;
}
