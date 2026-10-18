#pragma once

#include "Ui/Debugger/ColorKeyButton.h"
#include "Ui/Debugger/ColorKeyPopup.h"
#include "Ui/Debugger/ColorLegend.h"
#include "Ui/Debugger/Panes/IDebuggerPane.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IColorKeysHost
//
//  The debugger window, as the panes' info buttons use it: a button made in
//  the window, the window and dock site a pane is in, and the colors and
//  theme the key is drawn in.
//
////////////////////////////////////////////////////////////////////////////////

class IColorKeysHost
{
public:
    virtual ~IColorKeysHost() = default;

    virtual ColorKeyButton *        CreateColorKeyButton (const std::wstring & pane, ColorLegend::Pane legend) = 0;
    virtual DxuiWindow *            GetPaneWindow        (const std::wstring & pane) = 0;
    virtual DxuiDockSite *          FindDockSiteOf       (const std::wstring & pane) const = 0;
    virtual const DxuiDpiScaler &   GetPaneScaler        () const = 0;
    virtual bool                    IsBarRouted          (const std::wstring & pane) const = 0;
    virtual const DxuiTheme *       GetTheme             () const = 0;
    virtual ColorLegend::Palette    GetColorPalette      () const = 0;
    virtual void                    HideRoutedTip        () = 0;
    virtual void                    Repaint              () = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  PaneColorKeys
//
//  Each pane whose colors mean something has an info button at the end of
//  its toolbar, in a band of its own where the pane has no toolbar (the call
//  stack, a device panel with a memory map or a disk head), and one popup
//  draws the key of the button the pointer rests on or that was pressed. A
//  legend function returns the key of a pane whose colors depend on its
//  content, or none to hide the button.
//
////////////////////////////////////////////////////////////////////////////////

class PaneColorKeys
{
public:
    explicit PaneColorKeys (IColorKeysHost & host) : m_host (host) {}

    //  A button for every pane with a key, made after the panes' frames,
    //  which contain the bands.
    void  Create (const std::vector<std::unique_ptr<IDebuggerPane>> & panes);

    ColorKeyButton *       Find     (const std::wstring & pane) const;
    const ColorKeyPopup &  GetPopup () const { return m_popup; }
    bool                   IsShown  () const { return m_popup.IsShown(); }

    void  Place       ();
    RECT  GetBarStrip (const std::wstring & pane, const RECT & slot);
    bool  RouteMouse  (const DxuiMouseEvent & ev);
    void  Hide        ();

    //  The rect of a key pinned open in the window the event being routed
    //  came from.
    bool  TryGetPinnedRect (RECT & rect) const;

private:
    struct Key
    {
        ColorKeyButton                                     * button = nullptr;
        DebuggerPaneFrame                                  * slot   = nullptr;
        std::function<std::optional<ColorLegend::Pane> ()>   legend;
    };

    void  Add             (const std::wstring & pane, const PaneColorKeySpec & spec);
    bool  TryGetTitleRect (const std::wstring & pane, bool reserve, RECT * rect = nullptr);
    void  Show            (ColorKeyButton * button, bool pin);
    void  Toggle          (ColorKeyButton * button);

    IColorKeysHost          & m_host;
    std::vector<Key>          m_keys;
    ColorKeyPopup             m_popup;
    ColorKeyButton          * m_owner = nullptr;
    ColorKeyButton          * m_hover = nullptr;
};
