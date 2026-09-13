#pragma once

#include "Pch.h"
#include "DxuiTextInput.h"
#include "DxuiToolbar.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarSearchBox
//
//  File Explorer's search box as a toolbar entry: a rounded field with a
//  magnifying glass at its right end, and a hint in italics while it is empty.
//
//  THE TOOLBAR LAYS IT OUT AND PAINTS IT; THE HOST ROUTES ITS KEYS. A press
//  inside it arrives through the toolbar's custom entry hooks and asks the
//  host for keyboard focus, and the host then passes key and character events
//  to OnKey until focus moves elsewhere.
//
//  Unfocused, the field is a subtle fill a shade off the strip. Focused, it
//  takes the content background and an accent line along its bottom edge.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiToolbarSearchBox : public IDxuiToolbarCustomEntry
{
public:
    using TextFn   = std::function<void (const std::wstring & text)>;
    using ActionFn = std::function<void ()>;

    DxuiToolbarSearchBox();
    ~DxuiToolbarSearchBox() override = default;

    void  SetHint         (const std::wstring & hint)    { m_input.SetPlaceholder (hint); }
    void  SetHwnd         (HWND hwnd)                    { m_input.SetHwnd (hwnd); }
    void  SetTextRenderer (IDxuiTextRenderer * renderer) { m_input.SetTextRenderer (renderer); }

    //  Told on every edit, on Enter and on Escape.
    void  SetOnChange (TextFn fn)   { m_onChange = std::move (fn); }
    void  SetOnSubmit (ActionFn fn) { m_onSubmit = std::move (fn); }
    void  SetOnCancel (ActionFn fn) { m_onCancel = std::move (fn); }

    //  Told when a press in the field asks for keyboard focus.
    void  SetOnFocusRequest (ActionFn fn) { m_onFocusRequest = std::move (fn); }

    const std::wstring &  GetText () const                    { return m_input.GetText(); }
    void                  SetText (const std::wstring & text) { m_input.SetText (text); }

    bool  IsFocused  () const { return m_focused; }
    void  SetFocused (bool focused);

    //  Key and character events while the field has focus. Tab, and keys the
    //  field has no use for, are left for the host.
    bool  OnKey (const DxuiKeyEvent & ev);

    int              GetWidthPx    (bool labeled, const DxuiDpiScaler & scaler, IDxuiTextRenderer * text) const override;
    void             Layout        (const RECT & rc, bool labeled, const DxuiDpiScaler & scaler) override;
    void             Paint         (IDxuiPainter      & painter,
                                    IDxuiTextRenderer & text,
                                    const IDxuiTheme  & theme,
                                    bool                hovered,
                                    bool                pressed,
                                    bool                labeled) override;
    const wchar_t *  GetTooltipAt  (int x, int y, RECT & anchor) const override;
    bool             OnClick       (int x, int y) override;
    bool             OnMouseMove   (int x, int y) override;
    void             OnMouseLeave  () override;
    bool             OnLButtonDown (int x, int y) override;

    static constexpr int    kWidthDip = 220;
    static constexpr float  kFontDip  = 14.0f;

private:
    bool  Contains (int x, int y) const;
    void  EditThenNotify (const std::function<void ()> & edit);

    static constexpr int  s_kInsetDip     = 4;
    static constexpr int  s_kGlyphSlotDip = 30;

    DxuiTextInput  m_input;
    DxuiDpiScaler  m_scaler;
    RECT           m_rc             = {};
    bool           m_focused        = false;
    bool           m_hover          = false;
    TextFn         m_onChange;
    ActionFn       m_onSubmit;
    ActionFn       m_onCancel;
    ActionFn       m_onFocusRequest;
};
