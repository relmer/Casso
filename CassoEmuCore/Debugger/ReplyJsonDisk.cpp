#include "Pch.h"

#include "Debugger/ReplyJson.h"

#include "Core/JsonValue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::GetDiskEventName
//
////////////////////////////////////////////////////////////////////////////////

const char * ReplyJson::GetDiskEventName (DiskEvent event)
{
    static constexpr const char * kNames[] =
    {
        "sectorRead", "addressField", "head",       "writeMode",  "writePrologue", "writeBlocked", "dosSector",
        "proDosBlock", "rwtsCall",    "driverCall", "motor",      "driveSelect",   "inserted",     "ejected",
    };



    return kNames[(int) event];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeDiskSpec
//
//  What BPDISK was given; numbers and filters left out match any and are
//  omitted.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeDiskSpec (const DiskBreakSpec & spec, const std::optional<Word> & resolved)
{
    static constexpr const char * kMotors[] = { "any", "on", "off", "stopped" };
    Members                       members;
    std::vector<JsonValue>        options;
    bool                          isCall     = spec.event == DiskEvent::RwtsCall || spec.event == DiskEvent::DriverCall;
    bool                          isPosition = spec.event == DiskEvent::Head || spec.event == DiskEvent::WriteMode;
    const char                  * head       = (spec.head == DiskHeadFilter::Half)  ? "half"
                                             : (spec.head == DiskHeadFilter::Any)   ? "any"
                                             : spec.isWholeTracks                   ? "wholeTracks"
                                             :                                        "range";



    members.emplace_back ("event", MakeString (GetDiskEventName (spec.event)));

    if (spec.drive != DiskDriveFilter::Either)
    {
        members.emplace_back ("drive", MakeNumber ((spec.drive == DiskDriveFilter::Drive1) ? 1 : 2));
    }

    for (const auto & [name, number] : { std::pair { "track", spec.track }, std::pair { "sector", spec.sector }, std::pair { "volume", spec.volume } })
    {
        if (number.has_value())
        {
            members.emplace_back (name, MakeNumber (*number));
        }
    }

    if (spec.block.has_value())
    {
        members.emplace_back ("block", MakeNumber (*spec.block));
    }

    if (isPosition)
    {
        members.emplace_back ("head", MakeString (head));
    }

    if (isPosition && spec.head == DiskHeadFilter::Range)
    {
        members.emplace_back ("quarterTracks", JsonValue (std::vector<JsonValue> { MakeNumber (spec.qtFirst), MakeNumber (spec.qtLast) }));
    }

    if (spec.event == DiskEvent::Motor)
    {
        members.emplace_back ("motor", MakeString (kMotors[(int) spec.motor]));
    }

    if (spec.command.has_value())
    {
        members.emplace_back ("command", MakeNumber (*spec.command));
    }

    for (const auto & [name, isSet] : { std::pair { "passed",      spec.isPassed },
                                        std::pair { "badChecksum", spec.isBadChecksum },
                                        std::pair { "wrongTrack",  spec.isWrongTrack },
                                        std::pair { "bump",        spec.isBump },
                                        std::pair { "every",       spec.isEvery } })
    {
        if (isSet)
        {
            options.push_back (MakeString (name));
        }
    }

    if (!options.empty())
    {
        members.emplace_back ("options", JsonValue (std::move (options)));
    }

    if (spec.addressMarks.has_value())
    {
        members.emplace_back ("addressMarks", MakeMarks (*spec.addressMarks));
        members.emplace_back ("format",       MakeNumber ((spec.format == DiskFieldKind::Thirteen) ? 13 : 16));
    }

    if (spec.dataMarks.has_value())
    {
        members.emplace_back ("dataMarks", MakeMarks (*spec.dataMarks));
    }

    if (isCall && !resolved.has_value())
    {
        members.emplace_back ("unresolved", JsonValue (true));
    }

    return JsonValue (std::move (members));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeMarks
//
//  Each nibble a number, or null for ??.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeMarks (const DiskMarkPattern & marks)
{
    std::vector<JsonValue>  nibbles;



    for (int index = 0; index < marks.GetLength(); index++)
    {
        nibbles.push_back (marks.IsAny (index) ? JsonValue (nullptr) : MakeNumber (marks.GetValue (index)));
    }

    return JsonValue (std::move (nibbles));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeDiskReport
//
//  A disk stop's report; fields that do not apply to the event are omitted.
//  Drives are numbered from 1.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeDiskReport (const DiskReport & report)
{
    static constexpr const char * kMotorEvents[] = { "on", "off", "stopped" };
    Members                       members;
    DiskEvent                     event       = report.event;
    bool                          isWrite     = event == DiskEvent::WriteMode || event == DiskEvent::WritePrologue || event == DiskEvent::WriteBlocked;
    bool                          isLogical   = event == DiskEvent::DosSector || event == DiskEvent::ProDosBlock;
    bool                          isMedia     = event == DiskEvent::Inserted  || event == DiskEvent::Ejected;
    std::vector<JsonValue>        physical;



    members.emplace_back ("event",        MakeString (GetDiskEventName (event)));
    members.emplace_back ("slot",         MakeNumber (report.slot));
    members.emplace_back ("drive",        MakeNumber (report.drive + 1));
    members.emplace_back ("quarterTrack", MakeNumber (report.quarterTrack));
    members.emplace_back ("motorOn",      JsonValue (report.isMotorOn));
    members.emplace_back ("instruction",  MakeNumber (report.instruction));

    if (event == DiskEvent::Head)
    {
        members.emplace_back ("previousQuarterTrack", MakeNumber (report.previousQuarterTrack));
        members.emplace_back ("bump",                 JsonValue (report.isBump));
        members.emplace_back ("phases",               MakeNumber (report.phases));
    }

    if (report.field.has_value())
    {
        members.emplace_back ("field", MakeDiskField (*report.field));
    }

    if (event == DiskEvent::SectorRead)
    {
        members.emplace_back ("epilogue",       JsonValue (std::vector<JsonValue> { MakeNumber (report.epilogue[0]), MakeNumber (report.epilogue[1]), MakeNumber (report.epilogue[2]) }));
        members.emplace_back ("epilogueRandom", JsonValue (report.isEpilogueRandom));
    }

    if (isLogical)
    {
        for (Byte sector : report.physicalSectors)
        {
            physical.push_back (MakeNumber (sector));
        }

        if (report.logicalSector.has_value())
        {
            members.emplace_back ("logicalSector", MakeNumber (*report.logicalSector));
        }

        if (report.block.has_value())
        {
            members.emplace_back ("block", MakeNumber (*report.block));
        }

        members.emplace_back ("physicalSectors", JsonValue (std::move (physical)));
        members.emplace_back ("order",           MakeString ((report.order == DiskSectorOrder::ProDos) ? "proDos" : "dos33"));
    }

    if (isWrite)
    {
        members.emplace_back ("protected", JsonValue (report.isProtected));
    }

    if (report.request.has_value())
    {
        members.emplace_back ("request", MakeDiskRequest (*report.request));
        members.emplace_back ("vector",  MakeNumber (report.vector));
    }

    if (report.motorEvent.has_value())
    {
        members.emplace_back ("motorEvent", MakeString (kMotorEvents[(int) *report.motorEvent]));
    }

    if (event == DiskEvent::DriveSelect)
    {
        members.emplace_back ("previousDrive", MakeNumber (report.previousDrive + 1));
    }

    if (isMedia)
    {
        members.emplace_back ("image", MakeString (std::filesystem::path (report.imageName).filename().string()));
    }

    return JsonValue (std::move (members));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeDiskField
//
//  wrongTrack only for an address field; the data checksums, the data
//  field's instruction and passed only where a data field was read.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeDiskField (const DiskFieldEvent & field)
{
    Members  members;
    bool     isData = field.kind == DiskFieldEventKind::DataChecksum;



    members.emplace_back ("format",           MakeNumber ((field.format == DiskFieldKind::Thirteen) ? 13 : 16));
    members.emplace_back ("track",            MakeNumber (field.track));
    members.emplace_back ("sector",           MakeNumber (field.sector));
    members.emplace_back ("volume",           MakeNumber (field.volume));
    members.emplace_back ("storedChecksum",   MakeNumber (field.storedChecksum));
    members.emplace_back ("expectedChecksum", MakeNumber (field.expectedChecksum));

    if (field.kind == DiskFieldEventKind::AddressField)
    {
        members.emplace_back ("wrongTrack", JsonValue (field.isWrongTrack));
    }

    if (isData)
    {
        members.emplace_back ("storedDataChecksum",   MakeNumber (field.storedDataChecksum));
        members.emplace_back ("expectedDataChecksum", MakeNumber (field.expectedDataChecksum));
        members.emplace_back ("dataChecksumGood",     JsonValue (field.isDataChecksumGood));
    }

    members.emplace_back ("addressInstruction", MakeNumber (field.addressInstruction));

    if (isData)
    {
        members.emplace_back ("dataInstruction", MakeNumber (field.dataInstruction));
        members.emplace_back ("passed",          JsonValue (field.isPassed));
    }

    return JsonValue (std::move (members));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeDiskRequest
//
//  RWTS's from the IOB; the driver's from $42-$47.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeDiskRequest (const DiskCallRequest & request)
{
    Members  members;



    if (!request.isDriver)
    {
        members.emplace_back ("slot",   MakeNumber (request.slot));
        members.emplace_back ("drive",  MakeNumber (request.drive));
        members.emplace_back ("volume", MakeNumber (request.volume));
        members.emplace_back ("track",  MakeNumber (request.track));
        members.emplace_back ("sector", MakeNumber (request.sector));
    }

    members.emplace_back ("command", MakeNumber (request.command));

    if (request.isDriver)
    {
        members.emplace_back ("unit",  MakeNumber (request.unit));
    }

    members.emplace_back ("buffer", MakeNumber (request.buffer));

    if (request.isDriver)
    {
        members.emplace_back ("block", MakeNumber (request.block));
    }

    return JsonValue (std::move (members));
}
