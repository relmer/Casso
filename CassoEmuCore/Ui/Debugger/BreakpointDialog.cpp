#include "Pch.h"

#include "Ui/Debugger/BreakpointDialog.h"
#include "Debugger/DiskBreakSpecText.h"
#include "Debugger/Source/SourcePathList.h"



static constexpr const wchar_t *  s_kDiskEventNames[] =
{
    L"Sector read", L"Address field", L"Head position", L"Write", L"DOS 3.3 sector", L"ProDOS block",
    L"RWTS call",   L"ProDOS driver call", L"Motor", L"Drive select", L"Disk inserted", L"Disk ejected",
};

static constexpr const char *  s_kDrivePrefixes[] = { "", "D1 ", "D2 " };





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialogPanel::Add
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointDialogPanel::Add (DxuiLabel & label, IDxuiControl & field, DxuiInfoTip * tip)
{
    m_rows.push_back ({ &label, &field, tip });
    Adopt (label);
    Adopt (field);

    if (tip != nullptr)
    {
        Adopt (*tip);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialogPanel::Layout
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointDialogPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int  row   = scaler.ToPx (kRowDip);
    int  gap   = scaler.ToPx (kGapDip);
    int  split = boundsDip.left + scaler.ToPx (kLabelDip);
    int  tip   = scaler.ToPx (kTipDip);
    int  y     = boundsDip.top;



    SetBounds (boundsDip);

    for (const Row & each : m_rows)
    {
        int  labelRight = (each.tip != nullptr) ? split - tip : split;

        each.label->SetVisible (each.field->IsVisible());

        if (each.tip != nullptr)
        {
            each.tip->SetVisible (each.field->IsVisible());
        }

        if (!each.field->IsVisible())
        {
            continue;
        }

        each.label->Layout (RECT { boundsDip.left, y, labelRight, y + row }, scaler);
        each.field->Layout (RECT { split, y, boundsDip.right, y + row }, scaler);

        if (each.tip != nullptr)
        {
            each.tip->SetDpi (scaler.GetDpi());
            each.tip->Layout (RECT { labelRight, y, split, y + row }, scaler);
        }

        y += row + gap;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::GetTypeRows
//
//  The types under headings: execution, memory accesses, BASIC's, then the
//  machine's own. A memory address is ReadWrite here; its checks choose.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<BreakpointDialog::TypeRow> & BreakpointDialog::GetTypeRows()
{
    static const std::vector<TypeRow>  rows =
    {
        { L"Execution",                    std::nullopt              },
        { L"Address",                      Type::Execute             },
        { L"Condition",                    Type::Condition           },
        { L"Opcode",                       Type::Opcode              },
        { L"BRK",                          Type::Brk                 },
        { L"BRK on interrupt",             Type::Interrupt           },
        { L"Memory",                       std::nullopt              },
        { L"Address",                      Type::ReadWrite           },
        { L"Value",                        Type::Value               },
        { L"Reading uninitialized memory", Type::UnwrittenRead       },
        { L"BASIC",                        std::nullopt              },
        { L"Line",                         Type::BasicLine           },
        { L"Variable change",              Type::BasicVariable       },
        { L"Error",                        Type::BasicError          },
        { L"Hardware",                     std::nullopt              },
        { L"Disk event",                   Type::Disk                },
        { L"Video line",                   Type::VideoLine           },
        { L"Beam position",                Type::Beam                },
    };



    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::GetRowOf
//
////////////////////////////////////////////////////////////////////////////////

int BreakpointDialog::GetRowOf (Type type)
{
    const std::vector<TypeRow>  & rows = GetTypeRows();



    Type                          shown = (type == Type::Read || type == Type::Write) ? Type::ReadWrite : type;



    for (size_t i = 0; i < rows.size(); i++)
    {
        if (rows[i].type == shown)
        {
            return (int) i;
        }
    }

    return 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::GetTitle
//
//  The type's heading and label, from the New drop-down's rows.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BreakpointDialog::GetTitle (Type type, std::optional<int> id)
{
    const std::vector<TypeRow>  & rows    = GetTypeRows();
    int                           row     = GetRowOf (type);
    std::wstring                  heading;



    for (int i = row; i >= 0; i--)
    {
        if (!rows[(size_t) i].type.has_value())
        {
            heading = rows[(size_t) i].label;
            break;
        }
    }

    if (id.has_value())
    {
        return std::format (L"{} breakpoint #{}: {}", heading, *id, rows[(size_t) row].label);
    }

    //  BASIC is an acronym; the other headings are words.
    if (!heading.empty() && heading != L"BASIC")
    {
        heading[0] = (wchar_t) towlower (heading[0]);
    }

    return std::format (L"New {} breakpoint: {}", heading, rows[(size_t) row].label);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::GetDiskFieldSet
//
//  What each event's arguments are, as BPDISK's syntax lines have them:
//  numbers for a sector, a block or an RWTS call, a range or HALF for a head
//  position, a kind of write, a call's command, the motor's state, and the
//  checks for PASSED, BADSUM, WRONGTRACK, BUMP and EVERY.
//
////////////////////////////////////////////////////////////////////////////////

BreakpointDialog::DiskFieldSet BreakpointDialog::GetDiskFieldSet (int event)
{
    DiskFieldSet  fieldSet;
    std::string   word     = DiskBreakSpecText::GetEventWords()[std::clamp<size_t> (event, 0, DiskBreakSpecText::GetEventWords().size() - 1)].word;



    if (word == "READ" || word == "DOS")
    {
        fieldSet.trackLabel  = L"Track";
        fieldSet.sectorLabel = L"Sector";
        fieldSet.isPassed    = true;
        fieldSet.isAdvanced  = word == "READ";
    }
    else if (word == "ADDRESS")
    {
        fieldSet.trackLabel    = L"Track";
        fieldSet.sectorLabel   = L"Sector";
        fieldSet.numberLabel   = L"Volume";
        fieldSet.isBadChecksum = true;
        fieldSet.isWrongTrack  = true;
        fieldSet.isAdvanced    = true;
    }
    else if (word == "HEAD")
    {
        fieldSet.choiceLabel = L"Position";
        fieldSet.choices     = { L"Any position", L"Tracks", L"Quarter tracks", L"Any half track" };
        fieldSet.trackLabel  = L"Tracks";
        fieldSet.isRange     = true;
        fieldSet.isBump      = true;
    }
    else if (word == "WRITE")
    {
        fieldSet.choiceLabel = L"Write";
        fieldSet.choices     = { L"Any write", L"Sector", L"Tracks", L"Quarter tracks", L"Blocked by write protection" };
        fieldSet.trackLabel  = L"Track";
        fieldSet.sectorLabel = L"Sector";
        fieldSet.isAdvanced  = true;
    }
    else if (word == "BLOCK")
    {
        fieldSet.numberLabel = L"Block";
        fieldSet.isPassed    = true;
    }
    else if (word == "RWTS")
    {
        fieldSet.trackLabel  = L"Track";
        fieldSet.sectorLabel = L"Sector";
        fieldSet.choiceLabel = L"Command";
        fieldSet.choices     = { L"Any command", L"Read", L"Write", L"Seek", L"Format" };
    }
    else if (word == "DRIVER")
    {
        fieldSet.numberLabel = L"Block";
        fieldSet.choiceLabel = L"Command";
        fieldSet.choices     = { L"Any command", L"Read", L"Write", L"Status", L"Format" };
    }
    else if (word == "MOTOR")
    {
        fieldSet.choiceLabel = L"Motor";
        fieldSet.choices     = { L"Any change", L"On", L"Off", L"Stopped" };
    }
    else if (word == "SELECT")
    {
        fieldSet.isEvery = true;
    }

    return fieldSet;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::MakeDiskArguments
//
//  The fields in the order BPDISK's syntax line has them. A field the event
//  does not have, or that its drop-down does not use, is not written.
//
////////////////////////////////////////////////////////////////////////////////

std::string BreakpointDialog::MakeDiskArguments (int event, const DiskFields & fields)
{
    static constexpr const char *  kRwtsCommands[]   = { "", "READ", "WRITE", "SEEK", "FORMAT" };
    static constexpr const char *  kDriverCommands[] = { "", "READ", "WRITE", "STATUS", "FORMAT" };
    static constexpr const char *  kMotorStates[]    = { "", "ON", "OFF", "STOPPED" };
    std::string                    word   = DiskBreakSpecText::GetEventWords()[std::clamp<size_t> (event, 0, DiskBreakSpecText::GetEventWords().size() - 1)].word;
    std::vector<std::string>       parts;
    int                            choice = std::max (fields.choice, 0);
    auto                           add    = [&parts] (const std::string & part) { if (!part.empty()) { parts.push_back (part); } };
    std::string                    text;



    if (word == "READ" || word == "DOS" || word == "ADDRESS" || word == "RWTS")
    {
        add (fields.track);
        add (fields.track.empty() ? std::string() : fields.sector);
        add (word == "ADDRESS" && !fields.sector.empty() ? fields.number : std::string());
    }

    if (word == "BLOCK")
    {
        add (fields.number);
    }

    if (word == "DRIVER" && !fields.number.empty())
    {
        add ("BLOCK " + fields.number);
    }

    if (word == "HEAD")
    {
        add ((choice == 1 && !fields.track.empty()) ? "T " + fields.track
           : (choice == 2 && !fields.track.empty()) ? "QT " + fields.track
           : (choice == 3)                          ? std::string ("HALF")
           :                                          std::string());
    }

    if (word == "WRITE")
    {
        add ((choice == 1 && !fields.track.empty()) ? fields.track + (fields.sector.empty() ? std::string() : " " + fields.sector)
           : (choice == 2 && !fields.track.empty()) ? "T " + fields.track
           : (choice == 3 && !fields.track.empty()) ? "QT " + fields.track
           : (choice == 4)                          ? std::string ("BLOCKED")
           :                                          std::string());
    }

    add (word == "RWTS"   ? kRwtsCommands  [std::min (choice, 4)] : "");
    add (word == "DRIVER" ? kDriverCommands[std::min (choice, 4)] : "");
    add (word == "MOTOR"  ? kMotorStates   [std::min (choice, 3)] : "");

    add (fields.isPassed      && GetDiskFieldSet (event).isPassed      ? "PASSED"     : "");
    add (fields.isBadChecksum && GetDiskFieldSet (event).isBadChecksum ? "BADSUM"     : "");
    add (fields.isWrongTrack  && GetDiskFieldSet (event).isWrongTrack  ? "WRONGTRACK" : "");
    add (fields.isBump        && GetDiskFieldSet (event).isBump        ? "BUMP"       : "");
    add (fields.isEvery       && GetDiskFieldSet (event).isEvery       ? "EVERY"      : "");
    add (GetDiskFieldSet (event).isAdvanced && (word != "WRITE" || choice == 1) ? fields.advanced : std::string());

    for (const std::string & part : parts)
    {
        text += (text.empty() ? "" : " ") + part;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::ParseDiskArguments
//
//  BPDISK's arguments back into the fields, word by word: the numbers in
//  the order the event takes them, each keyword's check or choice, and ADDR,
//  DATA and SECTORS with what follows them as the advanced text.
//
////////////////////////////////////////////////////////////////////////////////

BreakpointDialog::DiskFields BreakpointDialog::ParseDiskArguments (int event, const std::string & arguments)
{
    DiskFields                fields;
    std::istringstream        stream (arguments);
    std::vector<std::string>  words;
    std::string               word;
    std::string               name = DiskBreakSpecText::GetEventWords()[std::clamp<size_t> (event, 0, DiskBreakSpecText::GetEventWords().size() - 1)].word;
    std::vector<std::string>  numbers;
    auto                      upper  = [] (std::string text) { std::ranges::transform (text, text.begin(), [] (char ch) { return (char) std::toupper ((unsigned char) ch); }); return text; };



    while (stream >> word)
    {
        words.push_back (word);
    }

    for (size_t i = 0; i < words.size(); i++)
    {
        std::string  key = upper (words[i]);

        if (key == "ADDR" || key == "DATA" || key == "SECTORS")
        {
            size_t  count = (key == "SECTORS") ? 1 : 3;

            fields.advanced += (fields.advanced.empty() ? "" : " ") + words[i];

            for (size_t k = 0; k < count && i + 1 < words.size(); k++)
            {
                fields.advanced += " " + words[++i];
            }
        }
        else if (key == "PASSED")     { fields.isPassed      = true; }
        else if (key == "BADSUM")     { fields.isBadChecksum = true; }
        else if (key == "WRONGTRACK") { fields.isWrongTrack  = true; }
        else if (key == "BUMP")       { fields.isBump        = true; }
        else if (key == "EVERY")      { fields.isEvery       = true; }
        else if (key == "HALF")       { fields.choice        = 3; }
        else if (key == "BLOCKED")    { fields.choice        = 4; }
        else if ((key == "T" || key == "QT") && i + 1 < words.size())
        {
            fields.choice = (name == "HEAD") ? (key == "T" ? 1 : 2) : (key == "T" ? 2 : 3);
            fields.track  = words[++i];
        }
        else if (key == "BLOCK" && i + 1 < words.size())
        {
            fields.number = words[++i];
        }
        else if (key == "READ")    { fields.choice = 1; }
        else if (key == "WRITE")   { fields.choice = 2; }
        else if (key == "SEEK" || key == "STATUS") { fields.choice = 3; }
        else if (key == "FORMAT")  { fields.choice = 4; }
        else if (key == "ON")      { fields.choice = 1; }
        else if (key == "OFF")     { fields.choice = 2; }
        else if (key == "STOPPED") { fields.choice = 3; }
        else
        {
            numbers.push_back (words[i]);
        }
    }

    if (name == "BLOCK")
    {
        fields.number = numbers.empty() ? std::string() : numbers[0];
        return fields;
    }

    fields.track  = (numbers.size() > 0) ? numbers[0] : fields.track;
    fields.sector = (numbers.size() > 1) ? numbers[1] : fields.sector;
    fields.number = (numbers.size() > 2) ? numbers[2] : fields.number;
    fields.choice = (name == "WRITE" && !numbers.empty()) ? 1 : fields.choice;
    return fields;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::GetVisibleFields
//
//  What MakeDefinition reads for the type: a data breakpoint's mode, a disk
//  breakpoint's event and drive, the address (a disk breakpoint's
//  arguments), the value, and the condition of the types that take one.
//
////////////////////////////////////////////////////////////////////////////////

BreakpointDialog::VisibleFields BreakpointDialog::GetVisibleFields (Type type)
{
    VisibleFields  shown;
    bool           isWatched = type == Type::Read || type == Type::Write || type == Type::ReadWrite;



    shown.event     = type == Type::Disk;
    shown.drive     = type == Type::Disk;
    shown.mode      = isWatched;
    shown.access    = isWatched;
    shown.value     = type == Type::Value || type == Type::Opcode || type == Type::Beam;
    shown.condition = type == Type::Execute || isWatched || type == Type::Value || type == Type::Disk || type == Type::Condition;
    shown.address   = type == Type::Execute || isWatched || type == Type::Value || type == Type::BasicLine ||
                      type == Type::BasicVariable || type == Type::VideoLine || type == Type::Beam;

    return shown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::MakeDefinition
//
////////////////////////////////////////////////////////////////////////////////

std::string BreakpointDialog::MakeDefinition (const Fields & fields)
{
    std::string  clause = fields.condition.empty() ? std::string() : " IF " + fields.condition;
    std::string  mode   = (fields.mode == WatchMode::Before) ? " BEFORE" : "";



    switch (fields.type)
    {
    case Type::Read:      return "BPMR " + fields.address + mode + clause;
    case Type::Write:     return "BPMW " + fields.address + mode + clause;
    case Type::ReadWrite: return "BPM " + fields.address + mode + clause;
    case Type::Value:     return "BPMV " + fields.address + " " + fields.value + clause;
    case Type::Condition: return "BPR IF " + fields.condition;
    case Type::Opcode:    return "BRKOP " + fields.value;
    case Type::Brk:       return "BRK ON";
    case Type::Interrupt: return "BRKINT ON";
    case Type::Disk:      return std::format ("BPDISK {}{}{}{}", s_kDrivePrefixes[std::clamp (fields.drive, 0, 2)],
                                              DiskBreakSpecText::GetEventWords()[std::clamp<size_t> (fields.event, 0, DiskBreakSpecText::GetEventWords().size() - 1)].word,
                                              fields.arguments.empty() ? std::string() : " " + fields.arguments, clause);
    case Type::BasicLine:
        return "BPBASIC " + fields.address;
    case Type::BasicVariable:
        return "BPBASICVAR " + fields.address;
    case Type::BasicError:
        return "BRKBASIC ON";
    case Type::UnwrittenRead:
        return "BRKUNINIT ON";
    case Type::VideoLine:
        return "BPV " + fields.address;
    case Type::Beam:
        return (_stricmp (fields.address.c_str(), "VBL") == 0) ? std::string ("BPBEAM VBL") : "BPBEAM " + fields.address + " " + fields.value;
    default:              return "BP " + fields.address + clause;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::GetFields
//
//  A condition breakpoint's expression is its condition.
//
////////////////////////////////////////////////////////////////////////////////

BreakpointDialog::Fields BreakpointDialog::GetFields (const BreakpointInfo & info)
{
    Fields  fields;



    fields.type      = GetType (info);
    fields.mode      = info.mode;
    fields.address   = (info.last > info.address) ? std::format ("{:04X}:{:04X}", info.address, info.last)
                                                  : std::format ("{:04X}", info.address);
    fields.condition = info.condition;

    switch (fields.type)
    {
    case Type::Value:    fields.value = std::format ("{:02X}", info.value.value_or (0)); break;
    case Type::Opcode:   fields.value = std::format ("{:02X}", info.opcode);             break;
    case Type::Condition: fields.condition = info.condition;                             break;
    case Type::Disk:     fields = GetDiskFields (info, fields);                          break;
    case Type::BasicLine:
        fields.address = (info.address == 0) ? std::string() : std::to_string (info.address);
        break;
    case Type::BasicVariable:
        fields.address = info.basicName;
        break;
    default:                                                                             break;
    }

    return fields;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::GetDiskFields
//
//  The event, the drive, and BPDISK's arguments with the drive and the
//  event word taken off the front, or none for a new breakpoint, so the
//  field's placeholder has the event's syntax. A written prologue and a
//  blocked write are the Write event, with arguments that differ.
//
////////////////////////////////////////////////////////////////////////////////

BreakpointDialog::Fields BreakpointDialog::GetDiskFields (const BreakpointInfo & info, Fields fields)
{
    DiskBreakSpec                                  spec   = info.disk.value_or (DiskBreakSpec {});
    std::span<const DiskBreakSpecText::EventWord>  words  = DiskBreakSpecText::GetEventWords();
    DiskEvent                                      event  = (spec.event == DiskEvent::WritePrologue || spec.event == DiskEvent::WriteBlocked) ? DiskEvent::WriteMode : spec.event;
    std::string                                    text   = DiskBreakSpecText::FormatArguments (spec);
    size_t                                         prefix = 0;



    for (size_t index = 0; index < words.size(); index++)
    {
        fields.event = (words[index].event == event) ? (int) index : fields.event;
    }

    fields.drive = (spec.drive == DiskDriveFilter::Drive1) ? 1 : (spec.drive == DiskDriveFilter::Drive2) ? 2 : 0;
    prefix       = strlen (s_kDrivePrefixes[fields.drive]) + strlen (words[fields.event].word);

    fields.arguments = (info.disk.has_value() && text.size() > prefix) ? text.substr (prefix + 1) : std::string();
    return fields;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::GetType
//
////////////////////////////////////////////////////////////////////////////////

BreakpointDialog::Type BreakpointDialog::GetType (const BreakpointInfo & info)
{
    switch (info.kind)
    {
    case BreakpointKind::Register:      return Type::Condition;
    case BreakpointKind::Opcode:        return Type::Opcode;
    case BreakpointKind::MemoryValue:   return Type::Value;
    case BreakpointKind::Brk:           return Type::Brk;
    case BreakpointKind::Interrupt:     return Type::Interrupt;
    case BreakpointKind::Disk:          return Type::Disk;
    case BreakpointKind::BasicLine:     return Type::BasicLine;
    case BreakpointKind::BasicVariable: return Type::BasicVariable;
    case BreakpointKind::BasicError:    return Type::BasicError;
    case BreakpointKind::Memory:
        return (info.access == WatchAccess::Read)  ? Type::Read
             : (info.access == WatchAccess::Write) ? Type::Write
                                                   : Type::ReadWrite;
    default:                            return Type::Execute;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::ShowFieldsForType
//
//  Only the fields the type uses are visible, and the value field's label
//  is the type's meaning for it. A disk breakpoint takes an event, a drive
//  and the event's arguments in the address field, whose placeholder is the
//  event's syntax line.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointDialog::ShowFieldsForType()
{
    Type           type  = GetSelectedType();
    VisibleFields  shown = GetVisibleFields (type);



    m_mode.SetVisible      (shown.mode);
    m_event.SetVisible     (shown.event);
    m_drive.SetVisible     (shown.drive);
    m_value.SetVisible     (shown.value);
    m_address.SetVisible   (shown.address);
    m_condition.SetVisible (shown.condition);
    m_accessRead.SetVisible  (shown.access);
    m_accessWrite.SetVisible (shown.access);
    ShowDiskFields (type == Type::Disk);

    //  A new breakpoint's address is the PC only while its type is Execute.
    if (m_newAddress.has_value() && type != Type::Execute && m_address.GetText() == *m_newAddress)
    {
        m_address.SetText (std::wstring());
    }
    else if (m_newAddress.has_value() && type == Type::Execute && m_address.GetText().empty())
    {
        m_address.SetText (*m_newAddress);
    }

    //  The rows close up over the hidden fields.
    if (m_body != nullptr)
    {
        DxuiDpiScaler  scaler;

        scaler.SetDpi  (GetDpi());
        m_body->Layout (m_body->GetBounds(), scaler);
    }

    switch (type)
    {
    case Type::BasicLine:
        m_addressLabel.SetText   (L"Line number");
        m_address.SetPlaceholder (L"Such as 100");
        break;

    case Type::BasicVariable:
        m_addressLabel.SetText   (L"Variable");
        m_address.SetPlaceholder (L"Such as A, B%, C$ or D(3)");
        break;

    case Type::VideoLine:
        m_addressLabel.SetText   (L"Scanlines");
        m_address.SetPlaceholder (L"Hex, such as 64 or 64:6B");
        break;

    case Type::Beam:
        m_addressLabel.SetText   (L"Scanline");
        m_address.SetPlaceholder (L"Hex, such as 64, or VBL");
        break;

    default:
        m_addressLabel.SetText   (L"Address");
        m_address.SetPlaceholder (L"Address, symbol or range: 300, COUT, 300:3FF or 300,100");
        break;
    }

    switch (type)
    {
    case Type::Value:
        m_valueLabel.SetText   (L"Value");
        m_value.SetPlaceholder (L"Hex byte, such as 41");
        break;

    case Type::Opcode:
        m_valueLabel.SetText   (L"Opcode");
        m_value.SetPlaceholder (L"Hex byte or mnemonic, such as 60 or RTS");
        break;

    case Type::Beam:
        m_valueLabel.SetText   (L"Cycle in the line");
        m_value.SetPlaceholder (L"Hex, such as 1A");
        break;

    default:
        m_valueLabel.SetText   (L"Value");
        m_value.SetPlaceholder (std::wstring());
        break;
    }

    m_condition.SetPlaceholder (type == Type::Condition ? L"Stops when true, such as A == 41" : L"Optional, such as A == 41");
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::GetSelectedType
//
//  The type the dialog opened on; for a memory address, the one its checks
//  choose, both or neither being Read or write.
//
////////////////////////////////////////////////////////////////////////////////

BreakpointDialog::Type BreakpointDialog::GetSelectedType() const
{
    bool  isRead  = m_accessRead.IsChecked();
    bool  isWrite = m_accessWrite.IsChecked();



    if (m_type != Type::Read && m_type != Type::Write && m_type != Type::ReadWrite)
    {
        return m_type;
    }

    return (isRead && !isWrite) ? Type::Read : (isWrite && !isRead) ? Type::Write : Type::ReadWrite;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::ShowDiskFields
//
//  The chosen event's fields, labeled for it; its drop-down's items are set
//  again only when the event changes, so a choice made survives other
//  changes. A range's placeholder is an example of the range form.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointDialog::ShowDiskFields (bool isDisk)
{
    int           event    = std::clamp<int> (m_event.GetSelectedIndex(), 0, (int) DiskBreakSpecText::GetEventWords().size() - 1);
    DiskFieldSet  fieldSet = GetDiskFieldSet (event);



    if (event != m_diskFieldSetEvent && !fieldSet.choices.empty())
    {
        m_diskChoice.SetItems    (fieldSet.choices);
        m_diskChoice.SetSelected (0);
    }

    m_diskFieldSetEvent = event;

    m_diskTrack.SetVisible       (isDisk && fieldSet.trackLabel != nullptr);
    m_diskSector.SetVisible      (isDisk && fieldSet.sectorLabel != nullptr);
    m_diskNumber.SetVisible      (isDisk && fieldSet.numberLabel != nullptr);
    m_diskChoice.SetVisible      (isDisk && fieldSet.choiceLabel != nullptr);
    m_diskPassed.SetVisible      (isDisk && fieldSet.isPassed);
    m_diskBadChecksum.SetVisible (isDisk && fieldSet.isBadChecksum);
    m_diskWrongTrack.SetVisible  (isDisk && fieldSet.isWrongTrack);
    m_diskBump.SetVisible        (isDisk && fieldSet.isBump);
    m_diskEvery.SetVisible       (isDisk && fieldSet.isEvery);
    m_diskAdvanced.SetVisible    (isDisk && fieldSet.isAdvanced);

    m_diskTrackLabel.SetText  (fieldSet.trackLabel  != nullptr ? fieldSet.trackLabel  : L"");
    m_diskSectorLabel.SetText (fieldSet.sectorLabel != nullptr ? fieldSet.sectorLabel : L"");
    m_diskNumberLabel.SetText (fieldSet.numberLabel != nullptr ? fieldSet.numberLabel : L"");
    m_diskChoiceLabel.SetText (fieldSet.choiceLabel != nullptr ? fieldSet.choiceLabel : L"");

    m_diskTrack.SetPlaceholder  (fieldSet.isRange ? L"Hex, such as 11 or 10:12" : L"Hex, such as 11; empty for any");
    m_diskSector.SetPlaceholder (L"Hex, such as 0F; empty for any");
    m_diskNumber.SetPlaceholder (L"Hex; empty for any");
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::ReadDiskFields
//
////////////////////////////////////////////////////////////////////////////////

BreakpointDialog::DiskFields BreakpointDialog::ReadDiskFields() const
{
    DiskFields  fields;



    fields.track         = SourcePathList::WideToUtf8 (m_diskTrack.GetText());
    fields.sector        = SourcePathList::WideToUtf8 (m_diskSector.GetText());
    fields.number        = SourcePathList::WideToUtf8 (m_diskNumber.GetText());
    fields.choice        = m_diskChoice.GetSelectedIndex();
    fields.isPassed      = m_diskPassed.IsChecked();
    fields.isBadChecksum = m_diskBadChecksum.IsChecked();
    fields.isWrongTrack  = m_diskWrongTrack.IsChecked();
    fields.isBump        = m_diskBump.IsChecked();
    fields.isEvery       = m_diskEvery.IsChecked();
    fields.advanced      = SourcePathList::WideToUtf8 (m_diskAdvanced.GetText());
    return fields;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointDialog::OnCreate()
{
    DxuiButton  * ok     = nullptr;
    Fields        fields = GetFields (m_info);



    //  The New drop-down's choice, or the edited breakpoint's own type.
    if (m_presetType.has_value())
    {
        fields.type = *m_presetType;
    }

    m_type = fields.type;

    for (DxuiLabel * label : { &m_accessLabel, &m_accessWriteLabel, &m_eventLabel, &m_driveLabel, &m_modeLabel, &m_addressLabel, &m_valueLabel, &m_conditionLabel })
    {
        label->SetTextRole  (DxuiTextRole::Body);
        label->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    }

    m_accessLabel.SetText    (L"Access");
    m_eventLabel.SetText     (L"Disk event");
    m_driveLabel.SetText     (L"Drive");
    m_modeLabel.SetText      (L"Stops");
    m_addressLabel.SetText   (L"Address or range");
    m_conditionLabel.SetText (L"Condition");

    m_accessRead.SetLabel    (L"Read");
    m_accessWrite.SetLabel   (L"Write");
    m_accessRead.SetChecked  (fields.type != Type::Write);
    m_accessWrite.SetChecked (fields.type != Type::Read);

    m_mode.SetItems     ({ L"After the access", L"Before the access" });
    m_mode.SetSelected  ((int) fields.mode);
    m_mode.SetPopupHost (GetPopupHost());

    m_event.SetItems     (std::vector<std::wstring> (std::begin (s_kDiskEventNames), std::end (s_kDiskEventNames)));
    m_event.SetSelected  (fields.event);
    m_event.SetSelect    ([this] (int) { ShowFieldsForType(); Invalidate(); });
    m_event.SetPopupHost (GetPopupHost());

    m_drive.SetItems     ({ L"Either drive", L"Drive 1", L"Drive 2" });
    m_drive.SetSelected  (fields.drive);
    m_drive.SetPopupHost (GetPopupHost());

    {
        DiskFields    disk     = ParseDiskArguments (fields.event, fields.arguments);
        DiskFieldSet  fieldSet = GetDiskFieldSet (fields.event);

        for (DxuiLabel * label : { &m_diskTrackLabel, &m_diskSectorLabel, &m_diskNumberLabel, &m_diskChoiceLabel, &m_diskAdvancedLabel })
        {
            label->SetTextRole  (DxuiTextRole::Body);
            label->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
        }

        m_diskAdvancedLabel.SetText (L"Advanced");
        m_diskAdvanced.SetPlaceholder (L"Marks and length: ADDR D5 AA 96, DATA D5 AA AD, SECTORS 13");

        m_diskTrack.SetText    (SourcePathList::Utf8ToWide (disk.track));
        m_diskSector.SetText   (SourcePathList::Utf8ToWide (disk.sector));
        m_diskNumber.SetText   (SourcePathList::Utf8ToWide (disk.number));
        m_diskAdvanced.SetText (SourcePathList::Utf8ToWide (disk.advanced));

        m_diskChoice.SetItems     (fieldSet.choices);
        m_diskChoice.SetSelected  (disk.choice);
        m_diskChoice.SetPopupHost (GetPopupHost());
        m_diskFieldSetEvent = fields.event;

        m_diskPassed.SetLabel      (L"Include sectors passed over");
        m_diskBadChecksum.SetLabel (L"Only a bad checksum");
        m_diskWrongTrack.SetLabel  (L"Only a field from another track");
        m_diskBump.SetLabel        (L"Only a step against the stop");
        m_diskEvery.SetLabel       (L"Every selection, not only a change");

        m_diskPassed.SetChecked      (disk.isPassed);
        m_diskBadChecksum.SetChecked (disk.isBadChecksum);
        m_diskWrongTrack.SetChecked  (disk.isWrongTrack);
        m_diskBump.SetChecked        (disk.isBump);
        m_diskEvery.SetChecked       (disk.isEvery);
    }

    for (DxuiTextInput * box : { &m_address, &m_value, &m_condition, &m_diskTrack, &m_diskSector, &m_diskNumber, &m_diskAdvanced })
    {
        box->SetTheme        (m_theme);
        box->SetHwnd         (GetHwnd());
        box->SetTextRenderer (GetTextRenderer());
    }

    m_address.SetText   (SourcePathList::Utf8ToWide ((fields.type == Type::Disk) ? std::string() : fields.address));
    m_newAddress = m_newAddress.has_value() ? std::optional<std::wstring> (SourcePathList::Utf8ToWide (fields.address)) : std::nullopt;
    m_value.SetText     (SourcePathList::Utf8ToWide (fields.value));
    m_condition.SetText (SourcePathList::Utf8ToWide (fields.condition));

    m_conditionTip.SetText (L"Stops when the condition is true. For example:\n"
                            L"A == 41\n"
                            L"X >= 10 && Y < 20\n"
                            L"P.C && A >= 80, flag C set and A at least $80\n"
                            L"*FB == 0, the byte at $FB\n"
                            L"PC == COUT, a symbol's address\n"
                            L"Numbers are hex; 0n10 is decimal. P.C, P.Z, P.I, P.D, P.B, P.V and P.N are the flags, 0 or 1.\n"
                            L"Operators: + - * / % & | ^ ~ ! == != < > <= >= && ||, and parentheses.");
    m_conditionTip.SetPopupHost (GetPopupHost());
    m_conditionTip.SetDpi       (GetDpi());
    ShowFieldsForType();

    m_body = CreateDialogContent<BreakpointDialogPanel>();
    m_body->Add (m_eventLabel,     m_event);
    m_body->Add (m_driveLabel,     m_drive);
    m_body->Add (m_diskChoiceLabel,   m_diskChoice);
    m_body->Add (m_diskTrackLabel,    m_diskTrack);
    m_body->Add (m_diskSectorLabel,   m_diskSector);
    m_body->Add (m_diskNumberLabel,   m_diskNumber);
    m_body->Add (m_diskCheckLabels[0], m_diskPassed);
    m_body->Add (m_diskCheckLabels[1], m_diskBadChecksum);
    m_body->Add (m_diskCheckLabels[2], m_diskWrongTrack);
    m_body->Add (m_diskCheckLabels[3], m_diskBump);
    m_body->Add (m_diskCheckLabels[4], m_diskEvery);
    m_body->Add (m_diskAdvancedLabel, m_diskAdvanced);
    m_body->Add (m_accessLabel,      m_accessRead);
    m_body->Add (m_accessWriteLabel, m_accessWrite);
    m_body->Add (m_modeLabel,      m_mode);
    m_body->Add (m_addressLabel,   m_address);
    m_body->Add (m_valueLabel,     m_value);
    m_body->Add (m_conditionLabel, m_condition, &m_conditionTip);

    ok = AddDialogButton (L"OK", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    ok->SetOnClick ([this]()
    {
        Fields  edited;



        edited.type      = GetSelectedType();
        edited.mode      = (WatchMode) m_mode.GetSelectedIndex();
        edited.address   = SourcePathList::WideToUtf8 (m_address.GetText());
        edited.value     = SourcePathList::WideToUtf8 (m_value.GetText());
        edited.condition = SourcePathList::WideToUtf8 (m_condition.GetText());
        edited.event     = m_event.GetSelectedIndex();
        edited.drive     = m_drive.GetSelectedIndex();
        edited.arguments = MakeDiskArguments (edited.event, ReadDiskFields());
        m_definition     = MakeDefinition (edited);
        EndDialog (IDOK);
    });

    //  The first field the type uses.
    SetInitialFocus (GetVisibleFields (fields.type).address   ? (IDxuiControl *) &m_address
                   : GetVisibleFields (fields.type).value     ? (IDxuiControl *) &m_value
                   : GetVisibleFields (fields.type).condition ? (IDxuiControl *) &m_condition
                   : GetVisibleFields (fields.type).event     ? (IDxuiControl *) &m_event
                   :                                            (IDxuiControl *) ok);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::RunModal
//
////////////////////////////////////////////////////////////////////////////////

std::optional<std::string> BreakpointDialog::RunModal (HWND owner, const IDxuiTheme * theme, const BreakpointInfo & info, std::optional<Type> type)
{
    HRESULT                   hr     = S_OK;
    BreakpointDialog          dialog;
    DxuiWindow::CreateParams  params;



    dialog.m_theme      = theme;
    dialog.m_info       = info;
    dialog.m_presetType = type;
    dialog.m_newAddress = (info.id < 0) ? std::optional<std::wstring> (std::wstring()) : std::nullopt;

    params.title                    = GetTitle (type.value_or (GetType (info)), (info.id < 0) ? std::nullopt : std::optional<int> (info.id));
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 500, 560 };
    params.minSizeDip               = { 440, 560 };
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);

    if (FAILED (hr))
    {
        return std::nullopt;
    }

    dialog.SetTheme (theme);
    dialog.ShowModalDialog (IDOK);

    return dialog.m_definition;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::RunNew
//
////////////////////////////////////////////////////////////////////////////////

std::optional<std::string> BreakpointDialog::RunNew (HWND owner, const IDxuiTheme * theme, Type type, Word pc)
{
    BreakpointInfo  info;



    info.id      = -1;
    info.kind    = BreakpointKind::Address;
    info.address = pc;
    info.last    = pc;

    return RunModal (owner, theme, info, type);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::OnDialogTick
//
//  The condition's info tip appears after its dwell and fades on the dialog's
//  ticks, as nothing else ticks a dialog's controls.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointDialog::OnDialogTick()
{
    m_conditionTip.Tick ((int64_t) GetTickCount64());
}
