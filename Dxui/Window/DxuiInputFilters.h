#pragma once

#include "Pch.h"
#include "Core/DxuiEvents.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInputFilters
//
//  Handlers called with a window's input before the window routes it. A
//  mouse or key handler that returns true consumes the event, and the
//  handlers after it are not called. Every loss handler is called for
//  WM_CAPTURECHANGED to another window, or none, and for WM_CANCELMODE, with
//  the message. One filter per key; adding a key again replaces its filter.
//  Any handler may be empty.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiInputFilters
{
public:
    struct Filter
    {
        std::function<bool (const DxuiMouseEvent & ev)>  onMouse;
        std::function<bool (WPARAM vk)>                  onKeyDown;
        std::function<void (UINT message)>               onMouseLost;
    };

    void  Add          (const void * key, Filter filter);
    void  Remove       (const void * key);
    bool  RunMouse     (const DxuiMouseEvent & ev) const;
    bool  RunKey       (WPARAM vk) const;
    void  RunMouseLost (UINT message) const;

private:
    std::vector<std::pair<const void *, Filter>>  m_filters;
};
