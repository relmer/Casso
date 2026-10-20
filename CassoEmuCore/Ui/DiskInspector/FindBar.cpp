#include "Pch.h"

#include "Ui/DiskInspector/FindBar.h"
#include "Core/UnicodeSymbols.h"





static constexpr int      s_kMaxPatternChars = 96;
static constexpr int      s_kPadDip          = 8;
static constexpr int      s_kInsetDip        = 4;
static constexpr int      s_kGapDip          = 8;
static constexpr int      s_kLabelDip        = 40;
static constexpr int      s_kInputDip        = 240;
static constexpr int      s_kKindDip         = 120;
static constexpr int      s_kTipDip          = 28;
static constexpr int      s_kWholeDiskDip    = 112;
static constexpr int      s_kAnyBitDip       = 136;
static constexpr int      s_kCountDip        = 96;
static constexpr int      s_kStepDip         = 30;
static constexpr int      s_kListDip         = 56;





////////////////////////////////////////////////////////////////////////////////
//
//  FindBar::FindBar
//
//  A change of kind or option searches again when there is a pattern, so
//  the hits always match the controls.
//
////////////////////////////////////////////////////////////////////////////////

FindBar::FindBar()
{
    m_label       = CreateChild<DxuiLabel>    (L"Find", DxuiTextRole::Body, DxuiTextHAlign::Left);
    m_input       = CreateChild<DxuiTextInput>();
    m_kind        = CreateChild<DxuiComboBox>();
    m_hintTip     = CreateChild<DxuiInfoTip>();
    m_wholeDisk   = CreateChild<DxuiCheckbox> (L"Whole disk", true);
    m_anyBit      = CreateChild<DxuiCheckbox> (L"Any bit offset");
    m_count       = CreateChild<DxuiLabel>    (L"", DxuiTextRole::Body, DxuiTextHAlign::Right);
    m_prev        = CreateChild<DxuiButton>   (s_kpszTriangleUp);
    m_next        = CreateChild<DxuiButton>   (s_kpszTriangleDown);
    m_listButton  = CreateChild<DxuiButton>   (L"List");
    m_closeButton = CreateChild<DxuiButton>   (s_kpszMultiplyX);
    m_error       = CreateChild<DxuiLabel>    (L"", DxuiTextRole::Error, DxuiTextHAlign::Left);

    m_kind->SetItems    ({ L"Nibbles", L"Sector hex", L"Sector text" });
    m_kind->SetSelected (0);
    m_input->SetMaxLength (s_kMaxPatternChars);

    m_input->SetOnChange      ([this] (const std::wstring &) { SetError (L""); });
    m_kind->SetSelect         ([this] (int) { m_anyBit->SetEnabled (GetQuery().kind == SearchKind::Nibbles); ShowHint(); Submit (0); });
    m_wholeDisk->SetOnChange  ([this] (bool) { Submit (0); });
    m_anyBit->SetOnChange     ([this] (bool) { Submit (0); });
    m_prev->SetOnClick        ([this] () { Submit (-1); });
    m_next->SetOnClick        ([this] () { Submit (1); });
    m_listButton->SetOnClick  ([this] () { if (m_list) { m_list(); } });
    m_closeButton->SetOnClick ([this] () { if (m_close) { m_close(); } });

    SetResult (-1, 0);
    ShowHint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindBar::Init
//
////////////////////////////////////////////////////////////////////////////////

void FindBar::Init (HWND hwnd, IDxuiTextRenderer * text, DxuiHwndSource * popupHost, const IDxuiTheme * theme)
{
    m_input->SetHwnd         (hwnd);
    m_input->SetTextRenderer (text);
    m_input->SetTheme        (theme);
    m_kind->SetPopupHost     (popupHost);
    m_hintTip->SetPopupHost  (popupHost);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindBar::SetQuery
//
////////////////////////////////////////////////////////////////////////////////

void FindBar::SetQuery (const SearchQuery & query)
{
    m_input->SetText       (query.text);
    m_kind->SetSelected    (static_cast<int> (query.kind));
    m_wholeDisk->SetChecked (query.isWholeDisk);
    m_anyBit->SetChecked   (query.isAnyBitOffset);
    m_anyBit->SetEnabled   (query.kind == SearchKind::Nibbles);
    ShowHint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindBar::GetQuery
//
////////////////////////////////////////////////////////////////////////////////

SearchQuery FindBar::GetQuery() const
{
    SearchQuery  query;



    query.kind           = static_cast<SearchKind> (std::clamp (m_kind->GetSelectedIndex(), 0, static_cast<int> (SearchKind::SectorText)));
    query.text           = m_input->GetText();
    query.isWholeDisk    = m_wholeDisk->IsChecked();
    query.isAnyBitOffset = m_anyBit->IsChecked() && query.kind == SearchKind::Nibbles;

    return query;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindBar::SetResult
//
////////////////////////////////////////////////////////////////////////////////

void FindBar::SetResult (int index, int count)
{
    std::wstring  text;



    if (count > 0)
    {
        text = (index >= 0) ? std::format (L"{} of {}", index + 1, count) : InspectorSearch::FormatHitCount (static_cast<size_t> (count));
    }
    else if (m_hasSearched && !m_searched.text.empty() && !m_isError)
    {
        text = L"No matches";
    }

    m_count->SetText    (text);
    m_prev->SetEnabled  (count > 0);
    m_next->SetEnabled  (count > 0);
    m_listButton->SetEnabled (count > 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindBar::SetError
//
////////////////////////////////////////////////////////////////////////////////

void FindBar::SetError (const std::wstring & message)
{
    m_isError = !message.empty();
    m_error->SetText (message);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindBar::SetInputFocused
//
////////////////////////////////////////////////////////////////////////////////

void FindBar::SetInputFocused (bool isFocused)
{
    m_input->SetFocused (isFocused);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindBar::IsInputFocused
//
////////////////////////////////////////////////////////////////////////////////

bool FindBar::IsInputFocused() const
{
    return m_input->IsFocused();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindBar::GetHint
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FindBar::GetHint (SearchKind kind)
{
    std::wstring  hint;



    switch (kind)
    {
        case SearchKind::Nibbles:    hint = L"Nibbles in hex pairs, such as D5 AA 96. ? matches any digit, and + after a nibble for extra zero cells."; break;
        case SearchKind::SectorHex:  hint = L"Bytes of sector data in hex pairs, such as A9 00 8D. ? matches any digit.";                             break;
        case SearchKind::SectorText: hint = L"Text in sector data, such as HELLO, in either case and with or without the high bit set.";               break;
        default:                                                                                                                                         break;
    }

    return hint;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindBar::IsSame
//
////////////////////////////////////////////////////////////////////////////////

bool FindBar::IsSame (const SearchQuery & a, const SearchQuery & b)
{
    return a.kind == b.kind && a.text == b.text && a.isWholeDisk == b.isWholeDisk && a.isAnyBitOffset == b.isAnyBitOffset;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindBar::Submit
//
//  A query that differs from the searched one, or a step of 0, searches;
//  otherwise the step moves to the previous or next hit. An empty pattern
//  with no search behind it does nothing.
//
////////////////////////////////////////////////////////////////////////////////

void FindBar::Submit (int step)
{
    SearchQuery  query     = GetQuery();
    bool         isChanged = !m_hasSearched || !IsSame (query, m_searched);



    if ((isChanged || step == 0) && (!query.text.empty() || m_hasSearched) && m_search)
    {
        m_search (query);
    }
    else if (!isChanged && step != 0 && m_step)
    {
        m_step (step);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindBar::ShowHint
//
////////////////////////////////////////////////////////////////////////////////

void FindBar::ShowHint()
{
    m_hintTip->SetText (GetHint (GetQuery().kind));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindBar::Layout
//
//  One row: the label, the pattern, the kind, its info tip, the two options
//  and any error from the left, and the close button, List, the steps and
//  the count from the right.
//
////////////////////////////////////////////////////////////////////////////////

void FindBar::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    int  inset  = scaler.ToPx (s_kInsetDip);
    int  gap    = scaler.ToPx (s_kGapDip);
    int  top    = boundsPx.top + inset;
    int  bottom = boundsPx.bottom - inset;
    int  x      = boundsPx.left + scaler.ToPx (s_kPadDip);
    int  r      = boundsPx.right - scaler.ToPx (s_kPadDip);
    int  step   = scaler.ToPx (s_kStepDip);



    SetBounds (boundsPx);

    m_label->Layout ({ x, top, x + scaler.ToPx (s_kLabelDip), bottom }, scaler);
    x    += scaler.ToPx (s_kLabelDip);
    m_input->Layout ({ x, top, x + scaler.ToPx (s_kInputDip), bottom }, scaler);
    x    += scaler.ToPx (s_kInputDip) + gap;
    m_kind->Layout ({ x, top, x + scaler.ToPx (s_kKindDip), bottom }, scaler);
    x    += scaler.ToPx (s_kKindDip);
    m_hintTip->Layout ({ x, top, x + scaler.ToPx (s_kTipDip), bottom }, scaler);
    x    += scaler.ToPx (s_kTipDip) + gap;
    m_wholeDisk->Layout ({ x, top, x + scaler.ToPx (s_kWholeDiskDip), bottom }, scaler);
    x    += scaler.ToPx (s_kWholeDiskDip);
    m_anyBit->Layout ({ x, top, x + scaler.ToPx (s_kAnyBitDip), bottom }, scaler);
    x    += scaler.ToPx (s_kAnyBitDip) + gap;

    m_closeButton->Layout ({ r - step, top, r, bottom }, scaler);
    r -= step + gap;
    m_listButton->Layout ({ r - scaler.ToPx (s_kListDip), top, r, bottom }, scaler);
    r -= scaler.ToPx (s_kListDip) + gap;
    m_next->Layout ({ r - step, top, r, bottom }, scaler);
    r -= step;
    m_prev->Layout ({ r - step, top, r, bottom }, scaler);
    r -= step + gap;
    m_count->Layout ({ r - scaler.ToPx (s_kCountDip), top, r, bottom }, scaler);
    r -= scaler.ToPx (s_kCountDip) + gap;
    m_error->Layout ({ x, top, std::max (x, r), bottom }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindBar::Paint
//
//  The bar's surface, a step above the window's, with a divider along its
//  bottom, then its controls.
//
////////////////////////////////////////////////////////////////////////////////

void FindBar::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    painter.FillRect (static_cast<float> (m_boundsDip.left), static_cast<float> (m_boundsDip.top), static_cast<float> (m_boundsDip.right - m_boundsDip.left),
                      static_cast<float> (m_boundsDip.bottom - m_boundsDip.top), theme.BackgroundElevated());
    painter.FillRect (static_cast<float> (m_boundsDip.left), static_cast<float> (m_boundsDip.bottom) - 1.0f, static_cast<float> (m_boundsDip.right - m_boundsDip.left), 1.0f,
                      theme.Divider());

    DxuiPanel::Paint (painter, text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindBar::OnMouse
//
//  A press on the pattern focuses it; a press anywhere else on the bar
//  is consumed.
//
////////////////////////////////////////////////////////////////////////////////

bool FindBar::OnMouse (const DxuiMouseEvent & ev)
{
    RECT  input    = m_input->GetBounds();
    bool  isInside = PtInRect (&m_boundsDip, ev.positionDip) != FALSE;



    if (ev.kind == DxuiMouseEventKind::Down && PtInRect (&input, ev.positionDip))
    {
        m_input->SetFocused (true);
    }

    return DxuiPanel::OnMouse (ev) || (isInside && ev.kind == DxuiMouseEventKind::Down);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindBar::OnKey
//
//  Enter and Shift+Enter search or step, Escape closes the bar, and other
//  keys go to the pattern while it has the focus.
//
////////////////////////////////////////////////////////////////////////////////

bool FindBar::OnKey (const DxuiKeyEvent & ev)
{
    bool  isHandled = true;



    if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_RETURN)
    {
        Submit (ev.shift ? -1 : 1);
    }
    else if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_ESCAPE && m_close)
    {
        m_close();
    }
    else if (ev.kind == DxuiKeyEventKind::Char && (ev.vk == L'\r' || ev.vk == 0x1B))
    {
        //  The character an Enter or Escape also sends adds nothing.
    }
    else
    {
        isHandled = m_input->IsFocused() && m_input->OnKey (ev);
    }

    return isHandled;
}
