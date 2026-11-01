#pragma once

#include "Pch.h"

#include "Debugger/DiagnosticsSnapshot.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView
//
//  A sound card's chips as instruments. The speech chip, on a card with one,
//  has a card across the top: the phoneme playing, its waveform, its formants
//  and a strip of the phonemes before it. Each half of the card has a card
//  below, in as many columns as fit: three channels, each with a mute, its
//  note, its sources, a level meter and a scope; the envelope's picture; and
//  the 6522's registers and lamps.
//
//  The view paints what the payload has and changes nothing but its scroll.
//  A click on a mute calls the host's function, and the payload that follows
//  has the result.
//
////////////////////////////////////////////////////////////////////////////////

class SoundBoardView : public IDxuiControl
{
public:
    struct Palette
    {
        std::array<uint32_t, DiagnosticsSoundPsg::kChannelCount>  channels = { 0xFF3CC8C8, 0xFFE6B43C, 0xFFD070E0 };
        uint32_t                                                  voice    = 0xFF3C8CE6;
    };

    using MuteFn = std::function<void (int channel, bool isMuted)>;

    //  The channel the speech chip's mute has.
    static constexpr int  kVoiceMuteChannel = 6;

    void  SetBoard     (const DiagnosticsSoundBoard & board);
    void  SetPalette   (const Palette & palette)  { m_palette = palette; }
    void  SetOnMute    (MuteFn onMute)            { m_onMute = std::move (onMute); }

    const DiagnosticsSoundBoard &  GetBoard () const { return m_board; }

    //  Where a mute is in the view, in client pixels, for its channel; an empty
    //  rectangle for one the board does not have.
    RECT  GetMuteRect   (int channel) const;

    //  The raw values behind the part at a point.
    bool  TryGetTipAt   (POINT clientPx, std::wstring & outTip) const;

    int   GetContentHeight () const { return m_contentHeight; }
    int   GetScrollTop     () const { return m_scrollTop; }

    void                Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    void                Tick              (int64_t nowMs) override;
    bool                IsOverScrollbar   (POINT clientPx) const override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Mockingboard"; }

private:
    struct Region
    {
        RECT          rect        = {};      // in the content's coordinates
        std::wstring  tip;
        int           muteChannel = -1;
    };

    struct Painting
    {
        IDxuiPainter       & painter;
        IDxuiTextRenderer  & text;
        const IDxuiTheme   & theme;
        uint32_t             screen;
        uint32_t             card;
        uint32_t             edge;
    };

    static constexpr int  kScrollbarDip    = 12;
    static constexpr int  kGapDip          = 8;
    static constexpr int  kPadDip          = 12;
    static constexpr int  kTitleDip        = 28;
    static constexpr int  kChannelDip      = 58;
    static constexpr int  kChannelGapDip   = 6;
    static constexpr int  kEnvelopeDip     = 74;
    static constexpr int  kRegisterDip     = 20;
    static constexpr int  kRegisterRows    = 5;
    static constexpr int  kLeastCardDip    = 440;
    static constexpr int  kPhonemeDip      = 128;
    static constexpr int  kStripDip        = 30;
    static constexpr int  kValueRows       = 2;
    static constexpr int  kLabelDip        = 44;
    static constexpr int  kVoiceLabelDip   = 78;

    void  Reflow            ();
    void  BuildHalfRegions  (const DiagnosticsSoundHalf & half, const RECT & card);
    void  BuildVoiceRegions (const DiagnosticsSoundVoice & voice, const RECT & card);
    void  AddRegion         (const RECT & rect, std::wstring tip, int muteChannel = -1);
    void  SyncScrollbar     ();
    void  ScrollTo          (int top);

    RECT  GetChannelRect    (const RECT & card, int channel) const;
    RECT  GetMuteOfChannel  (const RECT & row) const;
    RECT  GetMuteOfVoice    (const RECT & card) const;
    RECT  GetEnvelopeRect   (const RECT & card) const;
    int   GetVoiceHeight    () const;
    int   GetHalfHeight     () const;
    int   GetClientWidth    () const;
    RECT  ToClient          (const RECT & content) const;

    void  PaintCard         (const Painting & p, const RECT & card, const std::wstring & title, const std::wstring & detail) const;
    void  PaintHalf         (const Painting & p, const DiagnosticsSoundHalf & half, const RECT & card) const;
    void  PaintChannel      (const Painting & p, const DiagnosticsSoundChannel & channel, const RECT & row, int index, int muteChannel) const;
    void  PaintEnvelope     (const Painting & p, const DiagnosticsSoundPsg & psg, const RECT & rect) const;
    void  PaintVia          (const Painting & p, const DiagnosticsSoundHalf & half, const RECT & rect) const;
    void  PaintVoice        (const Painting & p, const DiagnosticsSoundVoice & voice, const RECT & card) const;
    void  PaintStrip        (const Painting & p, const DiagnosticsSoundVoice & voice, const RECT & rect) const;
    void  PaintFormants     (const Painting & p, const DiagnosticsSoundVoice & voice, const RECT & rect) const;
    void  PaintScope        (const Painting & p, const std::vector<float> & scope, const RECT & rect, uint32_t color, double hz, bool isBipolar) const;
    void  PaintTrace        (const Painting & p, const std::vector<std::pair<float, float>> & points, uint32_t color) const;
    void  PaintLamp         (const Painting & p, float x, float y, const wchar_t * label, bool isOn, uint32_t color, float width) const;
    void  PaintValue        (const Painting & p, const RECT & rect, const std::wstring & label, const std::wstring & value, int labelDip = kLabelDip) const;
    void  PaintString       (const Painting & p, const std::wstring & value, const RECT & rect, uint32_t color, const DxuiFontHandle & font, DxuiTextHAlign align = DxuiTextHAlign::Left) const;

    static uint32_t  Mix       (uint32_t from, uint32_t to, float amount);
    static uint32_t  WithAlpha (uint32_t argb, float alpha);

    DiagnosticsSoundBoard     m_board;
    Palette                   m_palette;
    MuteFn                    m_onMute;
    DxuiDpiScaler             m_scaler;
    DxuiScrollbar             m_scrollbar;
    std::vector<RECT>         m_halfCards;
    RECT                      m_voiceCard     = {};
    std::vector<Region>       m_regions;
    std::array<float, 6>      m_peaks         = {};
    int                       m_contentHeight = 0;
    int                       m_scrollTop     = 0;
    bool                      m_hasScrollbar  = false;
};
