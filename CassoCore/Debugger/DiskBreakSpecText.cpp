#include "Pch.h"

#include "Debugger/DiskBreakSpecText.h"





static constexpr int  kQuartersPerTrack = 4;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakSpecText::FormatArguments
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskBreakSpecText::FormatArguments (const DiskBreakSpec & spec)
{
    std::string   text;
    const char  * command = spec.command.has_value() ? GetCommandWord (spec.event, *spec.command) : nullptr;



    if (spec.drive != DiskDriveFilter::Either)
    {
        text = (spec.drive == DiskDriveFilter::Drive1) ? "D1 " : "D2 ";
    }

    switch (spec.event)
    {
    case DiskEvent::SectorRead:     text += "READ";    AddNumbers ({ spec.track, spec.sector }, text);                          break;
    case DiskEvent::AddressField:   text += "ADDRESS"; AddFields  (spec, text);                                                   break;
    case DiskEvent::Head:           text += "HEAD";    AddRange   (spec, text);                                                   break;
    case DiskEvent::WriteMode:      text += "WRITE";   AddRange   (spec, text);                                                   break;
    case DiskEvent::WritePrologue:  text += "WRITE";   AddNumbers ({ spec.track, spec.sector }, text);                          break;
    case DiskEvent::WriteBlocked:   text += "WRITE BLOCKED";                                                                      break;
    case DiskEvent::DosSector:      text += "DOS";     AddNumbers ({ spec.track, spec.sector }, text);                          break;
    case DiskEvent::ProDosBlock:    text += std::format ("BLOCK {:X}", spec.block.value_or (0));                                  break;
    case DiskEvent::RwtsCall:       text += "RWTS";    AddFields  (spec, text);                                                   break;
    case DiskEvent::DriverCall:     text += "DRIVER";  text += spec.block.has_value() ? std::format (" BLOCK {:X}", *spec.block) : std::string();  break;
    case DiskEvent::Motor:          text += "MOTOR";                                                                              break;
    case DiskEvent::DriveSelect:    text += "SELECT";                                                                             break;
    case DiskEvent::Inserted:       text += "INSERT";                                                                             break;
    case DiskEvent::Ejected:        text += "EJECT";                                                                              break;
    }

    text += (command != nullptr) ? std::format (" {}", command) : std::string();

    switch (spec.motor)
    {
    case DiskMotorFilter::On:       text += " ON";      break;
    case DiskMotorFilter::Off:      text += " OFF";     break;
    case DiskMotorFilter::Stopped:  text += " STOPPED"; break;
    case DiskMotorFilter::Any:                          break;
    }

    text += spec.isPassed      ? " PASSED"     : "";
    text += spec.isBadChecksum ? " BADSUM"     : "";
    text += spec.isWrongTrack  ? " WRONGTRACK" : "";
    text += spec.isBump        ? " BUMP"       : "";
    text += spec.isEvery       ? " EVERY"      : "";

    AddMarks (spec, text);

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakSpecText::AddNumbers
//
//  Each number in hex after a space; a sector read, a written prologue and a
//  DOS sector take both, a star standing for any.
//
////////////////////////////////////////////////////////////////////////////////

void DiskBreakSpecText::AddNumbers (std::initializer_list<std::optional<Byte>> numbers, std::string & text)
{
    for (const std::optional<Byte> & number : numbers)
    {
        text += number.has_value() ? std::format (" {:X}", *number) : std::string (" *");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakSpecText::AddFields
//
//  ADDRESS's and RWTS's track, sector and volume: each that is given, with a
//  star for one left out before a later one.
//
////////////////////////////////////////////////////////////////////////////////

void DiskBreakSpecText::AddFields (const DiskBreakSpec & spec, std::string & text)
{
    std::array<std::optional<Byte>, 3>  fields = { spec.track, spec.sector, spec.volume };
    size_t                              count  = fields.size();



    while (count > 0 && !fields[count - 1].has_value())
    {
        count--;
    }

    for (size_t i = 0; i < count; i++)
    {
        text += fields[i].has_value() ? std::format (" {:X}", *fields[i]) : std::string (" *");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakSpecText::AddRange
//
//  HEAD's and WRITE's position: QT first[:last], T first[:last] for whole
//  tracks, or HALF.
//
////////////////////////////////////////////////////////////////////////////////

void DiskBreakSpecText::AddRange (const DiskBreakSpec & spec, std::string & text)
{
    int  divisor = spec.isWholeTracks ? kQuartersPerTrack : 1;
    int  first   = spec.qtFirst / divisor;
    int  last    = spec.qtLast  / divisor;



    if (spec.head == DiskHeadFilter::Half)
    {
        text += " HALF";
    }
    else if (spec.head == DiskHeadFilter::Range)
    {
        text += std::format (" {} {:X}", spec.isWholeTracks ? "T" : "QT", first);
        text += (last != first) ? std::format (":{:X}", last) : std::string();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakSpecText::AddMarks
//
////////////////////////////////////////////////////////////////////////////////

void DiskBreakSpecText::AddMarks (const DiskBreakSpec & spec, std::string & text)
{
    text += spec.addressMarks.has_value() ? " ADDR " + spec.addressMarks->ToText() : std::string();
    text += spec.dataMarks.has_value()    ? " DATA " + spec.dataMarks->ToText()    : std::string();
    text += (spec.format == DiskFieldKind::Thirteen) ? " SECTORS 13" : "";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakSpecText::GetDescription
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskBreakSpecText::GetDescription (const DiskBreakSpec & spec)
{
    auto          number  = [] (const char * label, const std::optional<Byte> & value) { return value.has_value() ? std::format ("{} ${:02X}", label, *value) : std::format ("any {}", label); };
    std::string   text    = GetEventText (spec.event);
    const char  * command = spec.command.has_value() ? GetCommandWord (spec.event, *spec.command) : nullptr;



    switch (spec.event)
    {
    case DiskEvent::SectorRead:
    case DiskEvent::WritePrologue:
    case DiskEvent::DosSector:
        text += ", " + number ("track", spec.track) + " " + number ("sector", spec.sector);
        break;

    case DiskEvent::AddressField:
    case DiskEvent::RwtsCall:
        text += spec.track.has_value()  ? std::format (", track ${:02X}",  *spec.track)  : std::string();
        text += spec.sector.has_value() ? std::format (" sector ${:02X}",  *spec.sector) : std::string();
        text += spec.volume.has_value() ? std::format (" volume ${:02X}",  *spec.volume) : std::string();
        break;

    case DiskEvent::Head:
    case DiskEvent::WriteMode:
        text += FormatRange (spec);
        break;

    case DiskEvent::ProDosBlock:
    case DiskEvent::DriverCall:
        text += spec.block.has_value() ? std::format (", block ${:04X}", *spec.block) : std::string();
        break;

    default:
        break;
    }

    if (command != nullptr)
    {
        text += ", ";

        for (const char * c = command; *c != 0; c++)
        {
            text += (char) tolower ((unsigned char) *c);
        }
    }

    switch (spec.motor)
    {
    case DiskMotorFilter::On:       text += ", on";      break;
    case DiskMotorFilter::Off:      text += ", off";     break;
    case DiskMotorFilter::Stopped:  text += ", stopped"; break;
    case DiskMotorFilter::Any:                           break;
    }

    text += spec.isBadChecksum ? ", bad checksum"        : "";
    text += spec.isWrongTrack  ? ", wrong track"         : "";
    text += spec.isBump        ? ", bump"                : "";
    text += spec.isEvery       ? ", every access"        : "";
    text += (spec.drive == DiskDriveFilter::Drive1) ? ", drive 1" : (spec.drive == DiskDriveFilter::Drive2) ? ", drive 2" : "";
    text += spec.isPassed      ? ", fields passed over too" : "";
    text += spec.addressMarks.has_value() ? ", address marks " + spec.addressMarks->ToText() : std::string();
    text += spec.dataMarks.has_value()    ? ", data marks "    + spec.dataMarks->ToText()    : std::string();
    text += (spec.format == DiskFieldKind::Thirteen) ? ", 13-sector" : "";

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakSpecText::FormatRange
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskBreakSpecText::FormatRange (const DiskBreakSpec & spec)
{
    int          divisor = spec.isWholeTracks ? kQuartersPerTrack : 1;
    int          first   = spec.qtFirst / divisor;
    int          last    = spec.qtLast  / divisor;
    const char * unit    = spec.isWholeTracks ? "track" : "quarter track";
    std::string  text;



    if (spec.head == DiskHeadFilter::Half)
    {
        text = ", half tracks";
    }
    else if (spec.head == DiskHeadFilter::Range)
    {
        text = (last != first) ? std::format (", {}s ${:02X}-${:02X}", unit, first, last) : std::format (", {} ${:02X}", unit, first);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakSpecText::GetTriggerText
//
//  FR-163's words, in the order they sort after Interrupt.
//
////////////////////////////////////////////////////////////////////////////////

const char * DiskBreakSpecText::GetTriggerText (DiskEvent event)
{
    switch (event)
    {
    case DiskEvent::SectorRead:     return "Sector read";
    case DiskEvent::AddressField:   return "Address field";
    case DiskEvent::Head:           return "Head";
    case DiskEvent::WriteMode:
    case DiskEvent::WritePrologue:
    case DiskEvent::WriteBlocked:   return "Write";
    case DiskEvent::DosSector:      return "DOS sector";
    case DiskEvent::ProDosBlock:    return "ProDOS block";
    case DiskEvent::RwtsCall:       return "RWTS call";
    case DiskEvent::DriverCall:     return "Driver call";
    case DiskEvent::Motor:          return "Motor";
    case DiskEvent::DriveSelect:    return "Drive select";
    case DiskEvent::Inserted:       return "Insert";
    case DiskEvent::Ejected:        return "Eject";
    default:                        return "";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakSpecText::GetEventText
//
////////////////////////////////////////////////////////////////////////////////

const char * DiskBreakSpecText::GetEventText (DiskEvent event)
{
    switch (event)
    {
    case DiskEvent::SectorRead:     return "sector read";
    case DiskEvent::AddressField:   return "address field";
    case DiskEvent::Head:           return "head";
    case DiskEvent::WriteMode:      return "write mode";
    case DiskEvent::WritePrologue:  return "written data prologue";
    case DiskEvent::WriteBlocked:   return "blocked write";
    case DiskEvent::DosSector:      return "DOS 3.3 sector";
    case DiskEvent::ProDosBlock:    return "ProDOS block";
    case DiskEvent::RwtsCall:       return "RWTS call";
    case DiskEvent::DriverCall:     return "driver call";
    case DiskEvent::Motor:          return "motor";
    case DiskEvent::DriveSelect:    return "drive select";
    case DiskEvent::Inserted:       return "disk inserted";
    case DiskEvent::Ejected:        return "disk ejected";
    default:                        return "";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakSpecText::GetCommandWord
//
//  RWTS's IOB commands: 0 seek, 1 read, 2 write, 4 format. ProDOS's driver
//  commands: 0 status, 1 read, 2 write, 3 format.
//
////////////////////////////////////////////////////////////////////////////////

const char * DiskBreakSpecText::GetCommandWord (DiskEvent event, Byte command)
{
    static constexpr const char * kRwts[]   = { "SEEK",   "READ", "WRITE", nullptr,  "FORMAT" };
    static constexpr const char * kDriver[] = { "STATUS", "READ", "WRITE", "FORMAT" };
    const char                  * word      = nullptr;



    if (event == DiskEvent::RwtsCall && command < std::size (kRwts))
    {
        word = kRwts[command];
    }
    else if (event == DiskEvent::DriverCall && command < std::size (kDriver))
    {
        word = kDriver[command];
    }

    return word;
}
