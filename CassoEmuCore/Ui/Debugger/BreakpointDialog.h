#pragma once

#include "Core/DxuiPanel.h"
#include "Debugger/Reply.h"
#include "Widgets/DxuiComboBox.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiTextInput.h"
#include "Window/DxuiDialogWindow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialogPanel
//
//  A label beside each field, one field a row.
//
////////////////////////////////////////////////////////////////////////////////

class BreakpointDialogPanel : public DxuiPanel
{
public:
    void  Add    (DxuiLabel & label, IDxuiControl & field);
    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;

    static constexpr int  kRowDip   = 30;
    static constexpr int  kGapDip   = 8;
    static constexpr int  kLabelDip = 130;

private:
    std::vector<std::pair<DxuiLabel *, IDxuiControl *>>  m_rows;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog
//
//  Edits a breakpoint's type and the fields that type takes, and gives back
//  the definition BPEDIT reads: BP, BPMR, BPMW, BPM, BPMV, BPR or BRKOP with
//  its arguments, a watchpoint's BEFORE and any IF condition, or BRK ON or
//  BRKINT ON.
//
////////////////////////////////////////////////////////////////////////////////

class BreakpointDialog : public DxuiDialogWindow
{
public:
    //  The types offered, in the order the list shows them.
    enum class Type { Execute, Read, Write, ReadWrite, Value, Register, Opcode, Brk, Interrupt };

    //  What the dialog's controls hold.
    struct Fields
    {
        Type         type      = Type::Execute;
        WatchMode    mode      = WatchMode::After;
        std::string  address;
        std::string  value;
        std::string  condition;
    };

    //  The definition for a type and its fields, as BPEDIT takes it.
    static std::string  MakeDefinition (const Fields & fields);

    //  The fields an existing breakpoint fills in.
    static Fields  GetFields (const BreakpointInfo & info);

    //  The type an existing breakpoint is.
    static Type  GetType (const BreakpointInfo & info);

    //  Runs the dialog modally over `owner`; the definition when OK.
    static std::optional<std::string>  Ask (HWND owner, const IDxuiTheme * theme, const BreakpointInfo & info);

protected:
    void  OnCreate() override;

private:
    void  ShowFieldsForType ();

    const IDxuiTheme            * m_theme          = nullptr;
    BreakpointInfo                m_info;
    std::optional<std::string>    m_definition;
    DxuiLabel                     m_typeLabel;
    DxuiLabel                     m_modeLabel;
    DxuiLabel                     m_addressLabel;
    DxuiLabel                     m_valueLabel;
    DxuiLabel                     m_conditionLabel;
    DxuiComboBox                  m_type;
    DxuiComboBox                  m_mode;
    DxuiTextInput                 m_address;
    DxuiTextInput                 m_value;
    DxuiTextInput                 m_condition;
    BreakpointDialogPanel       * m_body           = nullptr;
};
