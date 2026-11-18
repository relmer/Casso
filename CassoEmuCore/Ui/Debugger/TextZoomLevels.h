#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  TextZoomLevels
//
//  The debugger's text sizes: the window's, which the status bar sets and
//  every pane's text follows, and each pane's over the window's, which
//  Ctrl+wheel over a pane sets.
//
////////////////////////////////////////////////////////////////////////////////

class TextZoomLevels
{
public:
    static constexpr float  kMinZoom     = 0.5f;
    static constexpr float  kMaxZoom     = 3.0f;
    static constexpr float  kZoomStep    = 0.1f;
    static constexpr float  kDefaultZoom = 1.0f;

    //  A size moved by whole steps, within the limits.
    static float  Step (float zoom, int steps);

    float  GetWindowZoom    () const { return m_windowZoom; }
    void   SetWindowZoom    (float zoom);
    float  GetPaneZoom      (const std::wstring & pane) const;
    void   StepPaneZoom     (const std::wstring & pane, int steps);
    float  GetEffectiveZoom (const std::wstring & pane) const;

    //  The status bar slider's position for a size, and the size at a
    //  position.
    static float  GetSliderPosition (float zoom);
    static float  GetZoomAtSlider   (float position);

private:
    float                          m_windowZoom = kDefaultZoom;
    std::map<std::wstring, float>  m_paneZooms;
};
