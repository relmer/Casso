#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/KrunchMap.h"
#include "CassoLink/Session/CassoLinkSessionTypes.h"

class IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMapPanel
//
//  The krunch window's content: the drive, the map of its blocks, the file
//  under the pointer, what the krunch is doing, a progress bar, and the
//  legend, top to bottom. A file's blocks that are side by side in a row are
//  drawn as one bar, so the gaps in a row fall between files, and the file
//  under the pointer is outlined.
//
////////////////////////////////////////////////////////////////////////////////

class KrunchMapPanel : public DxuiPanel
{
public:
    struct View
    {
        std::wstring                   drive;
        std::vector<KrunchMap::Block>  blocks;
        std::vector<PascalKrunchMove>  files;
        int                            percent = 0;
        std::wstring                   status;
    };

    static constexpr float  kHeadingDip     = 24.0f;
    static constexpr float  kCellGapDip     = 2.0f;
    static constexpr float  kSectionGapDip  = 14.0f;
    static constexpr float  kLineDip        = 20.0f;
    static constexpr float  kBarDip         = 14.0f;
    static constexpr float  kSwatchDip      = 12.0f;
    static constexpr float  kLegendRowDip   = 22.0f;
    static constexpr float  kFontDip        = 12.0f;
    static constexpr float  kHeadingFontDip = 14.0f;
    static constexpr float  kOutlineDip     = 2.0f;
    static constexpr int    kLegendColumns  = 3;

    static constexpr const wchar_t *  s_kpszFont = L"Segoe UI";

    //  The colors of the Windows 95 Disk Defragmenter's map, which don't
    //  change with the theme, so the map reads as that one does.
    static uint32_t      GetColor (KrunchMap::Cell cell);
    static std::wstring  GetLabel (KrunchMap::Cell cell);

    //  The line for the file under the pointer: its name, its blocks, and
    //  where it moves, from its cell.
    static std::wstring  DescribeFile (const PascalKrunchMove & file, KrunchMap::Cell cell);

    void  SetView (View view);

    void  Layout  (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

private:
    struct Grid
    {
        float  left = 0.0f;
        float  top  = 0.0f;
        float  cell = 0.0f;
        float  gap  = 0.0f;
    };

    Grid  GetGrid       () const;
    int   FindBlockAt   (POINT pointPx) const;
    void  PaintMap      (IDxuiPainter & painter, const IDxuiTheme & theme, const Grid & grid) const;
    void  PaintLegend   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, float top) const;
    bool  IsJoinedRight (int block) const;

    View           m_view;
    DxuiDpiScaler  m_scaler;
    int            m_hoverBlock = -1;
};





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMapDialog
//
//  A linked drive's krunch as it runs on the Apple, drawn from the
//  operation's plan and its progress on each tick. Stop cancels the krunch
//  after the file being moved; Close hides the window and the krunch goes on.
//
////////////////////////////////////////////////////////////////////////////////

class KrunchMapDialog : public DxuiDialogWindow
{
public:
    static constexpr int   kWidthDip  = 640;
    static constexpr int   kHeightDip = 400;
    static constexpr int   kStopId    = 1001;
    static constexpr UINT  kTickMs    = 100;

    using StopFn = std::function<void ()>;

    void  Configure (std::shared_ptr<LinkOperation> operation, const std::wstring & drive, const IDxuiTheme * theme, StopFn stop);

    //  What the krunch is doing, under the map.
    static std::wstring  DescribeStatus (const LinkOperation & operation, bool stopping);

    //  The map at the operation's progress: every file in its new place once
    //  the krunch is done.
    static KrunchMapPanel::View  BuildView (const LinkOperation & operation, const std::wstring & drive, bool stopping);

    static std::unique_ptr<KrunchMapDialog>  Open (HWND owner, const IDxuiTheme * theme, std::shared_ptr<LinkOperation> operation, const std::wstring & drive, StopFn stop);

protected:
    void  OnCreate     () override;
    void  OnDialogTick () override;

private:
    void  Refresh ();

    std::shared_ptr<LinkOperation>  m_operation;
    std::wstring                    m_drive;
    const IDxuiTheme              * m_theme      = nullptr;
    StopFn                          m_stop;
    bool                            m_stopping   = false;
    KrunchMapPanel                * m_panel      = nullptr;
    DxuiButton                    * m_stopButton = nullptr;
};
