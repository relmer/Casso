#pragma once

#include "Ui/Debugger/DebuggerActions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryFileDialogPanel
//
//  A label beside each field, a row each, and the error line under them.
//
////////////////////////////////////////////////////////////////////////////////

class MemoryFileDialogPanel : public DxuiPanel
{
public:
    void  Add      (DxuiLabel & label, IDxuiControl & field);
    void  SetError (DxuiLabel & error);
    void  Layout   (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;

    static constexpr int  kRowDip   = 30;
    static constexpr int  kGapDip   = 8;
    static constexpr int  kLabelDip = 110;

private:
    std::vector<std::pair<DxuiLabel *, IDxuiControl *>>  m_rows;
    DxuiLabel                                          * m_error = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryFileDialog
//
//  File > Load file into memory... and Save memory to file...: for a file
//  already picked, a load's format, address and length, or a save's address
//  and length. OK takes them only once they make a BLOAD or BSAVE that can
//  run; until then the reason is under the fields.
//
////////////////////////////////////////////////////////////////////////////////

class MemoryFileDialog : public DxuiDialogWindow
{
public:
    //  The formats in the drop-down's order; with the first, the file's
    //  contents indicate the format.
    enum class Format { Detect, Raw, Dos33, IntelHex, SRecord, AppleSingle };

    struct Fields
    {
        Format        format = Format::Detect;
        std::wstring  address;
        std::wstring  length;
    };

    //  The command a load or a save of `path` with these fields runs; false,
    //  with the reason, for fields that cannot make one.
    static bool  TryMakeLoad (const std::string & path, const Fields & fields, CommandMode mode, DebuggerAction & action, std::wstring & error);
    static bool  TryMakeSave (const std::string & path, const Fields & fields, CommandMode mode, DebuggerAction & action, std::wstring & error);

    //  One to four hex digits for an address, five for a length, with or
    //  without a dollar sign.
    static bool  TryParseHex (const std::wstring & text, size_t maxDigits, uint32_t & value);

    //  Runs the dialog modally over `owner`; the fields when OK.
    static std::optional<Fields>  RunModal (HWND owner, const IDxuiTheme * theme, const std::wstring & path, bool isSave, const Fields & initial);

protected:
    void  OnCreate() override;

private:
    const IDxuiTheme       * m_theme  = nullptr;
    std::wstring             m_path;
    bool                     m_isSave = false;
    Fields                   m_fields;
    std::optional<Fields>    m_result;
    DxuiLabel                m_fileLabel;
    DxuiLabel                m_file;
    DxuiLabel                m_formatLabel;
    DxuiComboBox             m_format;
    DxuiLabel                m_addressLabel;
    DxuiTextInput            m_address;
    DxuiLabel                m_lengthLabel;
    DxuiTextInput            m_length;
    DxuiLabel                m_error;
    MemoryFileDialogPanel  * m_body   = nullptr;
};
