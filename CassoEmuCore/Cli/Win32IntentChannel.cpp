#include "Pch.h"

#include "Win32IntentChannel.h"





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
//  Win32IntentChannel::Encode
//
//  Intent first, then the path.
//
//  THE INTENT LEADS BECAUSE IT IS FIXED-WIDTH. A reader can take one byte,
//  check it, and treat everything after it as the path without needing a
//  length or a terminator -- and `cbData` already says where the path ends.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> Win32IntentChannel::Encode (const std::string & imagePath, PickUpIntent intent)
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
    bool  wellFormed = false;



    outPayload = Payload();

    //  One byte of intent plus at least one byte of path, and not more than a
    //  path could plausibly be.
    if (bytes == nullptr || byteCount < 2 || byteCount > kMaxPayloadBytes)
    {
        return false;
    }

    switch ((PickUpIntent) bytes[0])
    {
    case PickUpIntent::Unstated:
    case PickUpIntent::TakeUpInPlace:
    case PickUpIntent::Restart:
        outPayload.intent = (PickUpIntent) bytes[0];
        wellFormed        = true;
        break;

    default:
        //  A value this build does not know. Reading the rest would be reading
        //  a message meant for something else.
        wellFormed = false;
        break;
    }

    if (wellFormed)
    {
        outPayload.imagePath.assign (reinterpret_cast<const char *> (bytes + 1),
                                     byteCount - 1);

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

static BOOL CALLBACK SendToOneWindow (HWND window, LPARAM context)
{
    wchar_t                  className[64] = {};
    const COPYDATASTRUCT *   data          = reinterpret_cast<const COPYDATASTRUCT *> (context);
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
    delivered = SendMessageTimeoutW (window, WM_COPYDATA, 0,
                                     reinterpret_cast<LPARAM> (data),
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

void Win32IntentChannel::StateIntent (const std::string & imagePath, PickUpIntent intent)
{
    std::vector<Byte>  bytes      = Encode (imagePath, intent);
    COPYDATASTRUCT     data       = {};
    BOOL               enumerated = FALSE;



    if (imagePath.empty() || bytes.size() > kMaxPayloadBytes)
    {
        return;
    }

    data.dwData = GetMessageId();
    data.cbData = (DWORD) bytes.size();
    data.lpData = bytes.data();

    enumerated = EnumWindows (SendToOneWindow, reinterpret_cast<LPARAM> (&data));

    IGNORE_RETURN_VALUE (enumerated, TRUE);

    return;
}
