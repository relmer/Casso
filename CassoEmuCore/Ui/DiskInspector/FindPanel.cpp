#include "Pch.h"

#include "Ui/DiskInspector/FindPanel.h"





static constexpr int      s_kMaxPatternChars = 96;
static constexpr int      s_kFindId          = IDOK;
static constexpr int      s_kGoToId          = IDYES;
static constexpr LPCWSTR  s_kpszBadHex       = L"Enter pairs of hex digits, such as D5 AA 96. ? stands for any digit, and + after a nibble for extra zero cells.";
static constexpr LPCWSTR  s_kpszBadText      = L"Enter the text to find, in plain ASCII.";





////////////////////////////////////////////////////////////////////////////////
//
//  FindPanelBody
//
//  The kind of search, the pattern, the two options and the status line in
//  rows, and the hits filling the rest; laid out in physical pixels as the
//  other dialog bodies are.
//
////////////////////////////////////////////////////////////////////////////////

class FindPanelBody : public DxuiPanel
{
public:
    void  Init (DxuiRadioGroup & kind, DxuiLabel & label, DxuiTextInput & input, DxuiCheckbox & wholeDisk, DxuiCheckbox & anyBit, DxuiLabel & status,
                InspectorTableView & results)
    {
        m_kind      = &kind;
        m_label     = &label;
        m_input     = &input;
        m_wholeDisk = &wholeDisk;
        m_anyBit    = &anyBit;
        m_status    = &status;
        m_results   = &results;

        Adopt (kind);
        Adopt (label);
        Adopt (input);
        Adopt (wholeDisk);
        Adopt (anyBit);
        Adopt (status);
        Adopt (results);
    }

    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override
    {
        static constexpr LPCWSTR  kKinds[] = { L"Nibbles", L"Sector hex", L"Sector text" };

        int                           pad     = scaler.ToPx (kPadDip);
        int                           rowH    = scaler.ToPx (kRowDip);
        int                           labelW  = scaler.ToPx (kLabelDip);
        int                           optionW = scaler.ToPx (kOptionDip);
        int                           x       = boundsPx.left + pad;
        int                           y       = boundsPx.top  + pad;
        int                           i       = 0;
        int                           sel     = m_kind->GetSelected();
        std::vector<DxuiRadioOption>  options;



        SetBounds (boundsPx);

        for (LPCWSTR kind : kKinds)
        {
            options.push_back ({ RECT { x + i * optionW, y, x + (i + 1) * optionW, y + rowH - 4 }, kind });
            i++;
        }

        m_kind->SetOptions  (std::move (options));
        m_kind->SetSelected (sel);
        m_kind->Layout (RECT { x, y, x + 3 * optionW, y + rowH - 4 }, scaler);
        y += rowH;

        m_label->Layout (RECT { x, y, x + labelW, y + rowH - 4 }, scaler);
        m_input->Layout (RECT { x + labelW, y, boundsPx.right - pad, y + rowH - 4 }, scaler);
        y += rowH;

        m_wholeDisk->Layout (RECT { x, y, x + 2 * optionW, y + rowH - 4 }, scaler);
        m_anyBit->Layout    (RECT { x + 2 * optionW, y, boundsPx.right - pad, y + rowH - 4 }, scaler);
        y += rowH;

        m_status->Layout (RECT { x, y, boundsPx.right - pad, y + rowH }, scaler);
        y += rowH;

        m_results->Layout (RECT { x, y, boundsPx.right - pad, boundsPx.bottom - pad }, scaler);
    }

    bool  OnMouse (const DxuiMouseEvent & ev) override
    {
        return m_kind->OnMouse (ev) || m_input->OnMouse (ev) || m_wholeDisk->OnMouse (ev) || m_anyBit->OnMouse (ev) || m_results->OnMouse (ev);
    }

private:
    static constexpr int  kPadDip    = 12;
    static constexpr int  kRowDip    = 32;
    static constexpr int  kLabelDip  = 56;
    static constexpr int  kOptionDip = 130;

    DxuiRadioGroup *      m_kind      = nullptr;
    DxuiLabel *           m_label     = nullptr;
    DxuiTextInput *       m_input     = nullptr;
    DxuiCheckbox *        m_wholeDisk = nullptr;
    DxuiCheckbox *        m_anyBit    = nullptr;
    DxuiLabel *           m_status    = nullptr;
    InspectorTableView *  m_results   = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FindPanel::Configure
//
////////////////////////////////////////////////////////////////////////////////

void FindPanel::Configure (const IDxuiTheme * theme, InspectorViewContext & context, int quarterTrack, const SearchQuery & query, const vector<SearchHit> & hits)
{
    m_theme        = theme;
    m_context      = &context;
    m_quarterTrack = quarterTrack;
    m_query        = query;
    m_hits         = hits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindPanel::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void FindPanel::OnCreate()
{
    FindPanelBody *  body = nullptr;
    DxuiButton *     find = nullptr;
    DxuiButton *     goTo = nullptr;



    m_results = std::make_unique<InspectorTableView> (*m_context);
    m_results->SetColumns  ({ L"Place", L"State" });
    m_results->SetOnSelect ([this] (const TableRow & row) { m_highlighted = row.finding; });

    m_kind.SetDpi      (GetDpi());
    m_kind.SetSelected (static_cast<int> (m_query.kind));

    m_label.SetTextRole  (DxuiTextRole::Body);
    m_label.SetText      (L"Find");
    m_label.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    m_input.SetTheme        (m_theme);
    m_input.SetHwnd         (GetHwnd());
    m_input.SetMaxLength    (s_kMaxPatternChars);
    m_input.SetTextRenderer (GetTextRenderer());
    m_input.SetText         (m_query.text);

    m_wholeDisk.SetChecked (m_query.isWholeDisk);
    m_anyBit.SetChecked    (m_query.isAnyBitOffset);

    m_status.SetTextRole  (DxuiTextRole::Muted);
    m_status.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    body = CreateDialogContent<FindPanelBody>();
    body->Init (m_kind, m_label, m_input, m_wholeDisk, m_anyBit, m_status, *m_results);

    find = AddDialogButton (L"Find", s_kFindId);
    goTo = AddDialogButton (L"Go to", s_kGoToId);
    AddDialogButton (L"Close", IDCANCEL);

    find->SetOnClick ([this] () { OnFind(); });
    goTo->SetOnClick ([this] () { OnGoTo(); });

    ShowHits();
    SetInitialFocus (&m_input);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindPanel::OnFind
//
//  A pattern that cannot be read says how to write one and finds nothing.
//
////////////////////////////////////////////////////////////////////////////////

void FindPanel::OnFind()
{
    vector<SearchItem>  items;



    ReadQuery();

    if (!InspectorSearch::ParsePattern (m_query.text, m_query.kind, items))
    {
        m_status.SetTextRole (DxuiTextRole::Error);
        m_status.SetText     (m_query.kind == SearchKind::SectorText ? s_kpszBadText : s_kpszBadHex);
    }
    else if (m_context->analysis != nullptr)
    {
        m_hits        = InspectorSearch::Find (*m_context->analysis, m_quarterTrack, m_query);
        m_highlighted = m_hits.empty() ? -1 : 0;
        ShowHits();
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindPanel::OnGoTo
//
////////////////////////////////////////////////////////////////////////////////

void FindPanel::OnGoTo()
{
    if (m_highlighted >= 0 && m_highlighted < static_cast<int> (m_hits.size()))
    {
        ReadQuery();
        m_chosen = m_highlighted;
        EndDialog (IDOK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindPanel::ShowHits
//
////////////////////////////////////////////////////////////////////////////////

void FindPanel::ShowHits()
{
    vector<TableRow>  rows;
    size_t            i    = 0;



    for (i = 0; i < m_hits.size(); i++)
    {
        TableRow  row;

        row.cells        = { InspectorSearch::FormatHit (m_hits[i]),
                             (m_context->analysis != nullptr && InspectorSearch::IsOutOfDate (m_hits[i], *m_context->analysis)) ? L"Out of date" : L"" };
        row.quarterTrack = m_hits[i].quarterTrack;
        row.finding      = static_cast<int> (i);
        rows.push_back (std::move (row));
    }

    m_results->SetRows (std::move (rows));
    m_status.SetTextRole (DxuiTextRole::Muted);
    m_status.SetText (m_hits.empty() ? (m_query.text.empty() ? L"" : L"No matches") : InspectorSearch::FormatHitCount (m_hits.size()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindPanel::ReadQuery
//
////////////////////////////////////////////////////////////////////////////////

void FindPanel::ReadQuery()
{
    m_query.kind           = static_cast<SearchKind> (std::clamp (m_kind.GetSelected(), 0, 2));
    m_query.text           = m_input.GetText();
    m_query.isWholeDisk    = m_wholeDisk.IsChecked();
    m_query.isAnyBitOffset = m_anyBit.IsChecked() && m_query.kind == SearchKind::Nibbles;
}
