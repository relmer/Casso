#pragma once

#include "Pch.h"

#include "DialogDefinition.h"
#include "DialogLayout.h"
#include "DialogPrimitiveRenderer.h"
#include "../Widgets/Button.h"


struct ChromeTheme;




////////////////////////////////////////////////////////////////////////////////
//
//  DialogPrimitive
//
//  Themed modal dialog window. Call Show() to display the dialog
//  synchronously from the UI thread. Show() creates the window, runs
//  its own GetMessage loop (disabling the owner window for the
//  duration), and returns the chosen button's resultCode, or -1 when
//  the user dismisses with Alt+F4 / WM_CLOSE.
//
////////////////////////////////////////////////////////////////////////////////

class DialogPrimitive
{
public:
    DialogPrimitive  () = default;
    ~DialogPrimitive ();

    HRESULT  RegisterClass (HINSTANCE hInstance);
    int      Show          (HWND                     hwndOwner,
                            ID3D11Device           * device,
                            ID3D11DeviceContext    * context,
                            const ChromeTheme      * theme,
                            const DialogDefinition & def);
    void     Close         (int chosenId);

    // Runtime button mutation. Safe to call from inside an
    // `onButtonActivated` hook to switch the dialog into a different
    // mode (e.g. show "Downloading..." after the user clicks Download)
    // without closing the window. The dialog repaints on the next
    // frame; call Repaint() to schedule an immediate invalidation.
    void     SetButtonLabel   (size_t idx, const std::wstring & label);
    void     SetButtonEnabled (size_t idx, bool enabled);
    void     SetButtonVisible (size_t idx, bool visible);
    void     Repaint          ();
    HWND     Hwnd             () const { return m_hwnd; }

private:
    static LRESULT CALLBACK s_WndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    LRESULT  WndProc        (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    HRESULT  OnCreate       (HWND hwnd);
    void     OnDestroy      ();
    void     OnSize         (int widthPx, int heightPx);
    void     OnDpiChanged   (UINT dpi, const RECT & suggestedRect);
    void     OnKeyDown      (WPARAM vk);
    void     OnMouse        (UINT message, WPARAM wParam, LPARAM lParam);
    void     OnClose        ();

    void     BuildButtons         ();
    void     RecomputeLayout      (UINT dpi);
    void     RenderFrame          ();
    void     ActivateButton       (size_t idx);
    void     ActivateDefaultButton();
    void     ActivateCancelButton ();
    void     CycleFocus           (int delta);
    bool     HitTestHyperlink     (int xPx, int yPx, size_t & outBodyRunIdx) const;
    void     LaunchHyperlink      (size_t bodyRunIdx);
    bool     DispatchCustomBodyInput (DialogInputEvent::Kind kind, int xPx, int yPx, int vkCode);
    size_t   HyperlinkCount       () const;
    size_t   NthHyperlinkBodyIdx  (size_t hyperlinkIdx) const;

    int      TitleHeightPx        () const;
    RECT     CloseButtonRectPx    () const;
    bool     PointInCloseButton   (int xPx, int yPx) const;
    RECT     GetInitialWindowRect (HWND hwndOwner, UINT dpi) const;
    size_t   DefaultButtonIdx     () const;
    size_t   CancelButtonIdx      () const;

    HINSTANCE                    m_hInstance     = nullptr;
    HWND                         m_hwnd          = nullptr;
    HWND                         m_hwndOwner     = nullptr;
    ID3D11Device               * m_device        = nullptr;   // non-owning
    ID3D11DeviceContext        * m_context       = nullptr;   // non-owning
    const ChromeTheme          * m_theme         = nullptr;   // non-owning
    const DialogDefinition     * m_def           = nullptr;   // non-owning

    DialogPrimitiveRenderer      m_renderer;
    DialogLayoutResult           m_layout;
    std::vector<Button>          m_buttons;
    size_t                       m_focusedButton    = SIZE_MAX;
    size_t                       m_focusedHyperlink = SIZE_MAX;
    size_t                       m_hoveredHyperlink = SIZE_MAX;
    int                          m_chosenId         = -1;
    UINT                         m_dpi              = DpiScaler::kBaseDpi;
    bool                         m_closed           = false;
    bool                         m_closeHovered     = false;
    bool                         m_closePressed     = false;
};
