#pragma once

#include "Pch.h"

#include "CassoExplorer/Window/Panes/Preview/PreviewView.h"

struct PreviewContent;





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView
//
//  A preview drawn as text, which wraps and selects by character: a listing,
//  a document, a disassembly's lines.
//
////////////////////////////////////////////////////////////////////////////////

class PreviewTextView : public PreviewView
{
public:
    PreviewTextView (ExplorerPreviewManager & manager, ExplorerContext & context);

    void            Create                () override;
    void            Configure             () override;
    void            Layout                (const RECT & content) override;
    void            Hide                  () override;
    bool            UpdateScrollbarHover  (POINT at) override;
    bool            TickScrollbars        (int64_t nowMs) override;
    void            Fill                  (const PreviewContent & preview);
    void            SetVisible            (bool visible);
    void            SetZoom               (float zoom);
    bool            IsShowing             () const;
    LPCWSTR         GetCursorForPoint     (POINT clientPx) const;
    bool            TryGetScrollbarMenu   (const DxuiMouseEvent & ev, std::vector<DxuiPopupMenuItem> & outItems) const;
    bool            TryGetSelectionDetail (std::wstring & outDetail) const;
    void            ToggleLineAddresses   ();
    void            ToggleCrossReference  ();
    bool            IsCrossReference      () const { return m_crossReference; }
    IDxuiControl  * GetControl            () const { return m_text; }
    int             GetContentHeightPx    () const override { return m_text->IsVisible() ? m_text->GetContentHeightPx() : 0; }
    bool            IsAtTop               () const override { return !m_text->IsVisible() || m_text->GetTopLine() == 0; }
    int             GetScrollPx           () const override { return m_text->GetTopLine() * m_text->GetLineHeightPx(); }
    void            SetScrollPx           (int scrollPx) override { m_text->SetTopLine ((scrollPx + m_text->GetLineHeightPx() / 2) / (std::max) (1, m_text->GetLineHeightPx())); }
    void            SetScrollbarHidden    (bool isHidden) override { m_text->SetVerticalScrollbarHidden (isHidden); }

    bool            HandleKey             (const DxuiKeyEvent & ev) { return m_text->OnKey (ev); }

    //  Every mouse event in the window: a move over a document's picture with
    //  tips, such as a Wizardry map, puts up the tip of the cell under the
    //  pointer, and any other event hides it.
    void            UpdatePictureTipAt    (const DxuiMouseEvent & ev);

    //  The width a document's record tables are drawn as tables from;
    //  narrower, they are cards.
    static constexpr int  s_kWideTableDip = 640;

private:
    void  ShowContextMenu (int x, int y);
    bool  HandleTextMouse (const DxuiMouseEvent & ev);
    bool  IsWide          () const;
    void  FillRows        (const PreviewContent & preview);

    DxuiTextView      * m_text           = nullptr;
    bool                m_crossReference = false;
    bool                m_wide           = false;
    LONG                m_tableWidth     = 0;      // the text's width the rows were built for
    std::vector<int>    m_rowParagraph;   // the document paragraph of each row
};
