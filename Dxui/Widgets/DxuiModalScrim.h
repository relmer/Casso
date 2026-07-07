#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"






////////////////////////////////////////////////////////////////////////////////
//
//  DxuiModalScrim
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

class DxuiModalScrim : public IDxuiControl
{
public:
    using ActionFn = std::function<void ()>;

    ~DxuiModalScrim() override = default;

    void  SetViewportRect (const RECT & rect) { m_viewport = rect; SetBounds (rect); }
    void  SetDimArgb      (uint32_t argb) { m_dimArgb = argb; }

    void  Show            (ActionFn onConfirm, ActionFn onCancel);
    void  Hide            ();
    void  Confirm         ();
    void  Cancel          ();
    bool  IsVisible       () const { return m_visible; }

    bool  OnKey           (WPARAM vk);
    void  Paint           (IDxuiPainter & painter) const;

    //
    //  IDxuiControl overrides — additive shims so DxuiModalScrim can
    //  appear in a DxuiPanel tree as a full-bleed overlay child.
    //
    void                Layout         (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint          (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnKey          (const DxuiKeyEvent   & ev) override;
    DxuiAccessibleRole  AccessibleRole () const override { return DxuiAccessibleRole::Dialog; }

private:
    RECT      m_viewport = {};
    ActionFn  m_onConfirm;
    ActionFn  m_onCancel;
    uint32_t  m_dimArgb  = 0x80000000;
    bool      m_visible  = false;
};
