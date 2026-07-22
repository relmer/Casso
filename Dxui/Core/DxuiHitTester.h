#pragma once

////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHitTester
//
//  Pure-logic rect tree the native UI runtime consults for client
//  input (mouse / drag-drop). Widgets register a rect plus a typed slot;
//  the tester walks the registration list in reverse-insert order so
//  later (z-on-top) registrations win ties.
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


class DxuiHitTester
{
public:
    DxuiHitTester  () = default;
    ~DxuiHitTester() = default;

    void                              Clear         ();
    void                              Register      (const DxuiHitRect & rect);
    const DxuiHitRect               * Pick          (int xClient, int yClient) const;
    const std::vector<DxuiHitRect>  & Registrations() const { return m_rects; }

private:
    std::vector<DxuiHitRect>  m_rects;
};
