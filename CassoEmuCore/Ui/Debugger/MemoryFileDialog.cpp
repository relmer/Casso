#include "Pch.h"

#include "Ui/Debugger/MemoryFileDialog.h"
#include "Debugger/Source/SourcePathList.h"



//  The drop-down's labels and BLOAD's format words, in Format's order.
static constexpr const wchar_t *  s_kFormatLabels[] = { L"From the file", L"Raw bytes", L"DOS 3.3 binary", L"Intel HEX", L"S-record", L"AppleSingle" };
static constexpr const char *     s_kFormatWords[]  = { "",              "RAW",       "DOS",            "HEX",       "SREC",     "AS"          };





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryFileDialogPanel::Add
//
////////////////////////////////////////////////////////////////////////////////

void MemoryFileDialogPanel::Add (DxuiLabel & label, IDxuiControl & field)
{
    m_rows.push_back ({ &label, &field });
    Adopt (label);
    Adopt (field);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryFileDialogPanel::SetError
//
////////////////////////////////////////////////////////////////////////////////

void MemoryFileDialogPanel::SetError (DxuiLabel & error)
{
    m_error = &error;
    Adopt (error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryFileDialogPanel::Layout
//
////////////////////////////////////////////////////////////////////////////////

void MemoryFileDialogPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
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

    if (m_error != nullptr)
    {
        m_error->Layout (RECT { boundsDip.left, y, boundsDip.right, y + row }, scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryFileDialog::TryParseHex
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryFileDialog::TryParseHex (const std::wstring & text, size_t maxDigits, uint32_t & value)
{
    size_t        first  = text.find_first_not_of (L" \t");
    size_t        last   = text.find_last_not_of (L" \t");
    std::wstring  digits = (first == std::wstring::npos) ? std::wstring() : text.substr (first, last - first + 1);



    if (digits.starts_with (L'$'))
    {
        digits.erase (0, 1);
    }

    if (digits.empty() || digits.size() > maxDigits || !std::ranges::all_of (digits, [] (wchar_t ch) { return iswxdigit (ch) != 0; }))
    {
        return false;
    }

    value = (uint32_t) std::stoul (digits, nullptr, 16);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryFileDialog::TryMakeLoad
//
//  As BinaryImageReader reads each format: raw bytes have no address of
//  their own, so one is required; an Intel HEX or S-record file has its
//  addresses in it; the others take a typed address in place of their own.
//  A length is counted from an address.
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryFileDialog::TryMakeLoad (const std::string & path, const Fields & fields, CommandMode mode, DebuggerAction & action, std::wstring & error)
{
    constexpr size_t         kAddressDigits = 4;
    constexpr size_t         kLengthDigits  = 5;
    bool                     hasAddress     = fields.address.find_first_not_of (L" \t") != std::wstring::npos;
    bool                     hasLength      = fields.length.find_first_not_of (L" \t") != std::wstring::npos;
    bool                     isRecords      = fields.format == Format::IntelHex || fields.format == Format::SRecord;
    uint32_t                 address        = 0;
    uint32_t                 length         = 0;
    std::optional<Word>      at;
    std::optional<uint32_t>  count;



    if (hasAddress && !TryParseHex (fields.address, kAddressDigits, address))
    {
        error = L"An address is one to four hex digits, as $6000.";
        return false;
    }

    if (hasLength && !TryParseHex (fields.length, kLengthDigits, length))
    {
        error = L"A length is hex digits, as $2000.";
        return false;
    }

    if (fields.format == Format::Raw && !hasAddress)
    {
        error = L"Raw bytes have no address of their own. Type the address to load them at.";
        return false;
    }

    if (isRecords && (hasAddress || hasLength))
    {
        error = L"An Intel HEX or S-record file has its own addresses. Clear the address and length.";
        return false;
    }

    if (hasLength && !hasAddress)
    {
        error = L"A length runs from an address. Type the address too.";
        return false;
    }

    if (hasLength && (length == 0 || address + length > 0x10000))
    {
        error = L"The length must be more than zero and end within memory, at $FFFF.";
        return false;
    }

    at     = hasAddress ? std::optional<Word> ((Word) address) : std::nullopt;
    count  = hasLength  ? std::optional<uint32_t> (length)      : std::nullopt;
    action = DebuggerActions::GetLoadBinary (path, s_kFormatWords[(size_t) fields.format], at, count, mode);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryFileDialog::TryMakeSave
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryFileDialog::TryMakeSave (const std::string & path, const Fields & fields, CommandMode mode, DebuggerAction & action, std::wstring & error)
{
    constexpr size_t  kAddressDigits = 4;
    constexpr size_t  kLengthDigits  = 5;
    uint32_t          address        = 0;
    uint32_t          length         = 0;



    if (!TryParseHex (fields.address, kAddressDigits, address))
    {
        error = L"Type the address to save from, one to four hex digits, as $6000.";
        return false;
    }

    if (!TryParseHex (fields.length, kLengthDigits, length) || length == 0 || address + length > 0x10000)
    {
        error = L"Type the length to save, in hex: more than zero, ending within memory at $FFFF.";
        return false;
    }

    action = DebuggerActions::GetSaveBinary (path, (Word) address, length, mode);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryFileDialog::OnCreate
//
//  A save has no format: BSAVE writes raw bytes.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryFileDialog::OnCreate()
{
    DxuiButton  * ok = nullptr;



    for (DxuiLabel * label : { &m_fileLabel, &m_file, &m_formatLabel, &m_addressLabel, &m_lengthLabel, &m_error })
    {
        label->SetTextRole  (DxuiTextRole::Body);
        label->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    }

    m_error.SetTextRole (DxuiTextRole::Error);

    m_fileLabel.SetText    (L"File");
    m_file.SetText         (std::filesystem::path (m_path).filename().wstring());
    m_formatLabel.SetText  (L"Format");
    m_addressLabel.SetText (L"Address");
    m_lengthLabel.SetText  (L"Length");

    m_format.SetItems     (std::vector<std::wstring> (std::begin (s_kFormatLabels), std::end (s_kFormatLabels)));
    m_format.SetSelected  ((int) m_fields.format);
    m_format.SetPopupHost (GetPopupHost());

    for (DxuiTextInput * input : { &m_address, &m_length })
    {
        input->SetTheme        (m_theme);
        input->SetHwnd         (GetHwnd());
        input->SetTextRenderer (GetTextRenderer());
        input->SetMaxLength    (8);
    }

    m_address.SetText        (m_fields.address);
    m_address.SetPlaceholder (m_isSave ? L"$0400" : L"The file's own");
    m_length.SetText         (m_fields.length);
    m_length.SetPlaceholder  (m_isSave ? L"$0400" : L"The whole file");

    m_body = CreateDialogContent<MemoryFileDialogPanel>();
    m_body->Add (m_fileLabel, m_file);

    if (!m_isSave)
    {
        m_body->Add (m_formatLabel, m_format);
    }

    m_body->Add      (m_addressLabel, m_address);
    m_body->Add      (m_lengthLabel,  m_length);
    m_body->SetError (m_error);

    ok = AddDialogButton (L"OK", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    ok->SetOnClick ([this]()
    {
        Fields          edited;
        DebuggerAction  action;
        std::wstring    error;
        std::string     path = SourcePathList::WideToUtf8 (m_path);
        bool            isOk = false;



        edited.format  = m_isSave ? Format::Raw : (Format) m_format.GetSelectedIndex();
        edited.address = m_address.GetText();
        edited.length  = m_length.GetText();
        isOk           = m_isSave ? TryMakeSave (path, edited, CommandMode::AppleWin, action, error)
                                  : TryMakeLoad (path, edited, CommandMode::AppleWin, action, error);

        if (!isOk)
        {
            m_error.SetText (error);
            Invalidate();
            return;
        }

        m_result = edited;
        EndDialog (IDOK);
    });

    SetInitialFocus (&m_address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryFileDialog::RunModal
//
////////////////////////////////////////////////////////////////////////////////

std::optional<MemoryFileDialog::Fields> MemoryFileDialog::RunModal (HWND owner, const IDxuiTheme * theme, const std::wstring & path, bool isSave, const Fields & initial)
{
    HRESULT                   hr     = S_OK;
    MemoryFileDialog          dialog;
    DxuiWindow::CreateParams  params;



    dialog.m_theme  = theme;
    dialog.m_path   = path;
    dialog.m_isSave = isSave;
    dialog.m_fields = initial;

    params.title                    = isSave ? L"Save memory to file" : L"Load file into memory";
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 520, isSave ? 280L : 320L };
    params.minSizeDip               = params.initialSizeDip;
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

    return dialog.m_result;
}
