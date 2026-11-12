#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockHexSource.h"

class BlockFieldsView;
class BlockHexView;
class BlockInspectorModel;
class BlockMapView;
struct BlockField;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel
//
//  Block mode's content. Across the top: Back and Forward, the previous and
//  next block, the block number, Highlight file's blocks, and for 2IMG and
//  Disk Copy 4.2 images the container's header. On the left the block map,
//  with what the pointer is on under it; on the right the hex and text
//  panes of the block and its fields under them. The status line has the
//  block's role, its files, a Disk Copy block's tag bytes, and the edits
//  pending.
//
//  Page Up and Page Down step a block, Alt+Left and Alt+Right go back and
//  forward, and a click on a linked field, or a Ctrl+click on a linked byte,
//  follows the link.
//
////////////////////////////////////////////////////////////////////////////////

class BlockInspectorPanel : public DxuiPanel
{
public:
    static constexpr float  kPadDip      = 8.0f;
    static constexpr float  kToolbarDip  = 28.0f;
    static constexpr float  kButtonGap   = 6.0f;
    static constexpr float  kInputDip    = 80.0f;
    static constexpr float  kLabelDip    = 44.0f;
    static constexpr float  kCountDip    = 90.0f;
    static constexpr float  kLineDip     = 20.0f;
    static constexpr float  kFontDip     = 12.0f;
    static constexpr float  kMapShare    = 0.42f;
    static constexpr float  kHexShare    = 0.60f;
    static constexpr int    kBytesPerRow = 16;

    using PressedFn = std::function<void (IDxuiControl * child)>;
    using ChangedFn = std::function<void ()>;

    BlockInspectorPanel  ();
    ~BlockInspectorPanel () override;

    void  Init              (BlockInspectorModel * model, const IDxuiTheme * theme, HWND hwnd, IDxuiTextRenderer * text);
    void  SetOnChildPressed (PressedFn fn) { m_onPressed = std::move (fn); }
    void  SetOnChanged      (ChangedFn fn) { m_onChanged = std::move (fn); }

    //  Reads the model again after a change.
    void  Refresh ();

    BlockHexView  * GetHexView () const { return m_hex; }

    //  A block number typed in decimal, or in hex after '$'.
    static bool  TryParseBlock (const std::wstring & text, uint32_t & outBlock);

    //  Steps to a block, or back or forward, and refreshes.
    void  SelectBlock (uint32_t block);
    void  Step        (int64_t delta);
    void  GoBack      ();
    void  GoForward   ();

    void  Layout  (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

private:
    static constexpr const wchar_t *  s_kpszFont = L"Segoe UI";

    bool  OnType        (uint64_t offset, bool isHexColumn, bool isHighNibble, wchar_t ch);
    bool  OnNavigateKey (const DxuiKeyEvent & ev);
    void  OnInputChange (const std::wstring & text);
    void  MarkField     (const BlockField * field);
    void  ShowBytes     ();
    void  BuildStatus   ();

    BlockInspectorModel  * m_model      = nullptr;
    DxuiDpiScaler          m_scaler;
    DxuiButton           * m_back       = nullptr;
    DxuiButton           * m_forward    = nullptr;
    DxuiButton           * m_previous   = nullptr;
    DxuiButton           * m_next       = nullptr;
    DxuiButton           * m_highlight  = nullptr;
    DxuiButton           * m_header     = nullptr;
    DxuiTextInput        * m_input      = nullptr;
    BlockMapView         * m_map        = nullptr;
    BlockHexView         * m_hex        = nullptr;
    BlockFieldsView      * m_fields     = nullptr;
    BlockHexSource         m_source;
    PressedFn              m_onPressed;
    ChangedFn              m_onChanged;
    std::wstring           m_hoverText;
    std::wstring           m_status;
    int64_t                m_shownBlock = -1;
    bool                   m_isHeader   = false;
    bool                   m_isTyping   = false;
    RECT                   m_labelRect  = {};
    RECT                   m_countRect  = {};
    RECT                   m_hoverRect  = {};
    RECT                   m_statusRect = {};
};
