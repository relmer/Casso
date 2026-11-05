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

    //  What the part at a point means and the raw values behind it.
    bool  TryGetTipAt   (POINT clientPx, std::wstring & outTip) const;

    //  Whether a point is on the strip of recent phonemes or its labels.
    bool  IsOverPhonemeStrip (POINT clientPx) const;

    //  Where the envelope is along its picture's four ramps, from 0 to 1.
    static float  GetEnvelopeMarkX (const DiagnosticsSoundPsg & psg);

    int   GetContentHeight () const { return m_contentHeight; }
    int   GetScrollTop     () const { return m_scrollTop; }

    //  The phoneme strip scrolled back through the history by `count`
    //  phonemes, forward for a negative one, from the newest to the oldest
    //  the speech chip has; and how far back it is.
    void    ScrollPhonemes   (int count);
    size_t  GetPhonemesBack  () const { return m_phonemesBack; }

    //  How many columns the speech chip's values are in: four where the
    //  mode's name fits in the first, two or one where the pane is narrower.
    int     GetValueColumns  () const;

    //  A notch of the wheel over the strip.
    static constexpr int  kPhonemesPerNotch = 4;

    void                Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    bool                OnKey             (const DxuiKeyEvent & ev) override;
    void                OnFocusChanged    (bool focused) override;
    void                OnFocusEntered    (bool forward) override;
    void                Tick              (int64_t nowMs) override;
    bool                IsOverScrollbar   (POINT clientPx) const override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override;

    //  The channel of the mute the keyboard focused, or -1 while the view
    //  doesn't have the focus.
    int                 GetFocusedMute    () const;

private:
    struct Region
    {
        RECT          rect        = {};      // in the content's coordinates
        std::wstring  tip;
        int           muteChannel = -1;
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

    struct Painting
    {
        IDxuiPainter       & painter;
        IDxuiTextRenderer  & text;
        const IDxuiTheme   & theme;
        uint32_t             screen;
        uint32_t             card;
        uint32_t             edge;
        std::vector<Spot>  & spots;
    };

    static constexpr int  kScrollbarDip    = 12;
    static constexpr int  kGapDip          = 8;
    static constexpr int  kPadDip          = 12;
    static constexpr int  kTitleDip        = 28;
    static constexpr int  kChannelDip      = 58;
    static constexpr int  kChannelGapDip   = 6;
    static constexpr int  kEnvelopeDip     = 74;
    static constexpr int  kRegisterDip     = 20;
    static constexpr int  kRegisterRows    = 7;
    static constexpr int  kLeastCardDip    = 440;
    static constexpr int  kPhonemeDip      = 128;
    static constexpr int  kStripDip        = 30;
    static constexpr int  kStripHeadDip    = 18;
    static constexpr int  kHistoryDip      = 30;
    static constexpr int  kValueCount      = 8;
    static constexpr int  kValueFirstDip   = 250;     // the first column's least width, for the mode's name
    static constexpr int  kValueCellDip    = 160;
    static constexpr int  kLabelDip        = 44;
    static constexpr int  kVoiceLabelDip   = 78;
    static constexpr int  kWordBreakDip    = 6;

    void  Reflow            ();
    void  BuildHalfRegions  (const DiagnosticsSoundHalf & half, const RECT & card);
    void  BuildVoiceRegions (const DiagnosticsSoundVoice & voice, const RECT & card);
    void  AddRegion         (const RECT & rect, std::wstring tip, int muteChannel = -1);
    void  SyncScrollbar     ();
    void  ScrollTo          (int top);

    int               FindMuteAt        (POINT clientPx) const;
    std::vector<int>  GetMuteChannels   () const;
    bool              IsChannelMuted    (int channel) const;
    void              ToggleMute        (int channel);
    void              RevealFocusedMute ();

    RECT  GetChannelRect    (const RECT & card, int channel) const;
    RECT  GetMuteOfChannel  (const RECT & row) const;
    RECT  GetMuteOfVoice    (const RECT & card) const;
    RECT  GetEnvelopeRect   (const RECT & card) const;
    int   GetVoiceHeight    () const;
    RECT  GetValueCell      (const RECT & card, int index) const;
    int   GetHalfHeight     () const;
    int   GetClientWidth    () const;
    RECT  ToClient          (const RECT & content) const;

    void  PaintCard         (const Painting & p, const RECT & card, const std::wstring & title, const std::wstring & detail) const;
    void  PaintHalf         (const Painting & p, const DiagnosticsSoundHalf & half, const RECT & card) const;
    void  PaintChannel      (const Painting & p, const DiagnosticsSoundPsg & psg, const RECT & row, int index, int muteChannel) const;
    void  PaintEnvelope     (const Painting & p, const DiagnosticsSoundPsg & psg, const RECT & rect) const;
    void  PaintVia          (const Painting & p, const DiagnosticsSoundHalf & half, const RECT & rect) const;
    void  PaintRegisters    (const Painting & p, const DiagnosticsSoundPsg & psg, const RECT & rect) const;
    static void  AddSpot    (const Painting & p, const RECT & rect, std::wstring tip);
    void  PaintVoice        (const Painting & p, const DiagnosticsSoundVoice & voice, const RECT & card) const;
    void  PaintStrip        (const Painting & p, std::span<const DiagnosticsSoundVoice::Phoneme> recent, bool isSounding, const RECT & rect) const;
    float GetWidestPhonemeLabel (const Painting & p, const DxuiFontHandle & font) const;
    void  PaintWordBreak    (const Painting & p, float x, float top, float bottom) const;
    size_t  PaintHistory    (const Painting & p, std::span<const DiagnosticsSoundVoice::Phoneme> recent, bool isSounding, const RECT & rect) const;
    RECT  GetStripArea      (const RECT & card) const;
    void  PaintFormants     (const Painting & p, const DiagnosticsSoundVoice & voice, const RECT & rect) const;
    void  PaintScope        (const Painting & p, const std::vector<float> & scope, const RECT & rect, uint32_t color, double hz, bool isBipolar) const;
    void  PaintTrace        (const Painting & p, const std::vector<std::pair<float, float>> & points, uint32_t color, TraceLayer layer = TraceLayer::Both) const;
    void  PaintLamp         (const Painting & p, float x, float y, const wchar_t * label, bool isOn, uint32_t color, float width) const;
    void  PaintMuteButton   (const Painting & p, const RECT & rect, int channel, bool isMuted, uint32_t color) const;
    void  PaintValue        (const Painting & p, const RECT & rect, const std::wstring & label, const std::wstring & value, int labelDip = kLabelDip) const;
    void  PaintString       (const Painting & p, const std::wstring & value, const RECT & rect, uint32_t color, const DxuiFontHandle & font, DxuiTextHAlign align = DxuiTextHAlign::Left) const;

    static uint32_t  Mix       (uint32_t from, uint32_t to, float amount);
    static uint32_t  WithAlpha (uint32_t argb, float alpha);

    DiagnosticsSoundBoard  m_board;
    Palette                m_palette;
    MuteFn                 m_onMute;
    DxuiDpiScaler          m_scaler;
    DxuiScrollbar          m_scrollbar;
    std::vector<RECT>      m_halfCards;
    RECT                   m_voiceCard         = {};
    std::vector<Region>    m_regions;
    std::vector<Spot>      m_spots;
    std::array<float, 6>   m_peaks             = {};
    int                    m_contentHeight     = 0;
    int                    m_scrollTop         = 0;
    size_t                 m_phonemesBack      = 0;   // how many of the newest phonemes the strip is scrolled back past
    bool                   m_hasScrollbar      = false;
    bool                   m_isFocused         = false;
    int                    m_focusMute         = 0;   // the mute the keyboard focused, by its place in GetMuteChannels
    int                    m_hoverMute         = -1;   // the channel of the mute under the pointer
    mutable float          m_widestLabelPx     = 0.0f;   // the widest phoneme symbol, at
    mutable float          m_widestLabelSizePx = 0.0f;   // this font size
};
