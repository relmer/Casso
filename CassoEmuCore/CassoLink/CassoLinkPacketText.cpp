#include "Pch.h"

#include "CassoLink/CassoLinkPacketText.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/CassoLinkHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkPacketText::Describe
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkPacketText::Describe (Byte command, std::span<const Byte> payload)
{
    size_t  size = payload.size();



    switch (command)
    {
        case CassoLinkCodec::kReady:
            if (size >= 11)
            {
                return std::format ("Ready: program version {}, drive map {}, program CRC {:04X}", payload[0] | (payload[1] << 8), payload[3], payload[9] | (payload[10] << 8));
            }

            return (size >= 4) ? std::format ("Ready: program version {}, drive map {}", payload[0] | (payload[1] << 8), payload[3]) : std::string ("Ready");

        case CassoLinkCodec::kIdle:
            return "Idle";

        case CassoLinkCodec::kLoadProgram:
            return (size >= 1) ? std::format ("Load program: {} pages", payload[0]) : std::string ("Load program");

        case CassoLinkCodec::kReadSectors:
            return (size >= 3) ? std::format ("Read sectors: drive {}, track {}, {}", payload[0], payload[1], GetSectors (payload)) : std::string ("Read sectors");

        case CassoLinkCodec::kWriteSectors:
            return (size >= 3) ? std::format ("Write sectors: drive {}, track {}, {}", payload[0], payload[1], GetSectors (payload)) : std::string ("Write sectors");

        case CassoLinkCodec::kReadCrcs:
            return (size >= 3) ? std::format ("Read sector checksums: drive {}, track {}, {}", payload[0], payload[1], GetSectors (payload)) : std::string ("Read sector checksums");

        case CassoLinkCodec::kSectorData:
            return (size >= 2) ? std::format ("Sector data: track {}, sector {}", payload[0], payload[1]) : std::string ("Sector data");

        case CassoLinkCodec::kSectorNibbles:
            return (size >= 2) ? std::format ("Sector nibbles: track {}, sector {}", payload[0], payload[1]) : std::string ("Sector nibbles");

        case CassoLinkCodec::kSectorBlank:
            return (size >= 2) ? std::format ("Blank sector: track {}, sector {}", payload[0], payload[1]) : std::string ("Blank sector");

        case CassoLinkCodec::kSectorCrcs:
            return std::format ("Sector checksums: {} bytes", size);

        case CassoLinkCodec::kStatus:
            return (size >= 5) ? std::format ("Status: drive {}, track {}, {} sectors done, {}", payload[1], payload[2], payload[3], GetErrorText (payload[4])) : std::string ("Status");

        case CassoLinkCodec::kFormatTrack:
            return (size >= 3) ? std::format ("Format track: drive {}, track {}, volume {}", payload[0], payload[1], payload[2]) : std::string ("Format track");

        case CassoLinkCodec::kWriteTrack:
            return (size >= 2) ? std::format ("Write track: drive {}, track {}", payload[0], payload[1]) : std::string ("Write track");

        case CassoLinkCodec::kTimeDrive:
            return (size >= 3) ? std::format ("Time drive: drive {}, track {}, {} revolutions", payload[0], payload[1], payload[2]) : std::string ("Time drive");

        case CassoLinkCodec::kSetSpeed:
            return (size >= 1) ? std::format ("Set speed: control {:02X}", payload[0]) : std::string ("Set speed");

        case CassoLinkCodec::kSetColumns:
            return (size >= 1) ? std::format ("Set columns: {}", payload[0] != 0 ? 80 : 40) : std::string ("Set columns");

        case CassoLinkCodec::kCancel:
            return "Cancel";

        case CassoLinkCodec::kExit:
            return "Exit";

        case CassoLinkCodec::kLabel:
            return "Label: " + GetText (payload);

        case CassoLinkCodec::kLog:
            return (size >= 2) ? std::format ("History line at {:02}:{:02}: {}", payload[0], payload[1], GetText (payload.subspan (2))) : std::string ("History line");

        case CassoLinkCodec::kSessionScreen:
            if (size >= 2 && payload[0] == 0)
            {
                return "Full Send screen cleared";
            }

            return (size >= 2) ? std::format ("Full Send screen, tone {}: {}", payload[1], GetText (payload.subspan (2))) : std::string ("Full Send screen");

        case CassoLinkCodec::kWaitForKey:
            if (size == 1)
            {
                return "Key: " + GetKeyName (payload[0]);
            }

            return (size >= 1) ? std::format ("Wait for {}: {}", payload[0] == CassoLinkCodec::kWaitTyped ? "any key" : "a session key", GetText (payload.subspan (1))) : std::string ("Wait for a key");

        default:
            return std::format ("Command {:02X}, {} bytes", command, size);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkPacketText::GetKeyName
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkPacketText::GetKeyName (Byte key)
{
    switch (key)
    {
        case CassoLinkCodec::kKeyNone:   return "none";
        case CassoLinkCodec::kKeySpace:  return "Space";
        case CassoLinkCodec::kKeyReturn: return "Return";
        case CassoLinkCodec::kKeyEscape: return "Esc";
        case CassoLinkCodec::kKeyLeft:   return "Left arrow";
        case CassoLinkCodec::kKeyDelete: return "Delete";
        default:                         break;
    }

    if ((key & 0x7F) > ' ' && (key & 0x7F) < 0x7F)
    {
        return std::string (1, (char) (key & 0x7F));
    }

    return std::format ("code {:02X}", key);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkPacketText::GetErrorText
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkPacketText::GetErrorText (Byte error)
{
    switch (error)
    {
        case CassoLinkHost::kErrorNone:           return "no error";
        case CassoLinkHost::kErrorWriteProtected: return "write-protected";
        case CassoLinkHost::kErrorNotFound:       return "sector not found";
        case CassoLinkHost::kErrorData:           return "bad data checksum";
        case CassoLinkHost::kErrorVerify:         return "read back differently";
        case CassoLinkHost::kErrorLink:           return "link error";
        case CassoLinkHost::kErrorDrive:          return "no disk in the drive";
        default:                                  return std::format ("error {:02X}", error);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkPacketText::GetSectors
//
//  A request's sector list: its count at payload[2] and the sectors after.
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkPacketText::GetSectors (std::span<const Byte> payload)
{
    size_t       count = payload[2];
    std::string  text  = (count == 1) ? "sector " : "sectors ";



    for (size_t at = 0; at < count && 3 + at < payload.size(); at++)
    {
        text += std::format ("{}{}", (at == 0) ? "" : ", ", payload[3 + at]);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkPacketText::GetText
//
//  The text in quotes; each line after the first follows a slash.
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkPacketText::GetText (std::span<const Byte> text)
{
    std::string  line = "\"";
    bool         gap  = false;



    for (Byte b : text)
    {
        char  ch = (char) (b & 0x7F);

        if (ch == 0)
        {
            gap = true;
            continue;
        }

        if (gap && line.size() > 1)
        {
            line += " / ";
        }

        gap   = false;
        line += (ch >= ' ' && ch < 0x7F) ? ch : '?';
    }

    return line + "\"";
}
