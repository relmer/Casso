#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  StepFiltersPanel
//
//  A heading, the filters on an editable grid, and a line under the grid
//  with the reason an entry was turned down. The grid takes coordinates
//  relative to itself, so the panel moves each mouse event into them.
//
////////////////////////////////////////////////////////////////////////////////

class StepFiltersPanel : public DxuiPanel
{
public:
    using PressFn = std::function<void (const DxuiMouseEvent & ev, int row)>;

    void     Init              (DxuiLabel & heading, DxuiEditableGrid & grid, DxuiLabel & error);
    void     SetOnGridPressed  (PressFn fn) { m_pressed = std::move (fn); }
    void     Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool     OnMouse           (const DxuiMouseEvent & ev) override;
    LPCWSTR  GetCursorForPoint (POINT clientPx) const override;

    static constexpr int  kRowDip = 30;
    static constexpr int  kGapDip = 6;

private:
    DxuiLabel         * m_heading = nullptr;
    DxuiEditableGrid  * m_grid    = nullptr;
    DxuiLabel         * m_error   = nullptr;
    PressFn             m_pressed;
};





////////////////////////////////////////////////////////////////////////////////
//
//  StepFiltersDialog
//
//  Debug > Step filters... (FR-184): the routines a step goes over rather
//  than into, each as SKIP takes it, a symbol's name, an address or a range.
//  One is added on the grid's last row and deleted with Delete or its row's
//  menu. A name must be a symbol, as the check function returns.
//
////////////////////////////////////////////////////////////////////////////////

class StepFiltersDialog : public DxuiDialogWindow
{
public:
    using Filters = std::vector<std::wstring>;
    using CheckFn = std::function<bool (const std::wstring & text, std::wstring & error)>;

    //  An address in hex, or two apart at a period, as SKIP takes a range;
    //  false for anything else.
    static bool  TryParseRange (const std::wstring & text, Word & first, Word & last);

    //  Runs the dialog modally over `owner`; the filters when OK.
    static std::optional<Filters>  RunModal (HWND owner, const IDxuiTheme * theme, const Filters & filters, CheckFn check);

protected:
    void  OnCreate() override;

private:
    void  ConfigureGrid ();
    void  ApplyRows     ();
    bool  TryTake       (const std::wstring & text, std::wstring & taken, std::wstring & error) const;
    void  ShowRowMenu   (const DxuiMouseEvent & ev, int row);

    const IDxuiTheme              * m_theme   = nullptr;
    Filters                         m_filters;
    CheckFn                         m_check;
    std::optional<Filters>          m_result;
    DxuiLabel                       m_heading;
    DxuiEditableGrid                m_grid;
    DxuiLabel                       m_error;
    std::shared_ptr<DxuiCommand>    m_deleteCommand;
    StepFiltersPanel              * m_body    = nullptr;
};
