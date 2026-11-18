#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  FieldDialogPanel
//
//  The body of a small dialog of fields: a label beside each field, a row
//  each, and an error line under them. The labels' column is as wide as
//  the longest label.
//
////////////////////////////////////////////////////////////////////////////////

class FieldDialogPanel : public DxuiPanel
{
public:
    void  Add      (DxuiLabel & label, IDxuiControl & field);
    void  SetError (DxuiLabel & error);
    void  Layout   (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;

    //  A wrapped line of text above the fields.
    void  SetDescription (DxuiLabel & description);

    static constexpr int  kRowDip   = 30;
    static constexpr int  kGapDip   = 8;
    static constexpr int  kLabelDip = 110;

    //  The label column's width: kLabelDip, or wider so the longest label
    //  fits on one line.
    int  GetLabelDip () const;

private:
    std::vector<std::pair<DxuiLabel *, IDxuiControl *>>  m_rows;
    DxuiLabel                                          * m_error       = nullptr;
    DxuiLabel                                          * m_description = nullptr;
};
