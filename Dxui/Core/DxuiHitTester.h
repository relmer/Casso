#pragma once

////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHitTester
//
//  Pure-logic rect tree the native UI runtime consults for both client
//  input (mouse / drag-drop) and `WM_NCHITTEST` classification. Widgets
//  register a rect plus a typed slot; the tester walks the registration
//  list in reverse-insert order so later (z-on-top) registrations win
//  ties.
//
//  The NC path adds a DPI-scaled resize-edge inset around the outer
//  client rect and maps the eight resize codes plus min/max/close/
//  caption/client. The caller (Window.cpp) passes screen-space
//  coordinates and the same outer client rect; the helper does the
//  screen → client conversion before dispatch.
//
////////////////////////////////////////////////////////////////////////////////

enum class DxuiHitSlot
{
    None        = 0,
    Client      = 1,
    Caption     = 2,
    Minimize    = 3,
    Maximize    = 4,
    Close       = 5,
    Custom      = 6,
};


struct DxuiHitRect
{
    RECT         rect      = {};
    DxuiHitSlot  slot      = DxuiHitSlot::None;
    int          tag       = 0;       // caller-defined identifier (widget id)
};


struct DxuiNcHitTestInput
{
    RECT  windowRectScreen = {};
    int   mouseXScreen     = 0;
    int   mouseYScreen     = 0;
    int   resizeBorderPx   = 0;
    RECT  captionRect      = {};
    RECT  minButtonRect    = {};
    RECT  maxButtonRect    = {};
    RECT  closeButtonRect  = {};
};


class DxuiHitTester
{
public:
    DxuiHitTester  () = default;
    ~DxuiHitTester() = default;

    void                              Clear         ();
    void                              Register      (const DxuiHitRect & rect);
    const DxuiHitRect               * Pick          (int xClient, int yClient) const;
    const std::vector<DxuiHitRect>  & Registrations() const { return m_rects; }

    static LRESULT                    ClassifyNcHit (const DxuiNcHitTestInput & in);

private:
    std::vector<DxuiHitRect>  m_rects;
};
