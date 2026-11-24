#include "Pch.h"

#include "Devices/Disk/DiskGetStampCommand.h"

#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Disk/Stamp/FormatStamp.h"
#include "Devices/Disk/Stamp/FormatStampChunk.h"
#include "Devices/Disk/Stamp/FormatStampText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskGetStampCommand::Run
//
//  Reads the image's bytes and its first CASS chunk. With --json the
//  chunk's text is printed as stored, even when this build can't read it.
//
////////////////////////////////////////////////////////////////////////////////

void DiskGetStampCommand::Run (IDiskFileIo & io, const std::string & imagePath, bool isJson, DiskCommandResult & result)
{
    HRESULT            hr       = S_OK;
    std::vector<Byte>  bytes;
    std::string        text;
    StampState         state    = StampState::Absent;
    FormatStamp        stamp;
    bool               isRead   = false;
    bool               isWoz    = false;



    hr     = io.ReadAllBytes (imagePath, bytes);
    isRead = SUCCEEDED (hr);
    CBRF (isRead, result.FailAction (imagePath, "read the image", "the file couldn't be read"));

    hr    = FormatStampChunk::GetStampFromFile (bytes, text, state, stamp);
    isWoz = SUCCEEDED (hr);
    CBRF (isWoz, result.FailAction (imagePath, "read the format stamp", "the file is not a WOZ image"));

    if (state == StampState::Absent)
    {
        result.output     = imagePath + " has no format stamp.\n";
        result.exitStatus = kNoStamp;
    }
    else if (isJson)
    {
        result.output     = text + "\n";
        result.exitStatus = state == StampState::Readable ? DiskCommandResult::kClean : kUnreadable;
    }
    else if (state == StampState::Unreadable)
    {
        result.diagnostics = imagePath + " has a format stamp Casso can't read.\n";
        result.exitStatus  = kUnreadable;
    }
    else
    {
        result.output = FormatStampText::FormatTable (stamp);
    }

Error:
    return;
}
