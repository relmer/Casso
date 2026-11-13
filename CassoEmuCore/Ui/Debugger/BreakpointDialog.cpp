#include "Pch.h"

#include "Ui/Debugger/BreakpointDialog.h"
#include "Ui/Debugger/DebuggerActions.h"
#include "Debugger/DiskBreakSpecText.h"
#include "Debugger/Source/SourcePathList.h"



static constexpr const wchar_t *  s_kDiskEventNames[] =
{
    L"Read sector", L"Read address field", L"Move head", L"Write disk", L"Read or write DOS 3.3 sector", L"Read or write ProDOS block",
    L"Call RWTS", L"Call ProDOS driver", L"Turn motor on or off", L"Select drive", L"Insert disk image (Casso)", L"Eject disk image (Casso)",
};

static constexpr const char *  s_kDrivePrefixes[] = { "", "D1 ", "D2 " };





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialogPanel::Add
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointDialogPanel::Add (DxuiLabel & label, IDxuiControl & field, DxuiInfoTip * tip, IDxuiControl * second)
{
    m_rows.push_back ({ &label, &field, tip, second, nullptr });
    Adopt (label);
    Adopt (field);

    if (tip != nullptr)
    {
        Adopt (*tip);
    }

    if (second != nullptr)
    {
        Adopt (*second);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialogPanel::AddError
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointDialogPanel::AddError (DxuiFieldError & error)
{
    m_rows.push_back ({ nullptr, nullptr, nullptr, nullptr, &error });
    Adopt (error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialogPanel::Layout
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointDialogPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int  row    = scaler.ToPx (kRowDip);
    int  gap    = scaler.ToPx (kGapDip);
    int  split  = boundsDip.left + scaler.ToPx (kLabelDip);
    int  tip    = scaler.ToPx (kTipDip);
    int  tipGap = scaler.ToPx (kTipGapDip);
    int  width  = boundsDip.right - boundsDip.left;
    int  half   = (boundsDip.right - split) / 2;
    int  y      = boundsDip.top;



    SetBounds (boundsDip);

    for (const Row & each : m_rows)
    {
        bool  isShown = (each.error != nullptr) ? each.error->HasError() : each.field->IsVisible();
        int   height  = (each.error != nullptr) ? GetErrorRoomPx (*each.error, scaler, width) : row;



        if (each.error != nullptr)
        {
            each.error->SetVisible (isShown);
            each.error->Layout (RECT { boundsDip.left, y, boundsDip.right, y + height }, scaler);
            y += height + gap;
            continue;
        }

        each.label->SetVisible (isShown);

        if (each.second != nullptr)
        {
            each.second->SetVisible (isShown);
        }

        if (each.tip != nullptr)
        {
            each.tip->SetVisible (isShown && !each.tip->GetText().empty());
        }

        if (!isShown)
        {
            continue;
        }

        each.label->Layout (RECT { boundsDip.left, y, split, y + row }, scaler);
        each.field->Layout (RECT { split, y, (each.second != nullptr) ? split + half : boundsDip.right, y + row }, scaler);

        if (each.second != nullptr)
        {
            each.second->Layout (RECT { split + half, y, boundsDip.right, y + row }, scaler);
        }

        //  Just after the label's text, clear of the field.
        if (each.tip != nullptr)
        {
            int  left = std::min<int> (boundsDip.left + GetTextWidthPx (*each.label, scaler) + tipGap, split - tip - tipGap);

            each.tip->SetDpi (scaler.GetDpi());
            each.tip->Layout (RECT { left, y, left + tip, y + row }, scaler);
        }

        y += row + gap;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialogPanel::GetHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int BreakpointDialogPanel::GetHeightPx (const DxuiDpiScaler & scaler, int widthPx) const
{
    int  row    = scaler.ToPx (kRowDip);
    int  gap    = scaler.ToPx (kGapDip);
    int  height = 0;



    for (const Row & each : m_rows)
    {
        if (each.error != nullptr)
        {
            height += GetErrorRoomPx (*each.error, scaler, widthPx) + gap;
        }
        else if (each.error == nullptr && each.field->IsVisible())
        {
            height += row + gap;
        }
    }

    return std::max (height - gap, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialogPanel::GetErrorHeightPx
//
//  One row's height until the text renderer and the theme are set.
//
////////////////////////////////////////////////////////////////////////////////

int BreakpointDialogPanel::GetErrorHeightPx (const DxuiFieldError & error, const DxuiDpiScaler & scaler, int widthPx) const
{
    if (m_text == nullptr || m_theme == nullptr)
    {
        return scaler.ToPx (kRowDip);
    }

    return error.GetHeightPx (*m_text, *m_theme, scaler, widthPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialogPanel::GetErrorRoomPx
//
//  The room reserved for the error, or the error's height when it is taller.
//
////////////////////////////////////////////////////////////////////////////////

int BreakpointDialogPanel::GetErrorRoomPx (const DxuiFieldError & error, const DxuiDpiScaler & scaler, int widthPx) const
{
    int  room = scaler.ToPx (kErrorDip);



    return error.HasError() ? (std::max) (room, GetErrorHeightPx (error, scaler, widthPx)) : room;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialogPanel::GetTextWidthPx
//
//  The label's text in the theme's body font, as the label draws it, with
//  its required mark; the whole label column until the text renderer and
//  the theme are set.
//
////////////////////////////////////////////////////////////////////////////////

int BreakpointDialogPanel::GetTextWidthPx (const DxuiLabel & label, const DxuiDpiScaler & scaler) const
{
    DxuiFontHandle  font;
    std::wstring    marked = label.GetText() + (label.IsRequired() ? DxuiLabel::kRequiredMark : L"");
    float           width  = 0.0f;
    float           height = 0.0f;
    HRESULT         hr     = S_OK;



    if (m_text == nullptr || m_theme == nullptr)
    {
        return scaler.ToPx (kLabelDip);
    }

    font         = m_theme->BodyFont();
    font.sizeDip = scaler.ToPxf (font.sizeDip);
    hr           = m_text->MeasureString (marked.c_str(), font, width, height);

    return SUCCEEDED (hr) ? (int) std::ceil (width) : scaler.ToPx (kLabelDip);
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
        { L"Source line",                  Type::SourceLine          },
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
        { L"Errors",                       Type::BasicError          },
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
//  position, a kind of write, a call's command, the motor's state, the
//  checks for PASSED, BADSUM, WRONGTRACK, BUMP and EVERY, and the prologue
//  marks of the events that read or write a field.
//
////////////////////////////////////////////////////////////////////////////////

BreakpointDialog::DiskFieldSet BreakpointDialog::GetDiskFieldSet (int event)
{
    DiskFieldSet  fieldSet;
    std::string   word     = DiskBreakSpecText::GetEventWords()[std::clamp<size_t> (event, 0, DiskBreakSpecText::GetEventWords().size() - 1)].word;



    if (word == "READ" || word == "DOS")
    {
        fieldSet.trackLabel     = L"Track";
        fieldSet.sectorLabel    = L"Sector";
        fieldSet.isPassed       = true;
        fieldSet.isAddressMarks = word == "READ";
        fieldSet.isDataMarks    = word == "READ";
    }
    else if (word == "ADDRESS")
    {
        fieldSet.trackLabel     = L"Track";
        fieldSet.sectorLabel    = L"Sector";
        fieldSet.numberLabel    = L"Volume";
        fieldSet.isBadChecksum  = true;
        fieldSet.isWrongTrack   = true;
        fieldSet.isAddressMarks = true;
    }
    else if (word == "HEAD")
    {
        fieldSet.choiceLabel = L"Stops at";
        fieldSet.choices     = { L"Any position", L"Whole tracks", L"Quarter tracks", L"Any half track" };
        fieldSet.trackLabel  = L"Tracks";
        fieldSet.isRange     = true;
        fieldSet.isBump      = true;
    }
    else if (word == "WRITE")
    {
        fieldSet.choiceLabel    = L"Write";
        fieldSet.choices        = { L"Any write", L"A sector", L"Whole tracks", L"Quarter tracks", L"Blocked by write protection" };
        fieldSet.trackLabel     = L"Track";
        fieldSet.sectorLabel    = L"Sector";
        fieldSet.isAddressMarks = true;
        fieldSet.isDataMarks    = true;
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

    //  A DOS 3.3 sector or a ProDOS block is watched being read, written or
    //  either.
    if (word == "DOS" || word == "BLOCK")
    {
        fieldSet.choiceLabel = L"Access";
        fieldSet.choices     = { L"Read", L"Written", L"Read or written" };
    }

    return fieldSet;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::MakeDiskArguments
//
//  The fields in the order BPDISK's syntax line has them. A field the event
//  does not have, or that its drop-down does not use, is not written. A
//  sector read, a DOS sector and a written sector take both numbers, a star
//  for one left empty. The volume is typed in decimal and written in hex.
//
////////////////////////////////////////////////////////////////////////////////

std::string BreakpointDialog::MakeDiskArguments (int event, const DiskFields & fields)
{
    static constexpr const char *  kRwtsCommands[]   = { "", "READ", "WRITE", "SEEK", "FORMAT" };
    static constexpr const char *  kDriverCommands[] = { "", "READ", "WRITE", "STATUS", "FORMAT" };
    static constexpr const char *  kMotorStates[]    = { "", "ON", "OFF", "STOPPED" };
    static constexpr const char *  kAccessWords[]    = { "", "WRITTEN", "EITHER" };
    std::string                    word    = DiskBreakSpecText::GetEventWords()[std::clamp<size_t> (event, 0, DiskBreakSpecText::GetEventWords().size() - 1)].word;
    DiskFieldSet                   set     = GetDiskFieldSet (event);
    std::vector<std::string>       parts;
    int                            choice  = std::max (fields.choice, 0);
    auto                           add     = [&parts] (const std::string & part) { if (!part.empty()) { parts.push_back (part); } };
    auto                           orAny   = [] (const std::string & number) { return number.empty() ? std::string ("*") : number; };
    bool                           isField   = word != "WRITE" || choice == 1;
    bool                           hasVolume = word == "ADDRESS" && !fields.number.empty();
    std::string                    text;



    if (word == "READ" || word == "DOS")
    {
        add (orAny (fields.track));
        add (orAny (fields.sector));
    }

    if (word == "ADDRESS" || word == "RWTS")
    {
        add (fields.track.empty() && (!fields.sector.empty() || hasVolume) ? std::string ("*") : fields.track);
        add (fields.sector.empty() && hasVolume ? std::string ("*") : fields.sector);
        add (hasVolume ? ToHex (fields.number) : std::string());
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
        add ((choice == 1) ? orAny (fields.track) + " " + orAny (fields.sector)
           : (choice == 2 && !fields.track.empty()) ? "T " + fields.track
           : (choice == 3 && !fields.track.empty()) ? "QT " + fields.track
           : (choice == 4)                          ? std::string ("BLOCKED")
           :                                          std::string());
    }

    add ((word == "DOS" || word == "BLOCK") ? kAccessWords [std::min (choice, 2)] : "");
    add (word == "RWTS"   ? kRwtsCommands  [std::min (choice, 4)] : "");
    add (word == "DRIVER" ? kDriverCommands[std::min (choice, 4)] : "");
    add (word == "MOTOR"  ? kMotorStates   [std::min (choice, 3)] : "");

    add (fields.isPassed      && set.isPassed      ? "PASSED"     : "");
    add (fields.isBadChecksum && set.isBadChecksum ? "BADSUM"     : "");
    add (fields.isWrongTrack  && set.isWrongTrack  ? "WRONGTRACK" : "");
    add (fields.isBump        && set.isBump        ? "BUMP"       : "");
    add (fields.isEvery       && set.isEvery       ? "EVERY"      : "");
    add (set.isAddressMarks && isField && !fields.addressMarks.empty() ? "ADDR " + fields.addressMarks : std::string());
    add (set.isDataMarks    && isField && !fields.dataMarks.empty()    ? "DATA " + fields.dataMarks    : std::string());
    add (set.isAddressMarks && isField && fields.isThirteen            ? std::string ("SECTORS 13")    : std::string());

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
//  the order the event takes them, a star as an empty field, each keyword's
//  check or choice, ADDR's and DATA's three marks, and SECTORS 13. The
//  volume field is in decimal.
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

        if (key == "ADDR" || key == "DATA")
        {
            std::string  & marks = (key == "ADDR") ? fields.addressMarks : fields.dataMarks;

            marks.clear();

            for (size_t k = 0; k < 3 && i + 1 < words.size(); k++)
            {
                marks += (marks.empty() ? "" : " ") + upper (words[++i]);
            }
        }
        else if (key == "SECTORS" && i + 1 < words.size())
        {
            fields.isThirteen = words[++i] == "13";
        }
        else if (key == "PASSED")     { fields.isPassed      = true; }
        else if (key == "BADSUM")     { fields.isBadChecksum = true; }
        else if (key == "WRONGTRACK") { fields.isWrongTrack  = true; }
        else if (key == "BUMP")       { fields.isBump        = true; }
        else if (key == "EVERY")      { fields.isEvery       = true; }
        else if (key == "HALF")       { fields.choice        = 3; }
        else if (key == "WRITTEN")    { fields.choice        = 1; }
        else if (key == "EITHER")     { fields.choice        = 2; }
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

    for (std::string & number : numbers)
    {
        number = (number == "*") ? std::string() : number;
    }

    fields.track  = (numbers.size() > 0) ? numbers[0] : fields.track;
    fields.sector = (numbers.size() > 1) ? numbers[1] : fields.sector;
    fields.number = (numbers.size() > 2) ? ToDecimal (numbers[2]) : fields.number;
    fields.choice = (name == "WRITE" && !numbers.empty()) ? 1 : fields.choice;
    return fields;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::ToDecimal
//
//  A hex number as decimal; anything else as it is.
//
////////////////////////////////////////////////////////////////////////////////

std::string BreakpointDialog::ToDecimal (const std::string & hex)
{
    unsigned int  value  = 0;
    auto          parsed = std::from_chars (hex.data(), hex.data() + hex.size(), value, 16);



    return (!hex.empty() && parsed.ec == std::errc() && parsed.ptr == hex.data() + hex.size()) ? std::to_string (value) : hex;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::ToHex
//
//  A decimal number as hex; anything else as it is, for BPDISK to report.
//
////////////////////////////////////////////////////////////////////////////////

std::string BreakpointDialog::ToHex (const std::string & decimal)
{
    unsigned int  value  = 0;
    auto          parsed = std::from_chars (decimal.data(), decimal.data() + decimal.size(), value, 10);



    return (!decimal.empty() && parsed.ec == std::errc() && parsed.ptr == decimal.data() + decimal.size()) ? std::format ("{:X}", value) : decimal;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::GetVisibleFields
//
//  What MakeDefinition reads for the type: a data breakpoint's mode, a disk
//  breakpoint's event and drive, the address, the value, and the condition
//  of every type that is a row of the breakpoint table.
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
    shown.condition = type != Type::UnwrittenRead && type != Type::VideoLine && type != Type::Beam;
    shown.file      = type == Type::SourceLine;
    shown.log       = type != Type::UnwrittenRead;
    shown.address   = type == Type::Execute || type == Type::SourceLine || isWatched || type == Type::Value || type == Type::BasicLine ||
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
    return DebuggerActions::GetBreakpointEcho (ToDefinition (fields));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::ToDefinition
//
////////////////////////////////////////////////////////////////////////////////

BreakpointDefinition BreakpointDialog::ToDefinition (const Fields & fields)
{
    BreakpointDefinition  definition;



    definition.type      = fields.type;
    definition.isBefore  = fields.mode == WatchMode::Before;
    definition.address   = fields.address;
    definition.value     = fields.value;
    definition.condition = fields.condition;
    definition.event     = fields.event;
    definition.drive     = fields.drive;
    definition.arguments = fields.arguments;
    definition.file      = fields.file;

    return definition;
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
    fields.log       = info.log;
    fields.isLogGo   = !info.stops;

    switch (fields.type)
    {
    case Type::Value:    fields.value = std::format ("{:02X}", info.value.value_or (0)); break;
    case Type::Opcode:   fields.value = std::format ("{:02X}", info.opcode);             break;
    case Type::Condition: fields.condition = info.condition;                             break;
    case Type::Disk:     fields = GetDiskFields (info, fields);                          break;
    case Type::BasicLine:
        fields.address = std::to_string (info.address);
        break;
    case Type::BasicVariable:
        fields.address = info.basicName;
        break;
    case Type::VideoLine:
        fields.address = (info.last > info.address) ? std::format ("{:X}:{:X}", info.address, info.last) : std::format ("{:X}", info.address);
        break;
    case Type::Beam:
        fields.address = std::format ("{:X}", info.address);
        fields.value   = std::format ("{:X}", info.last);
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
    case BreakpointKind::UnwrittenRead: return Type::UnwrittenRead;
    case BreakpointKind::VideoLine:     return Type::VideoLine;
    case BreakpointKind::Beam:          return Type::Beam;
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



    m_conditionTip.SetText (GetConditionTip (type));
    m_logTip.SetText       (GetLogTip (type));
    m_mode.SetVisible      (shown.mode);
    m_event.SetVisible     (shown.event);
    m_drive.SetVisible     (shown.drive);
    m_file.SetVisible      (shown.file);
    m_value.SetVisible     (shown.value);
    m_address.SetVisible   (shown.address);
    m_condition.SetVisible (shown.condition);
    m_log.SetVisible       (shown.log);
    m_logGo.SetVisible     (shown.log);
    m_accessRead.SetVisible  (shown.access);
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

    switch (type)
    {
    case Type::BasicLine:
        m_addressLabel.SetText   (L"Line number");
        m_address.SetPlaceholder (L"A line number, 0 to 63999");
        break;

    case Type::SourceLine:
        m_addressLabel.SetText   (L"Line number");
        m_address.SetPlaceholder (L"A line number in the file");
        break;

    case Type::BasicVariable:
        m_addressLabel.SetText   (L"Variable");
        m_address.SetPlaceholder (L"A, B%, C$ or D(3)");
        break;

    case Type::VideoLine:
        m_addressLabel.SetText   (L"Scanlines");
        m_address.SetPlaceholder (L"A hex scanline or range: 64 or 64:6B");
        break;

    case Type::Beam:
        m_addressLabel.SetText   (L"Scanline");
        m_address.SetPlaceholder (L"A hex scanline, or VBL");
        break;

    default:
        m_addressLabel.SetText   (L"Address");
        m_address.SetPlaceholder (L"An address, symbol or range");
        break;
    }

    switch (type)
    {
    case Type::Value:
        m_valueLabel.SetText   (L"Value");
        m_value.SetPlaceholder (L"A hex byte: 41");
        break;

    case Type::Opcode:
        m_valueLabel.SetText   (L"Opcode");
        m_value.SetPlaceholder (L"A hex byte or mnemonic: 60 or RTS");
        break;

    case Type::Beam:
        m_valueLabel.SetText   (L"Cycle in the line");
        m_value.SetPlaceholder (L"A hex cycle: 1A");
        break;

    default:
        m_valueLabel.SetText   (L"Value");
        m_value.SetPlaceholder (std::wstring());
        break;
    }

    m_condition.SetPlaceholder (L"An expression: A == 41");

    //  The fields the type can't do without are marked, as IsCheckedOnFocusLoss
    //  finds them.
    m_addressLabel.SetRequired   (shown.address);
    m_valueLabel.SetRequired     (shown.value);
    m_conditionLabel.SetRequired (type == Type::Condition);

    //  The rows close up over the hidden fields, and the dialog fits them.
    Resize();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::Resize
//
//  The rows laid out again, and the dialog's height fitted to them.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointDialog::Resize()
{
    DxuiDpiScaler  scaler;



    if (m_body == nullptr)
    {
        return;
    }

    scaler.SetDpi  (GetDpi());
    m_body->Layout (m_body->GetBounds(), scaler);

    if (IsCreated())
    {
        FitToMaxSize();
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::TryGetMaxClientSizePx
//
//  The client's width, and the height of the caption, the pads, the visible
//  rows and the button row.
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointDialog::TryGetMaxClientSizePx (SIZE & outSizePx) const
{
    DxuiDpiScaler  scaler;
    RECT           client  = {};
    RECT           bounds  = GetBounds();
    int            pad     = 0;
    int            caption = 0;



    //  None before the first layout, when the caption cannot be measured.
    if (m_body == nullptr || bounds.bottom <= bounds.top || GetHwnd() == nullptr || !GetClientRect (GetHwnd(), &client))
    {
        return false;
    }

    scaler.SetDpi (GetDpi());
    pad     = scaler.ToPx (DxuiButtonRow::kEdgePadDip);
    caption = std::max<int> (0, (client.bottom - client.top) - (bounds.bottom - bounds.top));

    outSizePx.cx = client.right - client.left;
    outSizePx.cy = caption + 2 * pad + scaler.ToPx (DxuiButtonRow::kRowHeightDip) +
                   m_body->GetHeightPx (scaler, (client.right - client.left) - 2 * pad);
    return true;
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

    std::string   word     = DiskBreakSpecText::GetEventWords()[(size_t) event].word;
    int           choice   = m_diskChoice.GetSelectedIndex();
    bool          isField  = word != "WRITE" || choice == 1;
    bool          isNumber = (word != "HEAD" && word != "WRITE") || choice == 1 || choice == 2 || (word == "WRITE" && choice == 3);



    m_diskTrack.SetVisible        (isDisk && fieldSet.trackLabel != nullptr && isNumber);
    m_diskSector.SetVisible       (isDisk && fieldSet.sectorLabel != nullptr && isField);
    m_diskNumber.SetVisible       (isDisk && fieldSet.numberLabel != nullptr);
    m_diskChoice.SetVisible       (isDisk && fieldSet.choiceLabel != nullptr);
    m_diskPassed.SetVisible       (isDisk && fieldSet.isPassed);
    m_diskBadChecksum.SetVisible  (isDisk && fieldSet.isBadChecksum);
    m_diskWrongTrack.SetVisible   (isDisk && fieldSet.isWrongTrack);
    m_diskBump.SetVisible         (isDisk && fieldSet.isBump);
    m_diskEvery.SetVisible        (isDisk && fieldSet.isEvery);
    m_diskAddressMarks.SetVisible (isDisk && fieldSet.isAddressMarks && isField);
    m_diskDataMarks.SetVisible    (isDisk && fieldSet.isDataMarks && isField);
    m_diskThirteen.SetVisible     (isDisk && fieldSet.isAddressMarks && isField);

    m_diskTrackLabel.SetText  (fieldSet.trackLabel  != nullptr ? fieldSet.trackLabel  : L"");
    m_diskSectorLabel.SetText (fieldSet.sectorLabel != nullptr ? fieldSet.sectorLabel : L"");
    m_diskNumberLabel.SetText (fieldSet.numberLabel != nullptr ? fieldSet.numberLabel : L"");
    m_diskChoiceLabel.SetText (fieldSet.choiceLabel != nullptr ? fieldSet.choiceLabel : L"");

    m_diskTrack.SetPlaceholder   (fieldSet.isRange || (word == "WRITE" && choice != 1) ? L"A hex track or range: 11 or 10:12" : L"A hex track number");
    m_diskSector.SetPlaceholder  (L"A hex sector number");
    m_diskNumber.SetPlaceholder  ((word == "ADDRESS") ? L"A decimal volume, as in CATALOG" : L"A hex block number");
    m_diskTrackLabel.SetRequired (fieldSet.isRange || (word == "WRITE" && choice != 1));

    m_choiceTip.SetText ((word == "HEAD")  ? L"Any position\tevery move of the head\n"
                                             L"Whole tracks\tthe head arriving on a track in the range\n"
                                             L"Quarter tracks\tthe same in quarter tracks, four to a track\n"
                                             L"Any half track\tthe head arriving between two tracks"
                       : (word == "WRITE") ? L"Any write\tthe drive entering write mode\n"
                                             L"A sector\ta data prologue written after that sector's address field\n"
                                             L"Whole tracks\twrite mode entered on a track in the range\n"
                                             L"Quarter tracks\tthe same in quarter tracks, four to a track\n"
                                             L"Blocked by write protection\ta write the drive drops"
                       :                     L"");
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
    fields.addressMarks  = SourcePathList::WideToUtf8 (m_diskAddressMarks.GetText());
    fields.dataMarks     = SourcePathList::WideToUtf8 (m_diskDataMarks.GetText());
    fields.isThirteen    = m_diskThirteen.IsChecked();
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

    //  A breakpoint set on a source line is edited as one: its file and line.
    if (m_sourcePlace.has_value())
    {
        fields.type    = Type::SourceLine;
        fields.file    = SourcePathList::WideToUtf8 (m_sourcePlace->first);
        fields.address = std::to_string (m_sourcePlace->second);
    }

    m_type = fields.type;

    for (DxuiLabel * label : { &m_accessLabel, &m_eventLabel, &m_driveLabel, &m_fileLabel, &m_modeLabel, &m_addressLabel, &m_valueLabel, &m_conditionLabel, &m_logLabel })
    {
        label->SetTextRole  (DxuiTextRole::Body);
        label->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    }

    m_accessLabel.SetText    (L"Access");
    m_eventLabel.SetText     (L"Disk event");
    m_driveLabel.SetText     (L"Drive");
    m_fileLabel.SetText      (L"File");
    m_modeLabel.SetText      (L"Stops");
    m_addressLabel.SetText   (L"Address or range");
    m_conditionLabel.SetText (L"Condition");
    m_logLabel.SetText       (L"Log message");
    m_logGo.SetLabel         (L"Don't break");
    m_logGo.SetChecked       (fields.isLogGo);
    m_log.SetPlaceholder     (L"Text with {expressions}");

    m_accessRead.SetLabel    (L"Read");
    m_accessWrite.SetLabel   (L"Write");
    m_accessRead.SetChecked  (fields.type != Type::Write);
    m_accessWrite.SetChecked (fields.type != Type::Read);

    m_mode.SetItems     ({ L"After the access", L"Before the access" });
    m_mode.SetSelected  ((int) fields.mode);
    m_mode.SetPopupHost (GetPopupHost());

    m_event.SetItems     (std::vector<std::wstring> (std::begin (s_kDiskEventNames), std::end (s_kDiskEventNames)));
    m_event.SetSelected  (fields.event);
    m_event.SetSelect    ([this] (int) { ShowFieldsForType(); });
    m_event.SetPopupHost (GetPopupHost());

    m_drive.SetItems     ({ L"Either drive", L"Drive 1", L"Drive 2" });
    m_drive.SetSelected  (fields.drive);
    m_drive.SetPopupHost (GetPopupHost());

    m_file.SetItems     (m_sourceFiles);
    m_file.SetSelected  (std::ranges::contains (m_sourceFiles, SourcePathList::Utf8ToWide (fields.file))
                             ? (int) (std::ranges::find (m_sourceFiles, SourcePathList::Utf8ToWide (fields.file)) - m_sourceFiles.begin()) : 0);
    m_file.SetPopupHost (GetPopupHost());

    {
        DiskFields    disk     = ParseDiskArguments (fields.event, fields.arguments);
        DiskFieldSet  fieldSet = GetDiskFieldSet (fields.event);

        for (DxuiLabel * label : { &m_diskTrackLabel, &m_diskSectorLabel, &m_diskNumberLabel, &m_diskChoiceLabel, &m_diskAddressMarksLabel, &m_diskDataMarksLabel })
        {
            label->SetTextRole  (DxuiTextRole::Body);
            label->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
        }

        m_diskAddressMarksLabel.SetText   (L"Address prologue");
        m_diskDataMarksLabel.SetText      (L"Data prologue");
        m_diskAddressMarks.SetPlaceholder (L"Three hex bytes, ?? for any");
        m_diskDataMarks.SetPlaceholder    (L"Three hex bytes, ?? for any");

        m_diskTrack.SetText        (SourcePathList::Utf8ToWide (disk.track));
        m_diskSector.SetText       (SourcePathList::Utf8ToWide (disk.sector));
        m_diskNumber.SetText       (SourcePathList::Utf8ToWide (disk.number));
        m_diskAddressMarks.SetText (SourcePathList::Utf8ToWide (disk.addressMarks));
        m_diskDataMarks.SetText    (SourcePathList::Utf8ToWide (disk.dataMarks));

        m_diskChoice.SetItems     (fieldSet.choices);
        m_diskChoice.SetSelected  (disk.choice);
        m_diskChoice.SetSelect    ([this] (int) { ShowFieldsForType(); });
        m_diskChoice.SetPopupHost (GetPopupHost());
        m_diskFieldSetEvent = fields.event;

        m_diskPassed.SetLabel      (L"Fields passed over while searching");
        m_diskBadChecksum.SetLabel (L"Bad checksum");
        m_diskWrongTrack.SetLabel  (L"Track differs from the head's");
        m_diskBump.SetLabel        (L"Step against the end stop");
        m_diskEvery.SetLabel       (L"Every selection, including the same drive");
        m_diskThirteen.SetLabel    (L"13-sector format (DOS 3.2)");

        m_diskPassed.SetChecked      (disk.isPassed);
        m_diskBadChecksum.SetChecked (disk.isBadChecksum);
        m_diskWrongTrack.SetChecked  (disk.isWrongTrack);
        m_diskBump.SetChecked        (disk.isBump);
        m_diskEvery.SetChecked       (disk.isEvery);
        m_diskThirteen.SetChecked    (disk.isThirteen);
    }

    for (DxuiTextInput * box : { &m_address, &m_value, &m_condition, &m_log, &m_diskTrack, &m_diskSector, &m_diskNumber, &m_diskAddressMarks, &m_diskDataMarks })
    {
        box->SetTheme        (m_theme);
        box->SetHwnd         (GetHwnd());
        box->SetTextRenderer (GetTextRenderer());
        box->SetOnFocusLost  ([this] { m_isCheckDue = true; });
    }

    m_address.SetText   (SourcePathList::Utf8ToWide ((fields.type == Type::Disk) ? std::string() : fields.address));
    m_newAddress = m_newAddress.has_value() ? std::optional<std::wstring> (SourcePathList::Utf8ToWide (fields.address)) : std::nullopt;
    m_value.SetText     (SourcePathList::Utf8ToWide (fields.value));
    m_condition.SetText (SourcePathList::Utf8ToWide (fields.condition));
    m_log.SetText       (SourcePathList::Utf8ToWide (fields.log));

    for (DxuiInfoTip * tip : { &m_conditionTip, &m_eventTip, &m_choiceTip, &m_logTip })
    {
        tip->SetPopupHost (GetPopupHost());
        tip->SetDpi       (GetDpi());
    }

    m_conditionTip.SetText (GetConditionTip (fields.type));
    m_eventTip.SetText     (GetDiskEventTip());
    m_logTip.SetText       (GetLogTip (fields.type));

    m_body = CreateDialogContent<BreakpointDialogPanel>();
    m_body->SetTextMeasure (GetTextRenderer(), m_theme);
    m_body->Add (m_eventLabel,               m_event, &m_eventTip);
    m_body->Add (m_driveLabel,               m_drive);
    m_body->Add (m_diskChoiceLabel,          m_diskChoice, &m_choiceTip);
    m_body->Add (m_diskTrackLabel,           m_diskTrack);
    m_body->Add (m_diskSectorLabel,          m_diskSector);
    m_body->Add (m_diskNumberLabel,          m_diskNumber);
    m_body->Add (m_diskCheckLabels[0],       m_diskPassed);
    m_body->Add (m_diskCheckLabels[1],       m_diskBadChecksum);
    m_body->Add (m_diskCheckLabels[2],       m_diskWrongTrack);
    m_body->Add (m_diskCheckLabels[3],       m_diskBump);
    m_body->Add (m_diskCheckLabels[4],       m_diskEvery);
    m_body->Add (m_diskAddressMarksLabel,    m_diskAddressMarks);
    m_body->Add (m_diskCheckLabels[5],       m_diskThirteen);
    m_body->Add (m_diskDataMarksLabel,       m_diskDataMarks);
    m_body->Add (m_accessLabel,              m_accessRead, nullptr, &m_accessWrite);
    m_body->Add (m_modeLabel,                m_mode);
    m_body->Add (m_fileLabel,                m_file);
    m_body->Add (m_addressLabel,             m_address);
    m_body->Add (m_valueLabel,               m_value);
    m_body->Add (m_conditionLabel,           m_condition, &m_conditionTip);
    m_body->Add (m_logLabel,                 m_log, &m_logTip);
    m_body->Add (m_logGoLabel,               m_logGo);
    m_body->AddError (m_error);

    ShowFieldsForType();

    ok = AddDialogButton (L"OK", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    ok->SetOnClick ([this]() { Confirm(); });

    //  The first field the type uses.
    SetInitialFocus (GetVisibleFields (fields.type).address   ? (IDxuiControl *) &m_address
                   : GetVisibleFields (fields.type).value     ? (IDxuiControl *) &m_value
                   : GetVisibleFields (fields.type).condition ? (IDxuiControl *) &m_condition
                   : GetVisibleFields (fields.type).event     ? (IDxuiControl *) &m_event
                   :                                            (IDxuiControl *) ok);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::Confirm
//
//  The definition the fields make, which closes the dialog once the
//  validator accepts it; otherwise its reason is drawn under the fields and
//  the dialog grows to fit it.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointDialog::Confirm()
{
    BreakpointDefinition  definition = MakeEditedDefinition();
    std::string           reason     = m_validator ? m_validator (definition) : std::string();



    m_error.SetMessage (SourcePathList::Utf8ToWide (reason));

    if (!reason.empty())
    {
        Resize();
        return;
    }

    m_result = Result { definition, SourcePathList::WideToUtf8 (m_log.GetText()), m_logGo.IsChecked() };
    EndDialog (IDOK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::MakeEditedDefinition
//
////////////////////////////////////////////////////////////////////////////////

BreakpointDefinition BreakpointDialog::MakeEditedDefinition() const
{
    return ToDefinition (GetEditedFields());
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::GetEditedFields
//
////////////////////////////////////////////////////////////////////////////////

BreakpointDialog::Fields BreakpointDialog::GetEditedFields() const
{
    Fields  edited;



    edited.type      = GetSelectedType();
    edited.mode      = (WatchMode) m_mode.GetSelectedIndex();
    edited.address   = SourcePathList::WideToUtf8 (m_address.GetText());
    edited.value     = SourcePathList::WideToUtf8 (m_value.GetText());
    edited.condition = SourcePathList::WideToUtf8 (m_condition.GetText());
    edited.event     = m_event.GetSelectedIndex();
    edited.drive     = m_drive.GetSelectedIndex();
    edited.file      = (m_file.GetSelectedIndex() >= 0 && m_file.GetSelectedIndex() < (int) m_sourceFiles.size()) ? SourcePathList::WideToUtf8 (m_sourceFiles[(size_t) m_file.GetSelectedIndex()]) : std::string();
    edited.arguments = MakeDiskArguments (edited.event, ReadDiskFields());
    edited.log       = SourcePathList::WideToUtf8 (m_log.GetText());
    edited.isLogGo   = m_logGo.IsChecked();

    return edited;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::CheckFields
//
//  The check OK makes, after the focus moves off a field: its reason under the
//  fields, or none while a field the type requires is empty.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointDialog::CheckFields()
{
    Fields        edited = GetEditedFields();
    std::wstring  reason = (m_validator && IsCheckedOnFocusLoss (edited)) ? SourcePathList::Utf8ToWide (m_validator (ToDefinition (edited))) : std::wstring();



    if (reason != m_error.GetMessage())
    {
        m_error.SetMessage (reason);
        Resize();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::GetConditionTip
//
//  The items a condition can read for the type, each kind under its general
//  term, with examples that use them. Every condition reads the registers,
//  the flags and memory; a memory breakpoint adds its access, and a disk or
//  a condition breakpoint the disk's values.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BreakpointDialog::GetConditionTip (Type type)
{
    bool          isMemory = type == Type::Read || type == Type::Write || type == Type::ReadWrite || type == Type::Value;
    bool          isDisk   = type == Type::Disk || type == Type::Condition;
    std::wstring  tip;



    tip  = std::format (L"{} breakpoint can set conditions using these items:\n", GetTypeArticle (type));
    tip += L"The registers: A, X, Y, S, P and PC\n";
    tip += L"The flags: P.C, P.Z, P.I, P.D, P.B, P.V and P.N, each 0 or 1\n";
    tip += L"Memory: *addr for the byte at addr, and symbols by name\n";

    if (isMemory)
    {
        tip += L"The access: ACCESS for its address and VALUE for its byte, typed as those words\n";
    }

    if (isDisk)
    {
        tip += GetDiskValuesLine();
    }

    tip += L"All numbers are in hex, with or without a $ prefix. For decimal, prefix with 0n, for example 0n123.\n";
    tip += L"\n";
    tip += L"Examples:\n";
    tip += L"P.C && A >= 80: C set and A at least $80\n";
    tip += L"*FB == 0 && X != 0: the byte at $FB zero, X not";

    if (isMemory)
    {
        tip += L"\nVALUE == 8D: the byte is $8D, a return";
    }

    if (isDisk)
    {
        tip += L"\nDISK.QTRACK == 44: the head on track 17";
    }

    return tip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::GetTypeArticle
//
//  "A memory", "An execution" and the like: the type's heading on the New
//  drop-down, but a condition or a disk breakpoint by the row's label.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BreakpointDialog::GetTypeArticle (Type type)
{
    const std::vector<TypeRow>  & rows    = GetTypeRows();
    std::wstring                  heading;



    for (int i = GetRowOf (type); i >= 0 && heading.empty(); i--)
    {
        heading = rows[(size_t) i].type.has_value() ? std::wstring() : std::wstring (rows[(size_t) i].label);
    }

    heading = (type == Type::Condition) ? L"condition" : (type == Type::Disk) ? L"disk" : heading;

    if (!heading.empty() && heading != L"BASIC")
    {
        heading[0] = (wchar_t) towlower (heading[0]);
    }

    return (wcschr (L"aeiouAEIOU", heading.empty() ? L'x' : heading[0]) != nullptr ? L"An " : L"A ") + heading;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::IsCheckedOnFocusLoss
//
//  Moving the focus off a field checks the fields, except while a field the
//  type requires is empty: an empty field is reported only by OK, so a
//  click on the Condition tip before typing reports nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointDialog::IsCheckedOnFocusLoss (const Fields & fields)
{
    VisibleFields  used         = GetVisibleFields (fields.type);
    bool           isAddressOut = used.address && fields.address.empty();
    bool           isValueOut   = used.value   && fields.value.empty();
    bool           isRuleOut    = fields.type == Type::Condition && fields.condition.empty();



    return !isAddressOut && !isValueOut && !isRuleOut;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::GetDiskEventTip
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * BreakpointDialog::GetDiskEventTip()
{
    return L"Read sector\ta physical sector's data read by a program, not passed over\n"
           L"Read address field\tan address field passing under the head\n"
           L"Move head\tthe head arriving on a track\n"
           L"Write disk\twrite mode, a physical sector written, or a write blocked\n"
           L"Read or write DOS 3.3 sector\ta DOS 3.3 logical sector, through its physical sector\n"
           L"Read or write ProDOS block\tboth halves of a ProDOS block\n"
           L"Call RWTS\tDOS 3.3's RWTS called with that request, before it runs\n"
           L"Call ProDOS driver\tProDOS's disk driver called, before it runs\n"
           L"Turn motor on or off\tthe motor switched on or off, or stopping\n"
           L"Select drive\tthe other drive selected\n"
           L"Insert, eject disk image (Casso)\ta disk image mounted or ejected in Casso; the drive itself can't report this";
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::GetDiskValuesLine
//
//  The disk's values an expression reads, the address field's apart from
//  where the head is.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * BreakpointDialog::GetDiskValuesLine()
{
    return L"The disk: DISK.DRIVE, DISK.QTRACK (where the head is, in quarter tracks), DISK.QTRACK1, DISK.QTRACK2, DISK.MOTOR, "
           L"DISK.WRITING, DISK.PROTECTED, DISK.INSERTED, and the last address field read: DISK.ADDR.TRACK, "
           L"DISK.ADDR.SECTOR and DISK.ADDR.VOLUME\n";
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::GetLogTip
//
//  What a message can read for the type, as the condition tip lists it: a
//  memory breakpoint adds its access, and a disk or condition breakpoint
//  the disk's values, with an example that uses them.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BreakpointDialog::GetLogTip (Type type)
{
    bool          isMemory = type == Type::Read || type == Type::Write || type == Type::ReadWrite || type == Type::Value;
    bool          isDisk   = type == Type::Disk || type == Type::Condition;
    std::wstring  tip;



    tip  = L"Each hit writes this message to the console, with each expression in braces evaluated at the hit, in hex.\n";
    tip += L"An expression reads the registers, the flags, memory and symbols, as a condition does.\n";
    tip += L"A .b or .w after a symbol reads its byte or word.\n";

    if (isMemory)
    {
        tip += L"The access: ACCESS for its address and VALUE for its byte\n";
    }

    if (isDisk)
    {
        tip += GetDiskValuesLine();
    }

    tip += L"\n";
    tip += isDisk   ? L"Example: T={DISK.ADDR.TRACK} S={DISK.ADDR.SECTOR}\n"
         : isMemory ? L"Example: {ACCESS}={VALUE}\n"
         :            L"Example: A={A} count={count.b}\n";
    tip += L"\n";
    tip += L"With Don't break checked, a hit is counted and logged and the machine runs on; otherwise it stops.";

    return tip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::RunModal
//
////////////////////////////////////////////////////////////////////////////////

std::optional<BreakpointDialog::Result> BreakpointDialog::RunModal (HWND owner, const IDxuiTheme * theme, const BreakpointInfo & info, std::optional<Type> type, Validator validator,
                                                                    std::vector<std::wstring> sourceFiles, std::optional<SourcePlace> sourcePlace)
{
    HRESULT                            hr     = S_OK;
    std::unique_ptr<BreakpointDialog>  dialog = std::make_unique<BreakpointDialog>();
    DxuiWindow::CreateParams           params;



    dialog->m_theme      = theme;
    dialog->m_info       = info;
    dialog->m_presetType = type;
    dialog->m_newAddress = (info.id < 0) ? std::optional<std::wstring> (std::wstring()) : std::nullopt;
    dialog->m_validator  = std::move (validator);
    dialog->m_sourceFiles = std::move (sourceFiles);
    dialog->m_sourcePlace = std::move (sourcePlace);

    params.title                    = GetTitle (dialog->m_sourcePlace.has_value() ? Type::SourceLine : type.value_or (GetType (info)), (info.id < 0) ? std::nullopt : std::optional<int> (info.id));
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 500, 300 };
    params.minSizeDip               = { 440, 120 };
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog->Create (params);

    if (FAILED (hr))
    {
        return std::nullopt;
    }

    dialog->SetTheme (theme);
    dialog->FitToMaxSize();
    dialog->ShowModalDialog (IDOK);

    return dialog->m_result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::RunNew
//
////////////////////////////////////////////////////////////////////////////////

std::optional<BreakpointDialog::Result> BreakpointDialog::RunNew (HWND owner, const IDxuiTheme * theme, Type type, Word pc, Validator validator,
                                                                  std::vector<std::wstring> sourceFiles)
{
    BreakpointInfo  info;



    info.id      = -1;
    info.kind    = BreakpointKind::Address;
    info.address = pc;
    info.last    = pc;

    return RunModal (owner, theme, info, type, std::move (validator), std::move (sourceFiles));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::OnDialogTick
//
//  The info tips appear after their dwell and fade on the dialog's ticks, as
//  nothing else ticks a dialog's controls. A field the focus moved off is
//  checked here once no mouse button is down, so a press on OK or Cancel is
//  not moved by the dialog growing under it.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointDialog::OnDialogTick()
{
    int64_t  nowMs     = (int64_t) GetTickCount64();
    bool     isPressed = (DxuiInputState::GetAsyncKeyState (VK_LBUTTON) & 0x8000) != 0 || (DxuiInputState::GetAsyncKeyState (VK_RBUTTON) & 0x8000) != 0;



    m_conditionTip.Tick (nowMs);
    m_eventTip.Tick     (nowMs);
    m_choiceTip.Tick    (nowMs);
    m_logTip.Tick       (nowMs);

    if (m_isCheckDue && !isPressed)
    {
        m_isCheckDue = false;
        CheckFields();
    }
}
