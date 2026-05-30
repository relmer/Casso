#include "Pch.h"

#include "StandaloneDialog.h"

#include "DialogPrimitive.h"
#include "../Chrome/ChromeTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShowStandaloneDialog
//
//  Creates a transient D3D11 device for the lifetime of the modal.
//  When EmulatorShell exists callers should prefer its ShowModalDialog
//  helper so the dialog shares the renderer's device and theme.
//
////////////////////////////////////////////////////////////////////////////////

int ShowStandaloneDialog (HINSTANCE                hInstance,
                          HWND                     hwndOwner,
                          std::string_view         themeName,
                          const DialogDefinition & def)
{
    HRESULT                     hr            = S_OK;
    ComPtr<ID3D11Device>        device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL           level         = D3D_FEATURE_LEVEL_11_0;
    UINT                        createFlags   = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    ChromeTheme                 theme         = ChromeTheme::ForName (std::string (themeName));
    DialogPrimitive             primitive;
    int                         result        = -1;
    HWND                        hwndOwnerSafe = hwndOwner;



    hr = D3D11CreateDevice (nullptr,
                            D3D_DRIVER_TYPE_HARDWARE,
                            nullptr,
                            createFlags,
                            nullptr,
                            0,
                            D3D11_SDK_VERSION,
                            &device,
                            &level,
                            &context);
    CHRA (hr);

    hr = primitive.RegisterClass (hInstance);
    CHRA (hr);

    if (hwndOwnerSafe == nullptr)
    {
        hwndOwnerSafe = GetDesktopWindow();
    }

    result = primitive.Show (hwndOwnerSafe,
                             device.Get(),
                             context.Get(),
                             &theme,
                             def);

Error:
    return result;
}
