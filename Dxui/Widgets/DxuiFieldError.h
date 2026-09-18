#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Widgets/DxuiButton.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFieldError
//
//  The message under a field whose value cannot be used: a round error mark
//  level with the message's first line, and the message wrapping onto as many
//  lines as it needs. A host lays out the rows below it by its height, so a
//  long message pushes them down instead of running over them. With no message
//  it takes no height and draws nothing.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiFieldError : public IDxuiControl
{
public:
    void                  SetMessage (const std::wstring & message) { m_message = message; }
    const std::wstring &  GetMessage () const                        { return m_message; }
    bool                  HasError   () const                        { return !m_message.empty(); }

    //  The height the message needs at a width, both in pixels: nothing without
    //  a message, otherwise the wrapped text or the mark, whichever is taller.
    int  GetHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, const DxuiDpiScaler & scaler, int widthPx) const;

    //  Black or white, whichever stands out more against the mark's fill, as
    //  Windows chooses the glyph on its own error marks.
    static uint32_t  GetMarkGlyphColor (uint32_t fillArgb);

    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    std::wstring        GetAccessibleName () const override { return m_message; }
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Label; }

    static constexpr int  kMarkDip = 14;   // the mark's diameter
    static constexpr int  kGapDip  = 6;    // between the mark and the message

private:
    static double  GetChannelLuminance (uint32_t channel);

    std::wstring   m_message;
    DxuiDpiScaler  m_scaler;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFieldValidator
//
//  Checks a form's fields together. Each field names a check, which returns
//  why its value cannot be used or nothing, and the error widget that shows
//  the answer. Revalidate runs every check, shows each message, and enables
//  the confirming button only when none failed, so a form is never confirmed
//  with a value it would refuse.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiFieldValidator
{
public:
    using CheckFn = std::function<std::wstring()>;

    void  AddField         (CheckFn check, DxuiFieldError * error) { m_fields.push_back ({ std::move (check), error }); }
    void  SetConfirmButton (DxuiButton * button)                   { m_confirm = button; }

    //  True when every field passed.
    bool  Revalidate ();

private:
    struct Field
    {
        CheckFn           check;
        DxuiFieldError  * error = nullptr;
    };

    std::vector<Field>  m_fields;
    DxuiButton        * m_confirm = nullptr;
};
