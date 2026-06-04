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
    ~DxuiPanel () override;

    template <class T, class... Args>
    T &  Add  (Args &&... args)
    {
        std::unique_ptr<T>  child = std::make_unique<T> (std::forward<Args> (args)...);
        T *                 raw   = child.get();
        AppendChild (std::move (child));
        return *raw;
    }

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
    bool              OnKey        (const DxuiKeyEvent   & ev) override;
    void              OnThemeChanged ()                       override;
    void              Tick         (int64_t nowMs)            override;

    size_t            ChildCount   () const                   override { return m_children.size(); }
    IDxuiControl *    Child        (size_t index) const       override { return (index < m_children.size()) ? m_children[index].get() : nullptr; }

protected:
    void              OnVisibilityChanged () override;

private:
    void              AppendChild  (std::unique_ptr<IDxuiControl> child);
    void              MarkDirty    ();

    std::vector<std::unique_ptr<IDxuiControl>>  m_children;
    std::unique_ptr<IDxuiLayout>                m_layout;
    bool                                        m_dirty       = false;
    RECT                                        m_lastBoundsDip = {};
    DxuiDpiScaler                               m_lastScaler;
    bool                                        m_haveLast    = false;
};
