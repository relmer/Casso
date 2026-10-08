#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/CatalogModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerProperties
//
//  What File Explorer's General tab shows of a file, for an entry inside a
//  disk image: its type, where it is, its size and the room it takes, when it
//  was written and whether it is locked, and nothing the file system does not
//  record. Kept apart from the dialog so it is testable without a window.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerProperties
{
public:
    struct Row
    {
        std::wstring  label;
        std::wstring  value;
        bool          groupStart = false;   // a divider goes above it, as between Explorer's groups
    };

    static std::vector<Row>  DescribeEntry (const FileEntry & entry, VolumeKind kind, const std::wstring & location);

    //  Explorer's size: the rounded figure, then the exact count of bytes.
    static std::wstring      FormatBytes   (uint64_t bytes);

    static constexpr const wchar_t *  s_kLocationLabel = L"Location:";
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerPropertiesPanel
//
//  The entry's name, then each row's label beside its value, with a divider
//  between groups as Explorer's General tab draws them.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerPropertiesPanel : public DxuiPanel
{
public:
    struct Line
    {
        DxuiLabel  * label      = nullptr;
        DxuiLabel  * value      = nullptr;
        bool         groupStart = false;
    };

    void  Init   (DxuiLabel * name, std::vector<Line> lines);
    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

    //  The height the rows need, in dips, for the window's size.
    int   GetRequiredHeightDip () const;

    static constexpr int  kNameHeightDip = 32;
    static constexpr int  kRowHeightDip  = 24;
    static constexpr int  kGroupGapDip   = 17;    // the divider sits in its middle
    static constexpr int  kLabelWidthDip = 110;

private:
    DxuiLabel          * m_name = nullptr;
    std::vector<Line>    m_lines;
    std::vector<int>     m_dividerY;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerPropertiesDialog
//
//  The rows for an entry, in a window of its own over CE, with OK to close it;
//  nothing in it changes the entry.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerPropertiesDialog : public DxuiDialogWindow
{
public:
    static void  Show (HWND owner, const IDxuiTheme * theme, const std::wstring & name, const std::vector<CassoExplorerProperties::Row> & rows);

protected:
    void  OnCreate () override;

private:
    //  The caption, the margins and the button row, around the rows.
    static constexpr int  s_kChromeDip = 130;

    const IDxuiTheme                           * m_theme = nullptr;
    std::vector<CassoExplorerProperties::Row>    m_rows;
    DxuiLabel                                    m_name;
    std::vector<std::unique_ptr<DxuiLabel>>      m_labels;
    CassoExplorerPropertiesPanel               * m_body  = nullptr;
};
