#include "Pch.h"

#include "Ui/DiskInspector/FindBar.h"
#include "Core/UnicodeSymbols.h"





static constexpr int      s_kMaxPatternChars = 96;
static constexpr int      s_kPadDip          = 8;
static constexpr int      s_kInsetDip        = 4;
static constexpr int      s_kGapDip          = 8;
static constexpr int      s_kWidthDip        = 500;
static constexpr int      s_kRowDip          = 34;
static constexpr int      s_kEdgeDip         = 8;
static constexpr float    s_kRadiusDip       = 8.0f;
static constexpr float    s_kShadowDip       = 3.0f;
static constexpr uint32_t s_kShadowArgb      = 0x50000000u;
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
    m_input->SetPlaceholder (L"Find");

    m_input->SetOnChange      ([this] (const std::wstring & text) { SetError (GetPatternError (GetQuery().kind, text)); });
    m_kind->SetSelect         ([this] (int) { m_anyBit->SetEnabled (GetQuery().kind == SearchKind::Nibbles); ShowHint(); SetError (GetPatternError (GetQuery().kind, m_input->GetText())); Submit (0); });
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
    bool  isChanged = m_isError != !message.empty();



    m_isError = !message.empty();
    m_error->SetText (message);

    if (isChanged && m_scaler.GetDpi() != 0)
    {
        Layout (m_areaPx, m_scaler);
    }
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
        case SearchKind::SectorText: hint = L"Text in sector data, such as HELLO, in either case and with or without the high bit set. ? matches any character, and \\? a question mark."; break;
        default:                                                                                                                                         break;
    }

    return hint;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindBar::GetBadPatternMessage
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FindBar::GetBadPatternMessage (SearchKind kind)
{
    return (kind == SearchKind::SectorText) ? L"Enter the text to find, in plain ASCII." : L"Enter pairs of hex digits, such as D5 AA 96.";
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindBar::GetPatternError
//
//  What is wrong with a pattern now; nothing for one that can be read,
//  or one that can be once the pair being typed has its second digit.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FindBar::GetPatternError (SearchKind kind, const std::wstring & text)
{
    vector<SearchItem>  items;
    bool                isOk = text.empty() || InspectorSearch::ParsePattern (text, kind, items) || InspectorSearch::ParsePattern (text + L"0", kind, items);



    return isOk ? std::wstring() : GetBadPatternMessage (kind);
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
//  In the top right corner of the area, over what is there: the pattern, the
//  count, the steps, List and the close button; under them the kind, its info
//  tip and the two options; and under those any error.
//
////////////////////////////////////////////////////////////////////////////////

void FindBar::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    int  inset  = scaler.ToPx (s_kInsetDip);
    int  gap    = scaler.ToPx (s_kGapDip);
    int  edge   = scaler.ToPx (s_kEdgeDip);
    int  pad    = scaler.ToPx (s_kPadDip);
    int  rowH   = scaler.ToPx (s_kRowDip);
    int  step   = scaler.ToPx (s_kStepDip);
    int  right  = boundsPx.right - edge;
    int  left   = std::max (static_cast<int> (boundsPx.left) + edge, right - scaler.ToPx (s_kWidthDip));
    int  top    = boundsPx.top + edge;
    int  x      = left + pad;
    int  r      = right - pad;
    int  y      = top + inset;



    m_areaPx = boundsPx;
    m_scaler = scaler;

    SetBounds ({ left, top, right, top + 2 * inset + rowH * (m_isError ? 3 : 2) });

    m_closeButton->Layout ({ r - step, y + inset, r, y + rowH - inset }, scaler);
    r -= step + gap;
    m_listButton->Layout ({ r - scaler.ToPx (s_kListDip), y + inset, r, y + rowH - inset }, scaler);
    r -= scaler.ToPx (s_kListDip) + gap;
    m_next->Layout ({ r - step, y + inset, r, y + rowH - inset }, scaler);
    r -= step;
    m_prev->Layout ({ r - step, y + inset, r, y + rowH - inset }, scaler);
    r -= step + gap;
    m_count->Layout ({ r - scaler.ToPx (s_kCountDip), y + inset, r, y + rowH - inset }, scaler);
    r -= scaler.ToPx (s_kCountDip) + gap;
    m_input->Layout ({ x, y + inset, std::max (x, r), y + rowH - inset }, scaler);
    y += rowH;

    m_kind->Layout ({ x, y + inset, x + scaler.ToPx (s_kKindDip), y + rowH - inset }, scaler);
    x    += scaler.ToPx (s_kKindDip);
    m_hintTip->Layout ({ x, y + inset, x + scaler.ToPx (s_kTipDip), y + rowH - inset }, scaler);
    x    += scaler.ToPx (s_kTipDip) + gap;
    m_wholeDisk->Layout ({ x, y + inset, x + scaler.ToPx (s_kWholeDiskDip), y + rowH - inset }, scaler);
    x    += scaler.ToPx (s_kWholeDiskDip);
    m_anyBit->Layout ({ x, y + inset, x + scaler.ToPx (s_kAnyBitDip), y + rowH - inset }, scaler);
    y += rowH;

    m_error->SetVisible (m_isError);
    m_error->Layout ({ left + pad, y + inset, right - pad, y + rowH - inset }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindBar::Paint
//
//  A shadow and the bar's surface, a step above the window's, then its controls.
//
////////////////////////////////////////////////////////////////////////////////

void FindBar::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float  x      = static_cast<float> (m_boundsDip.left);
    float  y      = static_cast<float> (m_boundsDip.top);
    float  w      = static_cast<float> (m_boundsDip.right - m_boundsDip.left);
    float  h      = static_cast<float> (m_boundsDip.bottom - m_boundsDip.top);
    float  radius = m_scaler.ToPxf (s_kRadiusDip);
    float  shadow = m_scaler.ToPxf (s_kShadowDip);



    painter.FillRoundedRect    (x + shadow, y + shadow, w, h, radius, s_kShadowArgb);
    painter.FillRoundedRect    (x, y, w, h, radius, theme.BackgroundElevated());
    painter.OutlineRoundedRect (x, y, w, h, radius, m_scaler.ToPxf (1.0f), theme.Border());

    DxuiPanel::Paint (painter, text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindBar::OnMouse
//
//  A press on the pattern focuses it. An event over the bar isn't passed to what
//  is under it.
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

    return DxuiPanel::OnMouse (ev) || (isInside && ev.kind != DxuiMouseEventKind::Leave);
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
