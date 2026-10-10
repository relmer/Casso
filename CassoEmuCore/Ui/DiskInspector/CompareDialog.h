#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/ComparisonText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CompareDialog
//
//  "Compare with...": a column of sources for A and one for B (FR-117), each
//  drive's disk now, as inserted and its file, then an image file, which OK
//  asks for. A starts on the window's disk and B on an image file.
//
////////////////////////////////////////////////////////////////////////////////

class CompareDialog : public DxuiDialogWindow
{
public:
    void  Configure (const IDxuiTheme * theme, int driveCount, const ComparisonSource & a, const ComparisonSource & b);

    bool                      IsChosen  () const { return m_isChosen; }
    const ComparisonSource &  GetSource (int side) const { return m_sources[side]; }

    static constexpr SIZE  kSizeDip = { 520, 360 };

    static vector<ComparisonSource>  BuildSources (int driveCount);

protected:
    void  OnCreate () override;

private:
    void  OnOkClicked ();
    bool  PickImage   (ComparisonSource & inOut);

    const IDxuiTheme *                m_theme      = nullptr;   // non-owning
    vector<ComparisonSource>          m_choices;
    std::array<ComparisonSource, 2>   m_sources;
    bool                              m_isChosen   = false;

    DxuiRadioGroup                    m_sideA;
    DxuiRadioGroup                    m_sideB;
    DxuiLabel                         m_titleA;
    DxuiLabel                         m_titleB;
    DxuiLabel                         m_error;
};
