#include "Pch.h"

#include "DxuiHitTester.h"





namespace
{
    inline bool RectContains (const RECT & r, int x, int y)
    {
        return (x >= r.left) && (x < r.right) && (y >= r.top) && (y < r.bottom);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Clear
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHitTester::Clear()
{
    m_rects.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Register
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHitTester::Register (const DxuiHitRect & rect)
{
    m_rects.push_back (rect);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Pick
//
//  Walks registrations in reverse-insert order so later registrations
//  (which render on top) take precedence on hit ties.
//
////////////////////////////////////////////////////////////////////////////////

const DxuiHitRect * DxuiHitTester::Pick (int xClient, int yClient) const
{
    for (auto it = m_rects.rbegin(); it != m_rects.rend(); ++it)
    {
        if (RectContains (it->rect, xClient, yClient))
        {
            return &(*it);
        }
    }

    return nullptr;
}
