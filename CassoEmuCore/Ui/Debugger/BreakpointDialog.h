#pragma once

#include "Debugger/Reply.h"





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
//  Edits a breakpoint's type and the fields that type takes, and returns
//  the definition BPEDIT reads: BP, BPMR, BPMW, BPM, BPMV, BPR or BRKOP with
//  its arguments, a watchpoint's BEFORE and any IF condition, BRK ON or
//  BRKINT ON, BPDISK with its drive, event, arguments and condition, or
//  BPBASIC, BPBASICVAR or BRKBASIC ON.
//
////////////////////////////////////////////////////////////////////////////////

class BreakpointDialog : public DxuiDialogWindow
{
public:
    //  The types offered, in the order the list shows them.
    enum class Type { Execute, Read, Write, ReadWrite, Value, Register, Opcode, Brk, Interrupt, Disk, BasicLine, BasicVariable, BasicError };

    //  The values of the dialog's controls. A disk breakpoint's event is an
    //  index into DiskBreakSpecText::GetEventWords, its drive 0 for either,
    //  and its arguments what BPDISK takes after the event word.
    struct Fields
    {
        Type         type      = Type::Execute;
        WatchMode    mode      = WatchMode::After;
        std::string  address;
        std::string  value;
        std::string  condition;
        int          event     = 0;
        int          drive     = 0;
        std::string  arguments;
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

    static Fields  GetDiskFields (const BreakpointInfo & info, Fields fields);

    const IDxuiTheme            * m_theme          = nullptr;
    BreakpointInfo                m_info;
    std::optional<std::string>    m_definition;
    DxuiLabel                     m_typeLabel;
    DxuiLabel                     m_modeLabel;
    DxuiLabel                     m_addressLabel;
    DxuiLabel                     m_valueLabel;
    DxuiLabel                     m_conditionLabel;
    DxuiLabel                     m_eventLabel;
    DxuiLabel                     m_driveLabel;
    DxuiComboBox                  m_type;
    DxuiComboBox                  m_mode;
    DxuiComboBox                  m_event;
    DxuiComboBox                  m_drive;
    DxuiTextInput                 m_address;
    DxuiTextInput                 m_value;
    DxuiTextInput                 m_condition;
    BreakpointDialogPanel       * m_body           = nullptr;
};
