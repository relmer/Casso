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

    void  Create               () override;
    void  Configure            () override;
    void  Layout               (const RECT & content) override;
    void  Hide                 () override;
    bool  UpdateScrollbarHover (POINT at) override;
    bool  TickScrollbars       (int64_t nowMs) override;

    void            Fill                  (const PreviewContent & preview);
    void            SetVisible            (bool visible);
    void            SetZoom               (float zoom);
    bool            IsShowing             () const;
    bool            HandleCapturedMouse   (const DxuiMouseEvent & ev);
    bool            HandleMouse           (const DxuiMouseEvent & ev);
    bool            HandleKey             (const DxuiKeyEvent & ev) { return m_text->OnKey (ev); }
    LPCWSTR         GetCursorForPoint     (POINT clientPx) const;
    bool            TryGetScrollbarMenu   (const DxuiMouseEvent & ev, std::vector<DxuiPopupMenuItem> & outItems) const;
    bool            TryGetSelectionDetail (std::wstring & outDetail) const;
    void            ToggleLineAddresses   ();
    IDxuiControl  * GetControl            () const { return m_text; }

private:
    void  ShowContextMenu (int x, int y);

    DxuiTextView  * m_text = nullptr;
};
