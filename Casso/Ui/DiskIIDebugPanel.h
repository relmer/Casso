#pragma once

#include "Chrome/ChromedPanelWindow.h"
#include "Chrome/IChromedPanelContent.h"
#include "DiskIIDebugPanelLayout.h"

#include "../../CassoEmuCore/Devices/IDiskIIEventSink.h"
#include "../../CassoEmuCore/Devices/DiskIIEventRing.h"
#include "../../CassoEmuCore/Audio/IDriveAudioEventSink.h"


struct ChromeTheme;
class TitleBar;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIIDebugPanel
//
//  Spec-011 / US7. Themed DX replacement for the legacy Win32
//  DiskIIDebugDialog. Hosts itself inside a ChromedPanelWindow and
//  implements the same two event-sink interfaces (IDiskIIEventSink
//  and IDriveAudioEventSink) so it slots into the existing
//  EmulatorShell event wiring with no contract changes.
//
//  T044 lands this empty -- chrome + state binding only, no controls.
//  T046 brings the layout, T047-T057 the individual control families.
//  Until T046, every sink callback is a no-op so the panel never
//  drops events but also never re-renders.
//
////////////////////////////////////////////////////////////////////////////////

class DiskIIDebugPanel : public IChromedPanelContent,
                         public IDiskIIEventSink,
                         public IDriveAudioEventSink
{
public:
    DiskIIDebugPanel  ();
    ~DiskIIDebugPanel () override;

    HRESULT Create  (HINSTANCE              hInstance,
                     HWND                   hwndOwner,
                     ID3D11Device         * device,
                     ID3D11DeviceContext  * context,
                     const ChromeTheme    * theme);
    void    Show    ();
    void    Hide    ();
    void    Destroy ();

    bool    IsOpen () const { return m_window.IsOpen(); }
    HWND    Hwnd   () const { return m_window.Hwnd(); }

    HRESULT RenderFrame ();
    void    SetTheme    (const ChromeTheme * theme);

    // IChromedPanelContent.
    LPCWSTR  GetWindowClassName () const override;
    LPCWSTR  GetWindowTitle     () const override;
    HRESULT  OnHostCreated      (HWND                   hwnd,
                                 ID3D11Device         * device,
                                 ID3D11DeviceContext  * context,
                                 int                    widthPx,
                                 int                    heightPx,
                                 UINT                   dpi,
                                 TitleBar             * titleBar,
                                 const ChromeTheme    * theme) override;
    void     OnHostDestroyed    ()                                  override;
    HRESULT  OnHostResize       (int widthPx, int heightPx, UINT dpi) override;
    void     SetChromeTheme     (TitleBar * titleBar, const ChromeTheme * theme) override;
    SIZE     PreferredClientSize (UINT dpi) const                   override;
    HRESULT  Render             ()                                  override;
    void     OnLButtonDown      (int x, int y)                      override;
    void     OnLButtonUp        (int x, int y)                      override;
    void     OnMouseMove        (int x, int y)                      override;
    bool     OnKey              (WPARAM vk)                         override;
    void     Accept             ()                                  override;
    void     Cancel             ()                                  override;
    bool     IsContentActive    () const                            override;

    // IDiskIIEventSink. All no-ops for T044; T046+ pushes into m_state.
    void OnMotorCommandOn  ()                                       override {}
    void OnMotorEngaged    ()                                       override {}
    void OnMotorCommandOff ()                                       override {}
    void OnMotorDisengaged ()                                       override {}
    void OnHeadStep        (int prevQt, int newQt)                  override { (void) prevQt; (void) newQt; }
    void OnHeadBump        (int atQt)                               override { (void) atQt; }
    void OnAddressMark     (int track, int sector, int volume)      override { (void) track; (void) sector; (void) volume; }
    void OnDataMarkRead    (int track, int sector, int volume, int byteCount) override { (void) track; (void) sector; (void) volume; (void) byteCount; }
    void OnDataMarkWrite   (int track, int sector, int volume, int byteCount) override { (void) track; (void) sector; (void) volume; (void) byteCount; }
    void OnDriveSelect     (int drive)                              override { (void) drive; }
    void OnDiskInserted    (int drive)                              override { (void) drive; }
    void OnDiskEjected     (int drive)                              override { (void) drive; }

    // IDriveAudioEventSink. All no-ops for T044.
    void OnAudioStarted     (SoundKind kind, int drive)                    override { (void) kind; (void) drive; }
    void OnAudioRestarted   (SoundKind kind, int drive)                    override { (void) kind; (void) drive; }
    void OnAudioContinued   (SoundKind kind, int drive)                    override { (void) kind; (void) drive; }
    void OnAudioSilent      (SoundKind kind, int drive, SilentReason reason) override { (void) kind; (void) drive; (void) reason; }
    void OnAudioLoopStarted (SoundKind kind, int drive)                    override { (void) kind; (void) drive; }
    void OnAudioLoopStopped (SoundKind kind, int drive)                    override { (void) kind; (void) drive; }

private:
    HRESULT EnsureSwapChain      ();
    HRESULT CreateBackBufferRtv  ();
    void    ReleaseRenderTargets ();
    void    RecomputeLayout      ();

    ChromedPanelWindow                   m_window;
    PanelLayoutSlots                     m_layout = {};

    ID3D11Device                       * m_device  = nullptr;
    ID3D11DeviceContext                * m_context = nullptr;
    const ChromeTheme                  * m_theme   = nullptr;
    TitleBar                           * m_titleBar = nullptr;
    HWND                                 m_hwnd    = nullptr;
    int                                  m_widthPx  = 0;
    int                                  m_heightPx = 0;
    UINT                                 m_dpi      = 96;

    Microsoft::WRL::ComPtr<IDXGISwapChain1>           m_swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>    m_rtv;
};
