#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Core/IDxuiLayout.h"



////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanel
//
//  Container control. Owns its children (std::unique_ptr<IDxuiControl>)
//  and an optional layout policy. The layout policy is invoked by
//  Layout() to assign bounds to each visible child; Paint(), OnMouse(),
//  OnKey(), Tick(), and OnThemeChanged() are auto-fanned-out across
//  children.
//
//  Visibility uses Collapsed-only semantics (FR-011): SetVisible(false)
//  on a child triggers a relayout on the next pump so siblings reclaim
//  the freed space.
//
//  All public methods are called on the UI thread (FR-083); each entry
//  point asserts this in debug builds.
//
////////////////////////////////////////////////////////////////////////////////



class DxuiPanel : public IDxuiControl
{
public:
    DxuiPanel  ();
    ~DxuiPanel() override;

    template <class T, class... Args>
    T &  Add  (Args &&... args)
    {
        std::unique_ptr<T>  child = std::make_unique<T> (std::forward<Args> (args)...);
        T *                 raw   = child.get();
        AppendChild (std::move (child));
        return *raw;
    }

    //
    //  Create — MFC/CreateWindow-style factory. Constructs a child of
    //  type T (forwarding any common-property constructor arguments,
    //  e.g. a label / text), parents it into this panel's child list
    //  (owning), and returns a raw observer pointer. The `<T>` is the
    //  type-safe analog of CreateWindow's class argument. Callers keep
    //  the returned pointer only for controls they touch later; pure-
    //  display children (static labels) can ignore the return value.
    //
    template <class T, class... Args>
    T *  Create  (Args &&... args)
    {
        std::unique_ptr<T>  child = std::make_unique<T> (std::forward<Args> (args)...);
        T *                 raw   = child.get();
        AppendChild (std::move (child));
        return raw;
    }

    //
    //  Adopt — non-owning child registration. The panel includes the
    //  adopted control in the unified child list so it participates in
    //  paint, input, focus, theme, tick, and DPI walks alongside Add'd
    //  children. Adopted entries are interleaved in insertion order
    //  with owned entries so the paint front-to-back and input
    //  back-to-front guarantees are preserved.
    //
    //  Lifetime contract: the caller retains ownership and MUST keep
    //  the adopted child alive at least as long as the panel keeps a
    //  reference to it. Failure causes use-after-free during paint or
    //  input dispatch. Use RemoveAdopted / ClearAdopted before the
    //  child is destroyed, or destroy the panel first.
    //
    //  Re-adopting the same pointer is a no-op (returns without
    //  reparenting). Adopting a control already owned by this panel
    //  via Add<T>() is a programming error and asserts.
    //
    void  Adopt         (IDxuiControl & nonOwnedChild);
    bool  RemoveAdopted (IDxuiControl & child);
    void  ClearAdopted  ();

    bool  Remove        (IDxuiControl * child);
    void  Clear         ();
    void  SetLayout     (std::unique_ptr<IDxuiLayout> layout);

    void  PropagateDpi  (const DxuiDpiScaler & scaler);
    void  PropagateTheme();

    bool  Dirty         () const { return m_dirty; }
    void  ClearDirty    ()       { m_dirty = false; }

    void              Layout       (const RECT          & boundsDip,
                                    const DxuiDpiScaler & scaler) override;
    void              Paint        (IDxuiPainter        & painter,
                                    IDxuiTextRenderer   & text,
                                    const IDxuiTheme    & theme) override;
    bool              OnMouse      (const DxuiMouseEvent & ev) override;
    LPCWSTR           CursorForPoint (POINT clientPx) const     override;
    bool              OnKey        (const DxuiKeyEvent   & ev) override;
    void              OnThemeChanged()                       override;
    void              Tick         (int64_t nowMs)            override;

    void              OnChildVisibilityChanged (IDxuiControl * child);

    size_t            ChildCount   () const                   override { return m_children.size(); }
    IDxuiControl *    Child        (size_t index) const       override { return (index < m_children.size()) ? m_children[index].raw : nullptr; }

protected:
    void              OnVisibilityChanged() override;

private:
    void              AppendChild  (std::unique_ptr<IDxuiControl> child);
    void              MarkDirty    ();

    //
    //  Unified child entry. Owned entries hold `owned` (a unique_ptr
    //  to the child) with `raw == owned.get()`. Adopted entries hold
    //  `owned == nullptr` and `raw == <external pointer>`. Insertion
    //  order is preserved across Add / Adopt calls so the paint
    //  back-to-front and input front-to-back walks see one unified
    //  list.
    //
    struct ChildSlot
    {
        IDxuiControl *                  raw   = nullptr;
        std::unique_ptr<IDxuiControl>   owned;
    };

    std::vector<ChildSlot>                      m_children;
    std::unique_ptr<IDxuiLayout>                m_layout;
    bool                                        m_dirty       = false;
    RECT                                        m_lastBoundsDip = {};
    DxuiDpiScaler                               m_lastScaler;
    bool                                        m_haveLast    = false;
};
