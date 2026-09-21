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

std::string BreakpointDialog::MakeDefinition (Type type, const std::string & address, const std::string & value, const std::string & condition)
{
    std::string  clause = condition.empty() ? std::string() : " IF " + condition;



    switch (type)
    {
    case Type::Read:      return "BPMR " + address + clause;
    case Type::Write:     return "BPMW " + address + clause;
    case Type::ReadWrite: return "BPM " + address + clause;
    case Type::Value:     return "BPMV " + address + " " + value + clause;
    case Type::Register:  return "BPR " + value;
    case Type::Opcode:    return "BRKOP " + value;
    default:              return "BP " + address + clause;
    }
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
//  BreakpointDialog::ShowValueLabel
//
//  The third field means what the type needs it to.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointDialog::ShowValueLabel()
{
    switch ((Type) m_type.GetSelectedIndex())
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
    DxuiButton   * ok      = nullptr;
    std::string    range   = (m_info.last > m_info.address) ? std::format ("{:04X}:{:04X}", m_info.address, m_info.last)
                                                            : std::format ("{:04X}", m_info.address);
    std::string    value;
    std::string    condition = m_info.condition;
    Type           type      = GetType (m_info);



    if      (type == Type::Value)    { value = std::format ("{:02X}", m_info.value.value_or (0)); }
    else if (type == Type::Opcode)   { value = std::format ("{:02X}", m_info.opcode); }
    else if (type == Type::Register) { value = m_info.condition; condition.clear(); }

    for (DxuiLabel * label : { &m_typeLabel, &m_addressLabel, &m_valueLabel, &m_conditionLabel })
    {
        label->SetTextRole  (DxuiTextRole::Body);
        label->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    }

    m_typeLabel.SetText      (L"Type");
    m_addressLabel.SetText   (L"Address or range");
    m_conditionLabel.SetText (L"Condition (A == 41)");

    m_type.SetItems    ({ L"Execute", L"Read", L"Write", L"Read or write", L"Memory value", L"Register", L"Opcode" });
    m_type.SetSelected ((int) type);
    m_type.SetSelect   ([this] (int) { ShowValueLabel(); Invalidate(); });
    m_type.SetPopupHost (GetPopupHost());

    for (DxuiTextInput * box : { &m_address, &m_value, &m_condition })
    {
        box->SetTheme        (m_theme);
        box->SetHwnd         (GetHwnd());
        box->SetTextRenderer (GetTextRenderer());
    }

    m_address.SetText   (SourcePathList::Utf8ToWide (range));
    m_value.SetText     (SourcePathList::Utf8ToWide (value));
    m_condition.SetText (SourcePathList::Utf8ToWide (condition));
    ShowValueLabel();

    m_body = CreateDialogContent<BreakpointDialogPanel>();
    m_body->Add (m_typeLabel,      m_type);
    m_body->Add (m_addressLabel,   m_address);
    m_body->Add (m_valueLabel,     m_value);
    m_body->Add (m_conditionLabel, m_condition);

    ok = AddDialogButton (L"OK", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    ok->SetOnClick ([this]()
    {
        m_definition = MakeDefinition ((Type) m_type.GetSelectedIndex(),
                                       SourcePathList::WideToUtf8 (m_address.GetText()),
                                       SourcePathList::WideToUtf8 (m_value.GetText()),
                                       SourcePathList::WideToUtf8 (m_condition.GetText()));
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
    params.initialSizeDip           = { 460, 280 };
    params.minSizeDip               = { 400, 280 };
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
