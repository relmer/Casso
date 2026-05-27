#pragma once

#include "Pch.h"

#include "../DxUiPainter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ModalScrim
//
//  Pure-logic scrim + dispatch helper for the settings panel's modal
//  confirm dialog (the reset-required apply path). Owns no rendering
//  state beyond the dim color and current visibility flag; the actual
//  Yes / No buttons live in the surrounding panel.
//
//  Dispatch contract:
//      * `Show (onConfirm, onCancel)` lights the scrim and remembers
//        the two callbacks.
//      * `Confirm()`  invokes onConfirm and hides the scrim.
//      * `Cancel ()`  invokes onCancel  and hides the scrim.
//      * `OnKey (vk)` maps Esc -> Cancel, Enter -> Confirm.
//      * Any other key returns false (so the panel can let focus
//        rotate through Yes / No while the scrim is up).
//
////////////////////////////////////////////////////////////////////////////////

class ModalScrim
{
public:
    using ActionFn = std::function<void ()>;

    void  SetViewportRect (const RECT & rect) { m_viewport = rect; }
    void  SetDimArgb      (uint32_t argb) { m_dimArgb = argb; }

    void  Show            (ActionFn onConfirm, ActionFn onCancel);
    void  Hide            ();
    void  Confirm         ();
    void  Cancel          ();
    bool  IsVisible       () const { return m_visible; }

    bool  OnKey           (WPARAM vk);
    void  Paint           (DxUiPainter & painter) const;

private:
    RECT      m_viewport = {};
    ActionFn  m_onConfirm;
    ActionFn  m_onCancel;
    uint32_t  m_dimArgb  = 0x80000000;
    bool      m_visible  = false;
};
