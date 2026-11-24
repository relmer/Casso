#pragma once

#include "Pch.h"
#include "Core/DxuiPanel.h"
#include "DxuiSystemButton.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTitleChip
//
//  A rounded label drawn after a caption's title, such as "Read as CP/M",
//  with a tooltip over it. An Info chip is drawn in the theme's accent, and
//  a Plain chip in the caption's text color.
//
////////////////////////////////////////////////////////////////////////////////

enum class DxuiTitleChipKind
{
    Info,
    Plain,
};

struct DxuiTitleChip
{
    std::wstring       text;
    std::wstring       tip;
    DxuiTitleChipKind  kind = DxuiTitleChipKind::Info;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCaptionBar
//
//  Container panel covering the custom-chrome title-bar strip at the
//  top of a `DxuiHwndSource`. Inherits all `DxuiPanel` semantics
//  (children, layout, paint fan-out) and adds caption-specific hit-
//  test defaults: any point not consumed by a child resolves to
//  `DxuiHitTestKind::Caption`, giving Win32 free drag / double-click-
//  to-maximize on blank areas of the title bar.
//
//  Two usage modes:
//
//    1. Plain container: the caller Adopts its own title label + Close
//       DxuiSystemButton and positions them via a layout. The bar paints
//       nothing of its own (flat panel).
//
//    2. Host-owned caption (DxuiHwndSource, the SetWindowText model):
//       ConfigureButtons() builds the owned min/max/close
//       DxuiSystemButton children and turns on self-rendering of the
//       gradient + app icon + title text. The consuming app never
//       touches this object -- it just sets the window title / icon on
//       the host, which forwards here. This is what replaces a bespoke
//       per-window title bar.
//
//  All public methods are called on the UI thread (FR-083).
//
////////////////////////////////////////////////////////////////////////////////



class DxuiCaptionBar : public DxuiPanel
{
public:
    enum class Buttons
    {
        None,
        CloseOnly,
        MinMaxClose,
        MaxClose,
    };

    DxuiCaptionBar  ();
    ~DxuiCaptionBar () override = default;

    // Caption-strip height. The DIP value is the design constant; the
    // pixel helpers scale it. HeightPxForDpi lets a consumer reserve the
    // band before a host (and thus a live scaler) exists.
    static constexpr int  kCaptionHeightDip = 32;
    static int            GetHeightPxForDpi    (UINT dpi);

    // Host-owned caption configuration (mode 2)

    void  ConfigureButtons (Buttons buttons);
    void  SetSystemHwnd    (HWND hwnd);
    void  SetTitle         (const std::wstring & title, size_t markAt = std::wstring::npos);
    void  SetAppIcon       (std::vector<uint32_t> bgraPremul, int widthPx, int heightPx);
    void  SetMaximized     (bool maximized);

    // Chips drawn after the title; none by default. In a narrow caption the
    // title is cut first, then chips are left out from the last.
    void  SetTitleChips    (std::vector<DxuiTitleChip> chips);

    // A consumer's control shown in a caption-button column to the left of
    // the system buttons (non-owning; null removes it). Laid out in DIPs like
    // the system buttons; hidden, it takes no room from the title.
    void  SetAccessory     (IDxuiControl * accessory);

    // The accessory's column width; one caption-button column by default.
    void  SetAccessoryWidthDip (int widthDip) { m_accessoryWidthDip = widthDip; }
    int   GetReservedWidthDip  () const;

    // Natural caption-strip height in physical pixels for the given DPI;
    // the host reserves this band at the top and lays content out below.
    int   GetPreferredHeightPx  (const DxuiDpiScaler & scaler) const;
    int   GetPreferredHeightDip () const;

    // IDxuiControl / DxuiPanel overrides

    void  Layout (const RECT          & boundsDip,
                  const DxuiDpiScaler & scaler) override;
    void  Paint  (IDxuiPainter        & painter,
                  IDxuiTextRenderer   & text,
                  const IDxuiTheme    & theme) override;

    DxuiHitTestKind     ClassifyHit       (POINT clientDip) const override;

    // Over a chip, its tip; over the title the last paint cut short, the
    // full title; null for a title that fits and anywhere else. The anchor
    // is the chip's or the title's rect, in client pixels.
    const wchar_t       * GetTooltipAt      (POINT clientPx, RECT & anchorPx) const;

    // The chip under a point in client pixels, as the last paint placed the
    // chips, or -1. ClassifyHit returns Caption over a chip, so the window
    // can be dragged by it.
    int                   GetTitleChipAt    (POINT clientPx) const;
    DxuiAccessibleRole    GetAccessibleRole () const          override { return DxuiAccessibleRole::CaptionBar; }

private:
    static constexpr int      kButtonWidthDip   = 46;
    static constexpr float    kChipPadDip       = 8.0f;
    static constexpr float    kChipGapDip       = 6.0f;
    static constexpr float    kChipHeightDip    = 20.0f;
    static constexpr float    kChipTextDip      = 12.0f;
    static constexpr wchar_t  kTitleFamily[]    = L"Segoe UI";

    float                PaintTitle   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const RECT & areaPx, float fontPx);
    float                FitChips     (IDxuiTextRenderer & text, float roomPx, std::vector<float> & outWidthsPx) const;
    void                 PaintChips   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, float leftPx, float topPx, float heightPx, float titleFontPx, const std::vector<float> & widthsPx);
    static float         GetChipTopPx (IDxuiTextRenderer & text, float topPx, float heightPx, float titleFontPx, float chipPx, float chipFontPx);
    static float         MeasureTitle (IDxuiTextRenderer & text, const std::wstring & value, float fontPx);
    static std::wstring  FitTitle     (IDxuiTextRenderer & text, const std::wstring & value, float fontPx, float roomPx);

    bool                                 m_renderCaption     = false;
    Buttons                              m_buttons           = Buttons::None;
    std::wstring                         m_title;
    size_t                               m_markAt            = std::wstring::npos;
    bool                                 m_titleCut          = false;
    RECT                                 m_titleRectPx       = {};
    std::vector<DxuiTitleChip>           m_chips;
    std::vector<RECT>                    m_chipRectsPx;
    std::vector<uint32_t>                m_iconPixels;
    int                                  m_iconW             = 0;
    int                                  m_iconH             = 0;
    bool                                 m_maximized         = false;
    DxuiDpiScaler                        m_scaler;
    std::unique_ptr<DxuiSystemButton>    m_minBtn;
    std::unique_ptr<DxuiSystemButton>    m_maxBtn;
    std::unique_ptr<DxuiSystemButton>    m_closeBtn;
    IDxuiControl                       * m_accessory         = nullptr;
    int                                  m_accessoryWidthDip = kButtonWidthDip;
};
