#pragma once

#include "Debugger/Reply.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialogPanel
//
//  A label beside each field, one field a row, and an info tip at a row's
//  right end where it has one; a hidden field's row is left out, so the
//  rows of the visible ones are together.
//
////////////////////////////////////////////////////////////////////////////////

class BreakpointDialogPanel : public DxuiPanel
{
public:
    void  Add    (DxuiLabel & label, IDxuiControl & field, DxuiInfoTip * tip = nullptr);
    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;

    static constexpr int  kRowDip   = 30;
    static constexpr int  kGapDip   = 8;
    static constexpr int  kLabelDip = 130;
    static constexpr int  kTipDip   = 24;

private:
    struct Row
    {
        DxuiLabel     * label = nullptr;
        IDxuiControl  * field = nullptr;
        DxuiInfoTip   * tip   = nullptr;
    };

    std::vector<Row>  m_rows;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog
//
//  Edits a breakpoint's type and the fields that type takes, and returns
//  the definition BPEDIT reads: BP, BPMR, BPMW, BPM, BPMV or BRKOP with its
//  arguments, a data breakpoint's BEFORE and any IF condition, BPR IF and a
//  condition, BRK ON or BRKINT ON, BPDISK with its drive, event, arguments
//  and condition, or BPBASIC, BPBASICVAR or BRKBASIC ON. The type list is
//  grouped under headings, which cannot be chosen.
//
////////////////////////////////////////////////////////////////////////////////

class BreakpointDialog : public DxuiDialogWindow
{
public:
    //  The types offered, in the order the list shows them.
    enum class Type { Execute, Read, Write, ReadWrite, Value, Condition, Opcode, Brk, Interrupt, Disk, BasicLine, BasicVariable, BasicError,
                      UnwrittenRead, VideoLine, Beam };

    //  A row of the type list: a heading when type is empty.
    struct TypeRow
    {
        const wchar_t        * label = nullptr;
        std::optional<Type>    type;
    };

    static const std::vector<TypeRow> &  GetTypeRows ();
    static int                           GetRowOf    (Type type);

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

    //  The fields a type uses, which are the dialog's only visible fields
    //  besides the type.
    struct VisibleFields
    {
        bool  event     = false;
        bool  drive     = false;
        bool  mode      = false;
        bool  address   = false;
        bool  value     = false;
        bool  condition = false;
    };

    static VisibleFields  GetVisibleFields (Type type);

    //  The definition for a type and its fields, as BPEDIT takes it.
    static std::string  MakeDefinition (const Fields & fields);

    //  The fields an existing breakpoint fills in.
    static Fields  GetFields (const BreakpointInfo & info);

    //  The type an existing breakpoint is.
    static Type  GetType (const BreakpointInfo & info);

    //  Runs the dialog modally over `owner`, on `type` when given and on
    //  the breakpoint's own type otherwise; the definition when OK.
    static std::optional<std::string>  RunModal (HWND owner, const IDxuiTheme * theme, const BreakpointInfo & info, std::optional<Type> type = std::nullopt);

    //  A new breakpoint's dialog: Execute at the PC, every other type's
    //  fields empty.
    static std::optional<std::string>  RunNew   (HWND owner, const IDxuiTheme * theme, Word pc);

protected:
    void  OnCreate() override;

private:
    void  ShowFieldsForType ();
    Type  GetSelectedType   () const;

    static Fields  GetDiskFields (const BreakpointInfo & info, Fields fields);

    const IDxuiTheme            * m_theme          = nullptr;
    BreakpointInfo                m_info;
    std::optional<Type>           m_presetType;
    std::optional<std::wstring>   m_newAddress;
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
    DxuiInfoTip                   m_conditionTip;
    BreakpointDialogPanel       * m_body           = nullptr;
};
