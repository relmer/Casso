#include "Pch.h"

#include "ModalScrim.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Show
//
////////////////////////////////////////////////////////////////////////////////

void ModalScrim::Show (ActionFn onConfirm, ActionFn onCancel)
{
    m_onConfirm = std::move (onConfirm);
    m_onCancel  = std::move (onCancel);
    m_visible   = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Hide
//
////////////////////////////////////////////////////////////////////////////////

void ModalScrim::Hide ()
{
    m_visible = false;
    m_onConfirm = nullptr;
    m_onCancel  = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Confirm
//
////////////////////////////////////////////////////////////////////////////////

void ModalScrim::Confirm ()
{
    ActionFn  cb;



    if (!m_visible)
    {
        return;
    }

    cb        = std::move (m_onConfirm);
    m_visible = false;
    m_onConfirm = nullptr;
    m_onCancel  = nullptr;

    if (cb)
    {
        cb();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Cancel
//
////////////////////////////////////////////////////////////////////////////////

void ModalScrim::Cancel ()
{
    ActionFn  cb;



    if (!m_visible)
    {
        return;
    }

    cb        = std::move (m_onCancel);
    m_visible = false;
    m_onConfirm = nullptr;
    m_onCancel  = nullptr;

    if (cb)
    {
        cb();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool ModalScrim::OnKey (WPARAM vk)
{
    if (!m_visible)
    {
        return false;
    }

    if (vk == VK_ESCAPE)
    {
        Cancel();
        return true;
    }

    if (vk == VK_RETURN)
    {
        Confirm();
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
////////////////////////////////////////////////////////////////////////////////

void ModalScrim::Paint (DxUiPainter & painter) const
{
    if (!m_visible)
    {
        return;
    }

    painter.FillRect ((float) m_viewport.left,
                      (float) m_viewport.top,
                      (float) (m_viewport.right  - m_viewport.left),
                      (float) (m_viewport.bottom - m_viewport.top),
                      m_dimArgb);
}
