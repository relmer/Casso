#include "Pch.h"

#include "Ui/Debugger/BreakpointDialog.h"
#include "Debugger/Source/SourcePathList.h"





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
    default:                                                                             break;
    }

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
    case BreakpointKind::Register:    return Type::Register;
    case BreakpointKind::Opcode:      return Type::Opcode;
    case BreakpointKind::MemoryValue: return Type::Value;
    case BreakpointKind::Brk:         return Type::Brk;
    case BreakpointKind::Interrupt:   return Type::Interrupt;
    case BreakpointKind::Memory:
    case BreakpointKind::Io:
        return (info.access == WatchAccess::Read)  ? Type::Read
             : (info.access == WatchAccess::Write) ? Type::Write
                                                   : Type::ReadWrite;
    default:                          return Type::Execute;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::ShowFieldsForType
//
//  The value field means what the type needs it to, and only a watchpoint
//  can stop before its access.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointDialog::ShowFieldsForType()
{
    Type  type      = (Type) m_type.GetSelectedIndex();
    bool  isWatched = type == Type::Read || type == Type::Write || type == Type::ReadWrite;



    m_mode.SetEnabled (isWatched);

    switch (type)
    {
    case Type::Value:    m_valueLabel.SetText (L"Value (hex byte)");       break;
    case Type::Register: m_valueLabel.SetText (L"Register test (A=41)");  break;
    case Type::Opcode:   m_valueLabel.SetText (L"Opcode (hex)");          break;
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



    for (DxuiLabel * label : { &m_typeLabel, &m_modeLabel, &m_addressLabel, &m_valueLabel, &m_conditionLabel })
    {
        label->SetTextRole  (DxuiTextRole::Body);
        label->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    }

    m_typeLabel.SetText      (L"Type");
    m_modeLabel.SetText      (L"Stops");
    m_addressLabel.SetText   (L"Address or range");
    m_conditionLabel.SetText (L"Condition (A == 41)");

    m_type.SetItems    ({ L"Execute", L"Read", L"Write", L"Read or write", L"Memory value", L"Register", L"Opcode", L"BRK", L"BRK on interrupt" });
    m_type.SetSelected ((int) fields.type);
    m_type.SetSelect   ([this] (int) { ShowFieldsForType(); Invalidate(); });
    m_type.SetPopupHost (GetPopupHost());

    m_mode.SetItems     ({ L"After the access", L"Before the access" });
    m_mode.SetSelected  ((int) fields.mode);
    m_mode.SetPopupHost (GetPopupHost());

    for (DxuiTextInput * box : { &m_address, &m_value, &m_condition })
    {
        box->SetTheme        (m_theme);
        box->SetHwnd         (GetHwnd());
        box->SetTextRenderer (GetTextRenderer());
    }

    m_address.SetText   (SourcePathList::Utf8ToWide (fields.address));
    m_value.SetText     (SourcePathList::Utf8ToWide (fields.value));
    m_condition.SetText (SourcePathList::Utf8ToWide (fields.condition));
    ShowFieldsForType();

    m_body = CreateDialogContent<BreakpointDialogPanel>();
    m_body->Add (m_typeLabel,      m_type);
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
        m_definition     = MakeDefinition (edited);
        EndDialog (IDOK);
    });

    SetInitialFocus (&m_address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog::Ask
//
////////////////////////////////////////////////////////////////////////////////

std::optional<std::string> BreakpointDialog::Ask (HWND owner, const IDxuiTheme * theme, const BreakpointInfo & info)
{
    HRESULT                   hr     = S_OK;
    BreakpointDialog          dialog;
    DxuiWindow::CreateParams  params;



    dialog.m_theme = theme;
    dialog.m_info  = info;

    params.title                    = std::format (L"Breakpoint #{}", info.id);
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 460, 320 };
    params.minSizeDip               = { 400, 320 };
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
