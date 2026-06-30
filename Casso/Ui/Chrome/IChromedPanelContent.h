#pragma once

#include "Pch.h"


class DxuiHostWindow;
struct CassoTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  IChromedPanelContent
//
//  Abstract content interface implemented by every panel hosted in a
//  ChromedPanelWindow. The chrome shell owns the HWND, the title bar
//  geometry, NC hit testing, DPI handling, and the mouse / keyboard
//  routing. The content owns the panel's renderer (each panel has its
//  own shader stack), the preferred-size policy, and the input
//  semantics (what Accept / Cancel mean, what's a click target, etc).
//
//  Lifecycle: ChromedPanelWindow calls OnHostCreated when its HWND is
//  ready (renderer init), Render on each paint, OnHostResize on
//  WM_SIZE / WM_DPICHANGED, OnHostDestroyed in WM_DESTROY. The content
//  must NOT touch the HWND after OnHostDestroyed returns.
//
////////////////////////////////////////////////////////////////////////////////

class IChromedPanelContent
{
public:
    virtual ~IChromedPanelContent () = default;

    // Identity used to register the Win32 window class for this content.
    // Each content type uses a distinct class name so multiple panel
    // types can be open simultaneously without sharing a wndproc dispatch.
    virtual LPCWSTR  GetWindowClassName () const = 0;
    virtual LPCWSTR  GetWindowTitle     () const = 0;

    // Renderer lifecycle. The content owns the renderer; the shell
    // hands it the HWND + device + initial size so it can stand up
    // its swap chain. SetChromeTheme also runs at create time so the
    // renderer can size title-bar metrics on first paint.
    virtual HRESULT  OnHostCreated   (HWND                   hwnd,
                                      ID3D11Device         * device,
                                      ID3D11DeviceContext  * context,
                                      int                    widthPx,
                                      int                    heightPx,
                                      UINT                   dpi,
                                      DxuiHostWindow       * captionHost,
                                      const CassoTheme    * theme) = 0;
    virtual void     OnHostDestroyed () = 0;
    virtual HRESULT  OnHostResize    (int widthPx, int heightPx, UINT dpi) = 0;
    virtual HRESULT  Render          () = 0;
    virtual void     SetChromeTheme  (DxuiHostWindow * captionHost, const CassoTheme * theme) = 0;

    // Layout. The shell uses this to size the popup on first show and
    // to honour WM_GETMINMAXINFO under each DPI.
    virtual SIZE     PreferredClientSize (UINT dpi) const = 0;

    // Input routing. Coordinates are client-relative. Mouse-wheel default
    // is a no-op since most panels don't scroll.
    virtual void     OnLButtonDown (int x, int y) = 0;
    virtual void     OnLButtonUp   (int x, int y) = 0;
    virtual void     OnRButtonDown (int x, int y) { (void) x; (void) y; }
    virtual void     OnMouseMove   (int x, int y) = 0;
    virtual void     OnMouseWheel  (int x, int y, int delta) { (void) x; (void) y; (void) delta; }
    virtual bool     OnKey         (WPARAM vk) = 0;
    virtual bool     OnChar        (wchar_t ch) { (void) ch; return false; }

    // Action semantics. Accept = VK_RETURN, Cancel = VK_ESCAPE /
    // WM_CLOSE / window-close. After either, the shell calls
    // IsContentActive to decide whether to destroy the host window.
    virtual void     Accept () = 0;
    virtual void     Cancel () = 0;
    virtual bool     IsContentActive () const = 0;

    // Window-ownership policy. Returning true makes ChromedPanelWindow
    // create the host HWND with NO owner so the panel can move freely
    // behind the main emulator window. Default is false (owned popup,
    // Windows pins it above the owner — appropriate for modal/inline
    // panels like Settings). Non-modal observability panels (debug
    // console, Disk II event viewer) should return true so the user
    // can park them behind Casso while watching the emulator.
    virtual bool     IsNonModal () const { return false; }

    // Cursor hook. Called from WM_SETCURSOR with client-relative coords
    // for the current cursor position. Return a loaded HCURSOR to
    // override the default arrow, or nullptr to let the shell fall
    // through to DefWindowProc (which paints the standard arrow).
    virtual HCURSOR  OnSetCursor (int x, int y) { (void) x; (void) y; return nullptr; }
};
