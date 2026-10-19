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
        int  right = (each.tip != nullptr) ? boundsDip.right - tip : boundsDip.right;

        each.label->SetVisible (each.field->IsVisible());

        if (each.tip != nullptr)
        {
            each.tip->SetVisible (each.field->IsVisible());
        }

        if (!each.field->IsVisible())
        {
            continue;
        }

        each.label->Layout (RECT { boundsDip.left, y, split, y + row }, scaler);
        each.field->Layout (RECT { split, y, right, y + row }, scaler);

        if (each.tip != nullptr)
        {
            each.tip->Layout (RECT { right, y, boundsDip.right, y + row }, scaler);
        }

        y += row + gap;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::GetTypeRows
//
//  The types under headings: code, memory accesses, BASIC's,
//  then the machine's own.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<BreakpointDialog::TypeRow> & BreakpointDialog::GetTypeRows()
{
    static const std::vector<TypeRow>  rows =
    {
        { L"Code",                         std::nullopt              },
        { L"Execute",                      Type::Execute             },
        { L"Condition",                    Type::Condition           },
        { L"Opcode",                       Type::Opcode              },
        { L"BRK",                          Type::Brk                 },
        { L"BRK on interrupt",             Type::Interrupt           },
        { L"Memory",                       std::nullopt              },
        { L"Read",                         Type::Read                },
        { L"Write",                        Type::Write               },
        { L"Read or write",                Type::ReadWrite           },
        { L"Memory value",                 Type::Value               },
        { L"Reading uninitialized memory", Type::UnwrittenRead       },
        { L"BASIC",                        std::nullopt              },
        { L"BASIC line",                   Type::BasicLine           },
        { L"BASIC variable change",        Type::BasicVariable       },
        { L"BASIC error",                  Type::BasicError          },
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



    for (size_t i = 0; i < rows.size(); i++)
    {
        if (rows[i].type == type)
        {
            return (int) i;
        }
    }

    return 1;
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
    shown.value     = type == Type::Value || type == Type::Opcode || type == Type::Beam;
    shown.condition = type == Type::Execute || isWatched || type == Type::Value || type == Type::Disk || type == Type::Condition;
    shown.address   = type == Type::Execute || isWatched || type == Type::Value || type == Type::Disk || type == Type::BasicLine ||
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
    size_t         event = std::clamp<size_t> (m_event.GetSelectedIndex(), 0, DiskBreakSpecText::GetEventWords().size() - 1);



    m_mode.SetVisible      (shown.mode);
    m_event.SetVisible     (shown.event);
    m_drive.SetVisible     (shown.drive);
    m_value.SetVisible     (shown.value);
    m_address.SetVisible   (shown.address);
    m_condition.SetVisible (shown.condition);

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
    case Type::Disk:
        m_addressLabel.SetText   (L"Arguments");
        m_address.SetPlaceholder (SourcePathList::Utf8ToWide (DiskBreakSpecText::GetEventWords()[event].syntax));
        break;

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
////////////////////////////////////////////////////////////////////////////////

BreakpointDialog::Type BreakpointDialog::GetSelectedType() const
{
    int  row = m_type.GetSelectedIndex();



    if (row < 0 || row >= (int) GetTypeRows().size() || !GetTypeRows()[(size_t) row].type.has_value())
    {
        return Type::Execute;
    }

    return *GetTypeRows()[(size_t) row].type;
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



    //  A type no breakpoint kind has, such as a video line, opens on that
    //  type.
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
    m_conditionLabel.SetText (L"Condition");

    {
        std::vector<std::wstring>  labels;
        std::vector<bool>          enabled;

        for (const TypeRow & row : GetTypeRows())
        {
            labels.push_back  (row.label);
            enabled.push_back (row.type.has_value());
        }

        m_type.SetItems        (labels);
        m_type.SetItemsEnabled (enabled);
    }

    m_type.SetSelected (GetRowOf (fields.type));
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
    m_body->Add (m_typeLabel,      m_type);
    m_body->Add (m_eventLabel,     m_event);
    m_body->Add (m_driveLabel,     m_drive);
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
        edited.arguments = edited.address;
        m_definition     = MakeDefinition (edited);
        EndDialog (IDOK);
    });

    //  The first text field the type uses.
    SetInitialFocus (GetVisibleFields (fields.type).address ? (IDxuiControl *) &m_address
                   : GetVisibleFields (fields.type).value   ? (IDxuiControl *) &m_value
                   :                                          (IDxuiControl *) &m_type);
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

    params.title                    = (info.id < 0) ? std::wstring (L"New breakpoint") : std::format (L"Breakpoint #{}", info.id);
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





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::RunNew
//
////////////////////////////////////////////////////////////////////////////////

std::optional<std::string> BreakpointDialog::RunNew (HWND owner, const IDxuiTheme * theme, Word pc)
{
    BreakpointInfo  info;



    info.id      = -1;
    info.kind    = BreakpointKind::Address;
    info.address = pc;
    info.last    = pc;

    return RunModal (owner, theme, info, Type::Execute);
}
