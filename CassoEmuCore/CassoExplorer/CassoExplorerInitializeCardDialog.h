#pragma once

#include "Pch.h"

#include "CassoExplorer/CardCompatibilityGrid.h"
#include "CassoExplorer/Model/CardLayoutModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerInitializeCardPanel
//
//  The question, the adapter and layout dropdowns with their labels, the
//  compatibility grid, the card's partitions and the warning, stacked.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerInitializeCardPanel : public DxuiPanel, public IDxuiMeasuredContent
{
public:
    struct Parts
    {
        DxuiLabel              * question     = nullptr;
        DxuiLabel              * adapterLabel = nullptr;
        DxuiComboBox           * adapter      = nullptr;
        DxuiLabel              * layoutLabel  = nullptr;
        DxuiComboBox           * layout       = nullptr;
        CardCompatibilityGrid  * grid         = nullptr;
        DxuiLabel              * contents     = nullptr;
        DxuiLabel              * warning      = nullptr;
    };

    void  Init (const Parts & parts);

    void  Layout             (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool  OnMouse            (const DxuiMouseEvent & ev) override;
    bool  TryMeasureHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, int widthPx, int & heightPx) override;

private:
    static constexpr int  kSectionGapDip = 12;

    static int  GetLabelHeightPx (const DxuiLabel & label, int widthPx, const DxuiDpiScaler & scaler);

    int  Arrange (const RECT & bounds, const DxuiDpiScaler & scaler, bool place);

    Parts          m_parts;
    DxuiDpiScaler  m_scaler;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerInitializeCardDialog
//
//  The confirmation of an erase. The adapter chosen selects the layout
//  recommended and the grid's rows; FAT32 is offered for a CFFA3000 only.
//  Cancel is the default and has the focus, so Enter and Escape cancel; the
//  erase runs only from its button, which is drawn in the theme's error
//  color, when it is clicked or focused with Tab.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerInitializeCardDialog : public DxuiDialogWindow
{
public:
    explicit CassoExplorerInitializeCardDialog (const CardLayoutModel & model) : m_model (model), m_grid (model) {}

    //  False when canceled or the dialog could not open; otherwise the layout
    //  code chosen. `ioAdapter` is the adapter selected first, and the adapter
    //  chosen after, whether or not the erase ran.
    static bool  Confirm (HWND                         owner,
                          const IDxuiTheme           * theme,
                          const std::wstring         & question,
                          const CardLayoutModel      & model,
                          const std::wstring         & contents,
                          CardLayoutModel::Adapter   & ioAdapter,
                          uint64_t                   & outLayout);

protected:
    void  OnCreate () override;

private:
    void  ChooseAdapter (CardLayoutModel::Adapter adapter);
    void  ChooseLayout  (int index);

    const CardLayoutModel               & m_model;
    std::wstring                          m_questionText;
    std::wstring                          m_contentsText;
    CardLayoutModel::Adapter              m_adapter   = CardLayoutModel::Adapter::Unsure;
    std::vector<int>                      m_positions;
    int                                   m_position  = 1;
    bool                                  m_confirmed = false;
    uint64_t                              m_layout    = 0;
    DxuiLabel                             m_question;
    DxuiLabel                             m_adapterLabel;
    DxuiComboBox                          m_adapterBox;
    DxuiLabel                             m_layoutLabel;
    DxuiComboBox                          m_layoutBox;
    CardCompatibilityGrid                 m_grid;
    DxuiLabel                             m_contents;
    DxuiLabel                             m_warning;
    CassoExplorerInitializeCardPanel    * m_body      = nullptr;
};
