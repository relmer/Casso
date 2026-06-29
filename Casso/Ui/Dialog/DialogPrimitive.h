#pragma once

#include "Pch.h"

#include "DialogDefinition.h"
#include "DialogLayout.h"
#include "DialogPrimitiveRenderer.h"
#include "Widgets/DxuiButton.h"


struct CassoTheme;




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
                            const CassoTheme      * theme,
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

    // Focuses custom-body stop `idx` (-1 clears); the body calls this on
    // click to sync its focus with the dialog's Tab ring.
    void     SetCustomBodyFocus (int idx);

private:
    static LRESULT CALLBACK s_WndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    LRESULT  WndProc        (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    HRESULT  OnCreate       (HWND hwnd);
    void     OnDestroy      ();
    void     OnSize         (int widthPx, int heightPx);
    void     OnDpiChanged   (UINT dpi, const RECT & suggestedRect);
    LRESULT  OnNcHitTest    (LPARAM lParam) const;
    void     OnGetMinMaxInfo (MINMAXINFO * mmi) const;
    void     OnKeyDown      (WPARAM vk);
    bool     OnSysChar      (WPARAM ch);
    void     OnMouse        (UINT message, WPARAM wParam, LPARAM lParam);
    void     OnMouseWheel   (WPARAM wParam, LPARAM lParam);
    void     OnMouseHWheel  (WPARAM wParam, LPARAM lParam);
    bool     OnSetCursor    (LPARAM lParam);
    void     OnClose        ();

    void     BuildButtons    ();
    void     RecomputeLayout (UINT dpi);

    static float MeasureLabelWidthPx (DxuiTextRenderer & measurer, std::wstring_view sv, float fontPx);

    void     RenderFrame          ();
    void     ActivateButton       (size_t idx);
    void     ActivateDefaultButton();
    void     ActivateCancelButton ();
    void     CycleFocus           (int delta);
    bool     HitTestHyperlink     (int xPx, int yPx, size_t & outBodyRunIdx) const;
    void     LaunchHyperlink      (size_t bodyRunIdx);
    size_t   GetHyperlinkCount    () const;
    bool     DispatchCustomBodyInput (DialogInputEvent::Kind kind, int xPx, int yPx, int intArg, bool wheelHorz = false);
    int      GetCustomBodyFocusCount () const;
    size_t   GetNthHyperlinkBodyIdx  (size_t hyperlinkIdx) const;

    int      GetTitleHeightPx     () const;
    RECT     GetCloseButtonRectPx () const;
    bool     IsPointInCloseButton (int xPx, int yPx) const;
    RECT     GetInitialWindowRect (HWND hwndOwner, UINT dpi) const;
    SIZE     GetResizableClientSizePx (UINT dpi) const;
    size_t   GetDefaultButtonIdx  () const;
    size_t   GetCancelButtonIdx   () const;

    HINSTANCE                    m_hInstance     = nullptr;
    HWND                         m_hwnd          = nullptr;
    HWND                         m_hwndOwner     = nullptr;
    ID3D11Device               * m_device        = nullptr;   // non-owning
    ID3D11DeviceContext        * m_context       = nullptr;   // non-owning
    const CassoTheme          * m_theme         = nullptr;   // non-owning
    const DialogDefinition     * m_def           = nullptr;   // non-owning

    DialogPrimitiveRenderer      m_renderer;
    DialogLayoutResult           m_layout;
    SIZE                         m_fillSizePx        = {};   // resizable: current content fill size (client minus title bar)
    std::vector<DxuiButton>      m_buttons;
    size_t                       m_focusedButton     = SIZE_MAX;
    size_t                       m_focusedHyperlink  = SIZE_MAX;
    int                          m_focusedCustomBody = -1;
    size_t                       m_hoveredHyperlink  = SIZE_MAX;
    int                          m_chosenId          = -1;
    UINT                         m_dpi               = DxuiDpiScaler::kBaseDpi;
    bool                         m_closed            = false;
    bool                         m_closeHovered      = false;
    bool                         m_closePressed      = false;
};
