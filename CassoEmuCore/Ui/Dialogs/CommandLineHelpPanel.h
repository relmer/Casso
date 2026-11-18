#pragma once

#include "Pch.h"

#include "Ui/Dialogs/CommandLineHelpLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelpPanel
//
//  The command-line help dialog's content: the Casso logo beside the name and
//  version, the error for a command line Casso cannot parse, the usage line, and
//  the options as a two-column table of switch and description.
//
////////////////////////////////////////////////////////////////////////////////

class CommandLineHelpPanel : public DxuiPanel, public IDxuiMeasuredContent
{
public:
    struct Row
    {
        std::wstring  option;
        std::wstring  description;
    };

    struct Content
    {
        std::wstring           title;
        std::wstring           subtitle;
        std::wstring           version;
        std::wstring           refusal;
        std::wstring           usage;
        std::vector<Row>       rows;
        std::vector<uint32_t>  logoPixels;   // premultiplied BGRA
        int                    logoWidth  = 0;
        int                    logoHeight = 0;
    };

    void  Init (const Content & content);

    void  Layout             (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void  Paint              (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  TryMeasureHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, int widthPx, int & heightPx) override;

    const CommandLineHelpTable &  GetTable () const { return m_table; }

private:
    static constexpr int    kLogoDip         = 72;
    static constexpr int    kLogoGapDip      = 18;
    static constexpr int    kTitleLineDip    = 36;
    static constexpr int    kSubtitleLineDip = 22;
    static constexpr int    kVersionLineDip  = 20;
    static constexpr int    kSectionGapDip   = 20;
    static constexpr int    kHeadingLineDip  = 22;
    static constexpr int    kHeadingGapDip   = 6;
    static constexpr int    kLineDip         = 20;
    static constexpr int    kInsetDip        = 10;
    static constexpr int    kColumnGapDip    = 24;
    static constexpr int    kRowPadDip       = 6;
    static constexpr int    kCharEstimateDip = 7;
    static constexpr float  kTitleFontDip    = 26.0f;
    static constexpr float  kSubtitleFontDip = 14.0f;
    static constexpr float  kVersionFontDip  = 12.0f;
    static constexpr float  kHeadingFontDip  = 14.0f;
    static constexpr float  kBodyFontDip     = 13.0f;

    struct RowLabels
    {
        DxuiLabel  * option      = nullptr;
        DxuiLabel  * description = nullptr;
    };

    DxuiLabel *  AddLabel          (const std::wstring & text, float fontDip, DxuiFontWeight weight, DxuiTextRole role);
    HRESULT      MeasureTable      (IDxuiTextRenderer & text, int widthPx);
    void         EstimateTable     (int widthPx);
    bool         IsMeasuredAt      (int widthPx) const;
    int          GetTableWidthPx   (int widthPx) const;
    int          Arrange           (const RECT & bounds, bool place);
    int          ArrangeHeader     (const RECT & bounds, bool place);
    void         PlaceRows         (int left, int right, int top);
    void         PaintDecorations  (IDxuiTextRenderer & text, const IDxuiTheme & theme);

    Content                 m_content;
    DxuiLabel             * m_title          = nullptr;
    DxuiLabel             * m_subtitle       = nullptr;
    DxuiLabel             * m_version        = nullptr;
    DxuiLabel             * m_refusal        = nullptr;
    DxuiLabel             * m_usageHeading   = nullptr;
    DxuiLabel             * m_usage          = nullptr;
    DxuiLabel             * m_optionsHeading = nullptr;
    std::vector<RowLabels>  m_rows;
    CommandLineHelpTable    m_table;
    int                     m_tableWidthPx   = 0;
    bool                    m_measured       = false;
    int                     m_refusalPx      = 0;
    int                     m_dividerYPx     = 0;
    RECT                    m_boundsPx       = {};
    RECT                    m_logoRectPx     = {};
    std::vector<RECT>       m_stripesPx;
    DxuiDpiScaler           m_scaler;
};
