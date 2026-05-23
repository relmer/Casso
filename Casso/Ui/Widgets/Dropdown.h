#pragma once

#include "Pch.h"





class Dropdown
{
public:
    using SelectFn = std::function<void (int index)>;

    void  SetItems       (const std::vector<std::wstring> & items) { m_items = items; }
    void  SetSelect      (SelectFn select) { m_select = std::move (select); }
    void  Open           () { m_open = true; m_highlight = m_items.empty() ? -1 : 0; }
    void  Close          () { m_open = false; }
    bool  IsOpen         () const { return m_open; }
    int   HighlightIndex () const { return m_highlight; }
    bool  HandleKey      (WPARAM vk);

private:
    std::vector<std::wstring>  m_items;
    SelectFn                  m_select;
    bool                      m_open      = false;
    int                       m_highlight = -1;
};
