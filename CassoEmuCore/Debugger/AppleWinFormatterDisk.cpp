#include "Pch.h"

#include "Debugger/AppleWinFormatter.h"

#include "Debugger/DiskBreakSpecText.h"





static constexpr int  kQuartersPerTrack = 4;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::DescribeDisk
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleWinFormatter::DescribeDisk (const BreakpointInfo & breakpoint)
{
    DiskBreakSpec  spec   = breakpoint.disk.value_or (DiskBreakSpec {});
    bool           isCall = spec.event == DiskEvent::RwtsCall || spec.event == DiskEvent::DriverCall;
    std::string    text   = DiskBreakSpecText::GetDescription (spec);



    if (isCall)
    {
        text += breakpoint.resolved.has_value() ? std::format (" at ${:04X}", *breakpoint.resolved) : std::string (", unresolved");
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatDiskReport
//
//  The disk report's lines: what happened, the checksums and epilogue of a
//  field or the request of a call, then the drive's state and the
//  instructions responsible. Drives are numbered from 1.
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatDiskReport (const DiskReport & report, Lines & lines)
{
    std::string  detail;



    lines.push_back (std::format ("Disk breakpoint #{}: {}", report.breakpointId, DescribeDiskEvent (report)));

    if (report.field.has_value())
    {
        detail = FormatDiskChecksums (report);
    }

    if (report.request.has_value())
    {
        detail = FormatDiskRequest (report);
    }

    if (!detail.empty())
    {
        lines.push_back ("  " + detail);
    }

    lines.push_back ("  " + FormatDiskState (report));
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::DescribeDiskEvent
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleWinFormatter::DescribeDiskEvent (const DiskReport & report)
{
    static constexpr const char * kMotorEvents[] = { "on", "off", "stopped" };
    std::string                   text           = DiskBreakSpecText::GetEventText (report.event);
    int                           drive          = report.drive + 1;
    DiskFieldEvent                field          = report.field.value_or (DiskFieldEvent {});
    DiskCallRequest               request        = report.request.value_or (DiskCallRequest {});
    const char                  * command        = DiskBreakSpecText::GetCommandWord (report.event, request.command);



    switch (report.event)
    {
    case DiskEvent::SectorRead:
    case DiskEvent::AddressField:
    case DiskEvent::WritePrologue:
        text += std::format (", track ${:02X} ({}) sector ${:02X} volume ${:02X}, drive {}, {}-sector",
                             field.track, field.track, field.sector, field.volume, drive, (field.format == DiskFieldKind::Thirteen) ? 13 : 16);
        break;

    case DiskEvent::DosSector:
        text += std::format (", track ${:02X} ({}) sector ${:02X} (physical ${:02X}) volume ${:02X}, drive {}, DOS 3.3 order assumed",
                             field.track, field.track, report.logicalSector.value_or (0), field.sector, field.volume, drive);
        break;

    case DiskEvent::ProDosBlock:
        text += std::format (" ${:04X}, track ${:02X} ({}) physical sectors ${:02X} and ${:02X}, drive {}",
                             report.block.value_or (0), field.track, field.track,
                             report.physicalSectors.empty() ? 0 : report.physicalSectors.front(),
                             report.physicalSectors.empty() ? 0 : report.physicalSectors.back(), drive);
        text += ", ProDOS order assumed";
        break;

    case DiskEvent::Head:
        text += std::format (", drive {}, quarter track ${:02X} to ${:02X}{}", drive, report.previousQuarterTrack, report.quarterTrack,
                             report.isBump ? ", a bump" : "");
        break;

    case DiskEvent::RwtsCall:
        text += std::format (", track ${:02X} sector ${:02X} {}, drive {}", request.track, request.sector,
                             (command != nullptr) ? command : std::format ("command ${:02X}", request.command), request.drive);
        break;

    case DiskEvent::DriverCall:
        text += std::format (", block ${:04X} {}, unit ${:02X}", request.block,
                             (command != nullptr) ? command : std::format ("command ${:02X}", request.command), request.unit);
        break;

    case DiskEvent::Motor:
        text += std::format (" {}, drive {}", kMotorEvents[(int) report.motorEvent.value_or (DiskMotorEvent::On)], drive);
        break;

    case DiskEvent::DriveSelect:
        text += std::format (", drive {} to drive {}", report.previousDrive + 1, drive);
        break;

    case DiskEvent::Inserted:
    case DiskEvent::Ejected:
        text += std::format (", drive {}: {}", drive, std::filesystem::path (report.imageName).filename().string());
        break;

    default:
        text += std::format (", drive {}{}", drive, report.isProtected ? ", write-protected" : "");
        break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatDiskChecksums
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleWinFormatter::FormatDiskChecksums (const DiskReport & report)
{
    const DiskFieldEvent  & field = *report.field;
    std::string             text  = std::format ("address checksum ${:02X}, expected ${:02X}", field.storedChecksum, field.expectedChecksum);



    if (field.kind == DiskFieldEventKind::AddressField && field.isWrongTrack)
    {
        text += ", wrong track";
    }

    if (field.kind == DiskFieldEventKind::DataChecksum)
    {
        text += std::format ("; data checksum ${:02X}, expected ${:02X}{}", field.storedDataChecksum, field.expectedDataChecksum,
                             field.isPassed ? ", passed over" : "");
    }

    if (report.event == DiskEvent::SectorRead)
    {
        text += std::format ("; epilogue {:02X} {:02X} {:02X}{}", report.epilogue[0], report.epilogue[1], report.epilogue[2],
                             report.isEpilogueRandom ? ", where the drive reads at random" : "");
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatDiskRequest
//
//  The IOB's request for RWTS, $42-$47's for the driver, and the vector the
//  routine was found through.
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleWinFormatter::FormatDiskRequest (const DiskReport & report)
{
    const DiskCallRequest  & request = *report.request;



    if (request.isDriver)
    {
        return std::format ("request: command ${:02X} unit ${:02X} (slot {} drive {}) block ${:04X} buffer ${:04X}; through ${:04X}",
                            request.command, request.unit, request.slot, request.drive, request.block, request.buffer, report.vector);
    }

    return std::format ("request: slot {} drive {} volume ${:02X} track ${:02X} sector ${:02X} command ${:02X} buffer ${:04X}; through ${:04X}",
                        request.slot, request.drive, request.volume, request.track, request.sector, request.command, request.buffer, report.vector);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatDiskState
//
//  The head as a whole track and its quarter track, the motor, and the
//  instructions: a field's reader, or the instruction that made the event.
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleWinFormatter::FormatDiskState (const DiskReport & report)
{
    std::string  text = std::format ("head on track {} (quarter track ${:02X}), motor {}",
                                     report.quarterTrack / kQuartersPerTrack, report.quarterTrack, report.isMotorOn ? "on" : "off");



    if (report.event == DiskEvent::Head)
    {
        text += std::format (", phases {:04b}", report.phases);
    }

    if (report.field.has_value() && report.event == DiskEvent::WritePrologue)
    {
        text += std::format ("; written by ${:04X}", report.instruction);
    }
    else if (report.field.has_value())
    {
        text += std::format ("; read by ${:04X}, address field found by ${:04X}", report.instruction, report.field->addressInstruction);
        text += (report.field->kind == DiskFieldEventKind::DataChecksum) ? std::format (", data field by ${:04X}", report.field->dataInstruction) : std::string();
    }
    else if (report.request.has_value())
    {
        text += std::format ("; routine at ${:04X}", report.instruction);
    }
    else
    {
        text += std::format ("; by ${:04X}", report.instruction);
    }

    return text;
}
