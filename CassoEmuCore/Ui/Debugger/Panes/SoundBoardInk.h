#pragma once

#include "Debugger/DiagnosticsSnapshot.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardInk
//
//  The pieces every card of the sound board is drawn from: the card itself
//  under its title, a string in a rectangle, a label and its value, a lamp,
//  a mute button, a scope and the glowing trace on it. Each part drawn with
//  a tip has a spot added where it was drawn.
//
////////////////////////////////////////////////////////////////////////////////

class SoundBoardInk
{
public:
    struct Palette
    {
        std::array<uint32_t, DiagnosticsSoundPsg::kChannelCount>  channels = { 0xFF3CC8C8, 0xFFE6B43C, 0xFFD070E0 };
        uint32_t                                                  voice    = 0xFF3C8CE6;
    };

    //  A part's tip, at the place the last paint drew the part, in client
    //  pixels.
    struct Spot
    {
        RECT          rect = {};
        std::wstring  tip;
    };

    //  A trace's glow, its bright line, or both.
    enum class TraceLayer
    {
        Glow,
        Core,
        Both,
    };

    //  One paint's targets and theme, the colors of the scopes' screens, the
    //  cards and their edges, the spots added for the parts, the scopes'
    //  sample rate, and the mute under the pointer and the one the keyboard
    //  focused, -1 for none.
    struct Painting
    {
        IDxuiPainter       & painter;
        IDxuiTextRenderer  & text;
        const IDxuiTheme   & theme;
        uint32_t             screen;
        uint32_t             card;
        uint32_t             edge;
        std::vector<Spot>  & spots;
        uint32_t             sampleRate;
        int                  hoverMute;
        int                  focusedMute;
    };

    static constexpr int  kGapDip      = 8;
    static constexpr int  kPadDip      = 12;
    static constexpr int  kTitleDip    = 28;
    static constexpr int  kRegisterDip = 20;
    static constexpr int  kLabelDip    = 44;

    explicit SoundBoardInk (const DxuiDpiScaler & scaler) : m_scaler (scaler) {}

    static uint32_t  Mix       (uint32_t from, uint32_t to, float amount);
    static uint32_t  WithAlpha (uint32_t argb, float alpha);
    static void      AddSpot   (const Painting & p, const RECT & rect, std::wstring tip);

    void  PaintString     (const Painting & p, const std::wstring & value, const RECT & rect, uint32_t color, const DxuiFontHandle & font, DxuiTextHAlign align = DxuiTextHAlign::Left) const;
    void  PaintCard       (const Painting & p, const RECT & card, const std::wstring & title, const std::wstring & detail) const;
    void  PaintLamp       (const Painting & p, float x, float y, const wchar_t * label, bool isOn, uint32_t color, float width) const;
    void  PaintValue      (const Painting & p, const RECT & rect, const std::wstring & label, const std::wstring & value, int labelDip = kLabelDip) const;
    void  PaintMuteButton (const Painting & p, const RECT & rect, int channel, bool isMuted, uint32_t color) const;
    void  PaintTrace      (const Painting & p, const std::vector<std::pair<float, float>> & points, uint32_t color, TraceLayer layer = TraceLayer::Both) const;
    void  PaintScope      (const Painting & p, const std::vector<float> & scope, const RECT & rect, uint32_t color, double hz, bool isBipolar) const;

private:
    const DxuiDpiScaler  & m_scaler;
};
