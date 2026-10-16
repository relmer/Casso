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

void BreakpointDialogPanel::Add (DxuiLabel & label, IDxuiControl & field)
{
    m_rows.push_back ({ &label, &field });
    Adopt (label);
    Adopt (field);
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
    int  y     = boundsDip.top;



    SetBounds (boundsDip);

    for (const auto & [label, field] : m_rows)
    {
        label->Layout (RECT { boundsDip.left, y, split, y + row }, scaler);
        field->Layout (RECT { split, y, boundsDip.right, y + row }, scaler);
        y += row + gap;
    }
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
    case Type::Register:  return "BPR " + fields.value;
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
    case Type::ExecuteWatch:
        return "BPA " + fields.address;
    default:              return "BP " + fields.address + clause;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::GetFields
//
//  A register breakpoint's predicate goes in the value field, since it has no
//  IF condition of its own.
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
    case Type::Register: fields.value = info.condition; fields.condition.clear();        break;
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
    case BreakpointKind::Register:      return Type::Register;
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
//  The value field means what the type needs it to, and only a watchpoint
//  can stop before its access. A disk breakpoint takes an event, a drive
//  and the event's arguments in the address field, whose placeholder is
//  the event's syntax line.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointDialog::ShowFieldsForType()
{
    Type    type      = (Type) m_type.GetSelectedIndex();
    bool    isWatched = type == Type::Read || type == Type::Write || type == Type::ReadWrite;
    bool    isDisk    = type == Type::Disk;
    bool    isBasic   = type == Type::BasicLine || type == Type::BasicVariable || type == Type::BasicError;
    bool    isMachine = type == Type::UnwrittenRead || type == Type::VideoLine || type == Type::Beam || type == Type::ExecuteWatch;
    size_t  event     = std::clamp<size_t> (m_event.GetSelectedIndex(), 0, DiskBreakSpecText::GetEventWords().size() - 1);



    m_mode.SetEnabled      (isWatched);
    m_event.SetEnabled     (isDisk);
    m_drive.SetEnabled     (isDisk);
    m_value.SetEnabled     (!isDisk && !isBasic && (!isMachine || type == Type::Beam));
    m_address.SetEnabled   (type != Type::BasicError && type != Type::UnwrittenRead);
    m_condition.SetEnabled (!isBasic && !isMachine);
    m_address.SetPlaceholder (isDisk ? SourcePathList::Utf8ToWide (DiskBreakSpecText::GetEventWords()[event].syntax) : std::wstring());

    switch (type)
    {
    case Type::Disk:          m_addressLabel.SetText (L"Arguments");                 break;
    case Type::BasicLine:     m_addressLabel.SetText (L"Line number");               break;
    case Type::BasicVariable: m_addressLabel.SetText (L"Variable (A, B%, C$, D(3))"); break;
    case Type::VideoLine:     m_addressLabel.SetText (L"Scanlines (hex, 64:6B)");    break;
    case Type::Beam:          m_addressLabel.SetText (L"Scanline (hex) or VBL");     break;
    default:                  m_addressLabel.SetText (L"Address or range");          break;
    }

    switch (type)
    {
    case Type::Value:    m_valueLabel.SetText (L"Value (hex byte)");       break;
    case Type::Register: m_valueLabel.SetText (L"Register test (A=41)");  break;
    case Type::Opcode:   m_valueLabel.SetText (L"Opcode or mnemonic");    break;
    case Type::Beam:     m_valueLabel.SetText (L"Cycle in the line (hex)"); break;
    default:             m_valueLabel.SetText (L"Value (unused)");        break;
    }
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



    //  A New entry for a type no breakpoint kind has, such as a video line,
    //  opens on that type.
    if (m_presetType.has_value())
    {
        fields.type = *m_presetType;
    }

    for (DxuiLabel * label : { &m_typeLabel, &m_eventLabel, &m_driveLabel, &m_modeLabel, &m_addressLabel, &m_valueLabel, &m_conditionLabel })
    {
        label->SetTextRole  (DxuiTextRole::Body);
        label->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    }

    m_typeLabel.SetText      (L"Type");
    m_eventLabel.SetText     (L"Disk event");
    m_driveLabel.SetText     (L"Drive");
    m_modeLabel.SetText      (L"Stops");
    m_addressLabel.SetText   (L"Address or range");
    m_conditionLabel.SetText (L"Condition (A == 41)");

    m_type.SetItems    ({ L"Execute", L"Read", L"Write", L"Read or write", L"Memory value", L"Register", L"Opcode", L"BRK", L"BRK on interrupt", L"Disk event",
                         L"BASIC line", L"BASIC variable change", L"BASIC error", L"Read of memory never written", L"Video line", L"Beam position",
                         L"Execute, with a watch" });
    m_type.SetSelected ((int) fields.type);
    m_type.SetSelect   ([this] (int) { ShowFieldsForType(); Invalidate(); });
    m_type.SetPopupHost (GetPopupHost());

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

    for (DxuiTextInput * box : { &m_address, &m_value, &m_condition })
    {
        box->SetTheme        (m_theme);
        box->SetHwnd         (GetHwnd());
        box->SetTextRenderer (GetTextRenderer());
    }

    m_address.SetText   (SourcePathList::Utf8ToWide ((fields.type == Type::Disk) ? fields.arguments : fields.address));
    m_value.SetText     (SourcePathList::Utf8ToWide (fields.value));
    m_condition.SetText (SourcePathList::Utf8ToWide (fields.condition));
    ShowFieldsForType();

    m_body = CreateDialogContent<BreakpointDialogPanel>();
    m_body->Add (m_typeLabel,      m_type);
    m_body->Add (m_eventLabel,     m_event);
    m_body->Add (m_driveLabel,     m_drive);
    m_body->Add (m_modeLabel,      m_mode);
    m_body->Add (m_addressLabel,   m_address);
    m_body->Add (m_valueLabel,     m_value);
    m_body->Add (m_conditionLabel, m_condition);

    ok = AddDialogButton (L"OK", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    ok->SetOnClick ([this]()
    {
        Fields  edited;



        edited.type      = (Type) m_type.GetSelectedIndex();
        edited.mode      = (WatchMode) m_mode.GetSelectedIndex();
        edited.address   = SourcePathList::WideToUtf8 (m_address.GetText());
        edited.value     = SourcePathList::WideToUtf8 (m_value.GetText());
        edited.condition = SourcePathList::WideToUtf8 (m_condition.GetText());
        edited.event     = m_event.GetSelectedIndex();
        edited.drive     = m_drive.GetSelectedIndex();
        edited.arguments = edited.address;
        m_definition     = MakeDefinition (edited);
        EndDialog (IDOK);
    });

    SetInitialFocus (&m_address);
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



    dialog.m_theme = theme;
    dialog.m_info  = info;
    dialog.m_presetType = type;

    params.title                    = std::format (L"Breakpoint #{}", info.id);
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 460, 396 };
    params.minSizeDip               = { 400, 396 };
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
