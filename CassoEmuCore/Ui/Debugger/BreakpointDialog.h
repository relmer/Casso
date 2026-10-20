#pragma once

#include "Debugger/Reply.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialogPanel
//
//  A label beside each field, one field a row or two side by side, and an
//  info tip just after a row's label text where it has one; a hidden
//  field's row is left out, so the rows of the visible ones are together.
//  An error row spans the width and is as tall as its wrapped message.
//
////////////////////////////////////////////////////////////////////////////////

class BreakpointDialogPanel : public DxuiPanel
{
public:
    void  Add            (DxuiLabel & label, IDxuiControl & field, DxuiInfoTip * tip = nullptr, IDxuiControl * second = nullptr);
    void  AddError       (DxuiFieldError & error);
    void  SetTextMeasure (IDxuiTextRenderer * text, const IDxuiTheme * theme) { m_text = text; m_theme = theme; }
    void  Layout         (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;

    //  The height of the visible rows at a width, both in pixels.
    int   GetHeightPx    (const DxuiDpiScaler & scaler, int widthPx) const;

    static constexpr int  kRowDip    = 30;
    static constexpr int  kGapDip    = 8;
    static constexpr int  kLabelDip  = 130;
    static constexpr int  kTipDip    = 22;
    static constexpr int  kTipGapDip = 4;

private:
    struct Row
    {
        DxuiLabel       * label  = nullptr;
        IDxuiControl    * field  = nullptr;
        DxuiInfoTip     * tip    = nullptr;
        IDxuiControl    * second = nullptr;
        DxuiFieldError  * error  = nullptr;
    };

    int  GetErrorHeightPx (const DxuiFieldError & error, const DxuiDpiScaler & scaler, int widthPx) const;
    int  GetTextWidthPx   (const DxuiLabel & label, const DxuiDpiScaler & scaler) const;

    std::vector<Row>      m_rows;
    IDxuiTextRenderer   * m_text  = nullptr;
    const IDxuiTheme    * m_theme = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDialog
//
//  Edits a breakpoint's type and the fields that type takes, and returns
//  the definition BPEDIT reads: BP, BPMR, BPMW, BPM, BPMV or BRKOP with its
//  arguments, a data breakpoint's BEFORE and any IF condition, BPR IF and a
//  condition, BRK ON or BRKINT ON, BPDISK with its drive, event, arguments
//  and condition, or BPBASIC, BPBASICVAR or BRKBASIC ON. The dialog makes
//  only the type it opens on: the New drop-down's choice, or an edited
//  breakpoint's own. A memory address's Read and Write checks choose BPMR,
//  BPMW or BPM.
//
////////////////////////////////////////////////////////////////////////////////

class BreakpointDialog : public DxuiDialogWindow
{
public:
    //  The types offered, in the order the list shows them.
    enum class Type { Execute, Read, Write, ReadWrite, Value, Condition, Opcode, Brk, Interrupt, Disk, BasicLine, BasicVariable, BasicError,
                      UnwrittenRead, VideoLine, Beam };

    //  A row of the New drop-down: a heading when type is empty.
    struct TypeRow
    {
        const wchar_t        * label = nullptr;
        std::optional<Type>    type;
    };

    static const std::vector<TypeRow> &  GetTypeRows ();
    static int                           GetRowOf    (Type type);

    //  The dialog's title for a type: "New memory breakpoint: Address", or
    //  "Memory breakpoint #3: Address" for breakpoint 3.
    static std::wstring                  GetTitle    (Type type, std::optional<int> id);

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
        bool  access    = false;
    };

    static VisibleFields  GetVisibleFields (Type type);

    //  A disk breakpoint's fields: a track, or for HEAD and WRITE a range
    //  such as 10:12; a sector; ADDRESS's volume, in decimal, or BLOCK's and
    //  DRIVER's block; the event's drop-down; its checks; and the prologue
    //  marks and the 13-sector format, as BPDISK writes them after ADDR,
    //  DATA and SECTORS.
    struct DiskFields
    {
        std::string  track;
        std::string  sector;
        std::string  number;
        int          choice        = 0;
        bool         isPassed      = false;
        bool         isBadChecksum = false;
        bool         isWrongTrack  = false;
        bool         isBump        = false;
        bool         isEvery       = false;
        std::string  addressMarks;
        std::string  dataMarks;
        bool         isThirteen    = false;
    };

    //  Which disk fields an event has, and their labels; a null label is a
    //  field the event does not have.
    struct DiskFieldSet
    {
        const wchar_t              * trackLabel     = nullptr;
        const wchar_t              * sectorLabel    = nullptr;
        const wchar_t              * numberLabel    = nullptr;
        const wchar_t              * choiceLabel    = nullptr;
        std::vector<std::wstring>    choices;
        bool                         isPassed       = false;
        bool                         isBadChecksum  = false;
        bool                         isWrongTrack   = false;
        bool                         isBump         = false;
        bool                         isEvery        = false;
        bool                         isAddressMarks = false;   // ADDR and SECTORS
        bool                         isDataMarks    = false;   // DATA
        bool                         isRange        = false;   // the track field takes a range
    };

    //  By the event's index into DiskBreakSpecText::GetEventWords. The
    //  arguments are what BPDISK takes after the event word.
    static DiskFieldSet    GetDiskFieldSet       (int event);
    static std::string  MakeDiskArguments  (int event, const DiskFields & fields);
    static DiskFields   ParseDiskArguments (int event, const std::string & arguments);

    //  The definition for a type and its fields, as BPEDIT takes it.
    static std::string  MakeDefinition (const Fields & fields);

    //  The fields an existing breakpoint fills in.
    static Fields  GetFields (const BreakpointInfo & info);

    //  The type an existing breakpoint is.
    static Type  GetType (const BreakpointInfo & info);

    //  Why a definition cannot be set, or nothing when it can.
    using Validator = std::function<std::string (const std::string & definition)>;

    //  Runs the dialog modally over `owner`, on `type` when given and on
    //  the breakpoint's own type otherwise; the definition when OK. OK
    //  closes the dialog only once the validator, when given, accepts the
    //  definition; otherwise its reason is drawn under the fields.
    static std::optional<std::string>  RunModal (HWND owner, const IDxuiTheme * theme, const BreakpointInfo & info, std::optional<Type> type = std::nullopt,
                                                 Validator validator = {});

    //  A new breakpoint's dialog for the type chosen: an execution address
    //  starts at the PC, and every other type's fields start empty.
    static std::optional<std::string>  RunNew   (HWND owner, const IDxuiTheme * theme, Type type, Word pc, Validator validator = {});

    //  The condition's info tip text.
    static const wchar_t *  GetConditionTip ();

    //  The disk event row's info tip text: what each event stops on.
    static const wchar_t *  GetDiskEventTip ();

protected:
    void  OnCreate              () override;
    void  OnDialogTick          () override;
    bool  TryGetMaxClientSizePx (SIZE & outSizePx) const override;

private:
    void        ShowFieldsForType ();
    void        ShowDiskFields    (bool isDisk);
    void        Resize            ();
    void        Confirm           ();
    std::string MakeEditedDefinition () const;
    void        CheckFields       ();
    Type        GetSelectedType   () const;
    DiskFields  ReadDiskFields    () const;

    static std::string  ToDecimal (const std::string & hex);
    static std::string  ToHex     (const std::string & decimal);

    static Fields  GetDiskFields (const BreakpointInfo & info, Fields fields);

    const IDxuiTheme             * m_theme              = nullptr;
    BreakpointInfo                 m_info;
    std::optional<Type>            m_presetType;
    std::optional<std::wstring>    m_newAddress;
    std::optional<std::string>     m_definition;
    Validator                      m_validator;
    bool                           m_isCheckDue         = false;   // the focus moved off a field
    DxuiFieldError                 m_error;
    DxuiLabel                      m_accessLabel;
    DxuiLabel                      m_modeLabel;
    DxuiLabel                      m_addressLabel;
    DxuiLabel                      m_valueLabel;
    DxuiLabel                      m_conditionLabel;
    DxuiLabel                      m_eventLabel;
    DxuiLabel                      m_driveLabel;
    DxuiCheckbox                   m_accessRead;
    DxuiCheckbox                   m_accessWrite;
    Type                           m_type               = Type::Execute;
    DxuiComboBox                   m_mode;
    DxuiComboBox                   m_event;
    DxuiComboBox                   m_drive;
    DxuiTextInput                  m_address;
    DxuiTextInput                  m_value;
    DxuiTextInput                  m_condition;
    DxuiInfoTip                    m_conditionTip;
    DxuiLabel                      m_diskTrackLabel;
    DxuiLabel                      m_diskSectorLabel;
    DxuiLabel                      m_diskNumberLabel;
    DxuiLabel                      m_diskChoiceLabel;
    DxuiLabel                      m_diskAddressMarksLabel;
    DxuiLabel                      m_diskDataMarksLabel;
    DxuiLabel                      m_diskCheckLabels[6];
    DxuiInfoTip                    m_eventTip;
    DxuiInfoTip                    m_choiceTip;
    DxuiTextInput                  m_diskTrack;
    DxuiTextInput                  m_diskSector;
    DxuiTextInput                  m_diskNumber;
    DxuiComboBox                   m_diskChoice;
    DxuiCheckbox                   m_diskPassed;
    DxuiCheckbox                   m_diskBadChecksum;
    DxuiCheckbox                   m_diskWrongTrack;
    DxuiCheckbox                   m_diskBump;
    DxuiCheckbox                   m_diskEvery;
    DxuiCheckbox                   m_diskThirteen;
    DxuiTextInput                  m_diskAddressMarks;
    DxuiTextInput                  m_diskDataMarks;
    int                            m_diskFieldSetEvent  = -1;
    BreakpointDialogPanel        * m_body               = nullptr;
};
