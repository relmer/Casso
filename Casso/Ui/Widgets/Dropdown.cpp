#include "Pch.h"

#include "Dropdown.h"





bool Dropdown::HandleKey (WPARAM vk)
{
    int  count = (int) m_items.size();



    if (!m_open || count <= 0)
    {
        return false;
    }

    if (vk == VK_DOWN)
    {
        m_highlight = (m_highlight + 1) % count;
        return true;
    }

    if (vk == VK_UP)
    {
        m_highlight = (m_highlight + count - 1) % count;
        return true;
    }

    if (vk == VK_RETURN && m_select)
    {
        m_select (m_highlight);
        Close();
        return true;
    }

    return false;
}
