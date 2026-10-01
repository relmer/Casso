#pragma once

#include "Pch.h"
#include "Window/DxuiCaptionDragTracker.h"
#include "Window/DxuiWindow.h"
#include "Widgets/DxuiDockSite.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow
//
//  A top-level window whose content is one DxuiDockSite: the window a pane
//  floats in. The application moves the pane's controls in with AttachChild
//  and out again with DetachChild, so a control passes between windows
//  intact, and adds the pane to the site as it would in its main window.
//
//  INPUT THE SITE DOES NOT TAKE GOES TO THE APPLICATION. Panes route their
//  own input in the application that owns them, so the window offers each
//  mouse and key event to the site and hands the rest on unchanged.
//
//  A CAPTION DRAG IS REPORTED. While the user moves the window by its title
//  bar the OS runs its move loop; each tick reports the cursor, in screen
//  pixels, so the owner can show its drop zones under it, and the first
//  end of the move loop reports where it ended, so the owner can dock the
//  pane there. A resize is not reported, and a drag that turns out to be one
//  is reported canceled, so the owner can take its drop zones down.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiDockedWindow : public DxuiWindow
{
public:
    using  MouseFn   = std::function<bool (const DxuiMouseEvent & ev)>;
    using  KeyFn     = std::function<bool (const DxuiKeyEvent & ev)>;
    using  PointFn   = std::function<void (POINT screenPx)>;
    using  ClosedFn  = std::function<void ()>;
    using  CommandFn = std::function<bool (int commandId)>;
    using  FilesFn   = std::function<bool (const std::vector<std::wstring> & paths)>;

    DxuiDockedWindow  () = default;
    ~DxuiDockedWindow () override = default;

    HRESULT  Create (const CreateParams & params);

    DxuiDockSite &  GetSite () { return *m_site; }

    void  SetOnContentMouse    (MouseFn fn)  { m_onMouse     = std::move (fn); }
    void  SetOnContentKey      (KeyFn fn)    { m_onKey       = std::move (fn); }
    void  SetOnCaptionDrag     (PointFn fn)  { m_onDrag      = std::move (fn); }
    void  SetOnCaptionDragEnd  (PointFn fn)  { m_onDragEnd   = std::move (fn); }
    void  SetOnCaptionDragCancel (ClosedFn fn) { m_onDragCancel = std::move (fn); }
    void  SetOnClosed            (ClosedFn fn) { m_onClosed     = std::move (fn); }

    //  A key the window's key map translates, for an owner that keeps one
    //  set of commands across its windows.
    void  SetOnMappedCommand   (CommandFn fn) { m_onCommand  = std::move (fn); }

    //  Files dropped on the window, once SetAcceptsDroppedFiles has turned
    //  that on, for an owner that takes them the same in all its windows.
    void  SetOnFilesDropped    (FilesFn fn)   { m_onFiles    = std::move (fn); }

    //  Once a frame: reports the end of a caption drag the end of the move
    //  loop did not.
    void  PollCaptionDrag ();

    //  Moves and sizes the window, in screen pixels.
    void  SetScreenRect (const RECT & rectPx);
    RECT  GetScreenRect () const;

    //  While on, the row of the site's carried tab, or of its title bar,
    //  fades so a tab strip under the window shows through it. It shows
    //  only in a window created composited.
    void  SetHeaderFade (bool on);
    bool  IsHeaderFaded () const { return m_headerFade; }

    //  The bands that fade a row, given in pixels: black, most opaque at the
    //  row's left edge and gone halfway across, so drawn over the row they
    //  take away that much of it.
    static std::vector<DxuiDockDragMark>  GetHeaderFade (const RECT & rowPx);

protected:
    void  OnCreate      () override;
    void  OnWindowClose () override;
    void  Layout        (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool  OnMouse       (const DxuiMouseEvent & ev) override;
    bool  OnKey         (const DxuiKeyEvent   & ev) override;
    LPCWSTR  GetCursorForPoint (POINT clientPx) const override;
    bool  OnMappedCommand (int commandId) override;
    bool  OnFilesDropped  (const std::vector<std::wstring> & paths) override;
    void  OnWindowPlaced () override;
    void  OnDpiChanged   (UINT newDpi) override;
    bool  HasModalOverlay   () const override { return m_headerFade; }
    void  PaintModalOverlay (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

private:
    static constexpr int    kFadeBands    = 16;
    static constexpr float  kFadeMaxErase = 0.7f;

    void  OnMoveLoopTick ();
    void  Report         (DxuiCaptionDragTracker::Event ev);
    SIZE  GetScreenSize  () const;

    DxuiDockSite            * m_site         = nullptr;
    MouseFn                   m_onMouse;
    KeyFn                     m_onKey;
    PointFn                   m_onDrag;
    PointFn                   m_onDragEnd;
    ClosedFn                  m_onDragCancel;
    ClosedFn                  m_onClosed;
    CommandFn                 m_onCommand;
    FilesFn                   m_onFiles;
    DxuiCaptionDragTracker    m_drag;
    bool                      m_headerFade   = false;
};