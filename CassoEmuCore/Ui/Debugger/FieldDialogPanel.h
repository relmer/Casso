#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  FieldDialogPanel
//
//  The body of a small dialog of fields: a label beside each field, a row
//  each, and an error line under them.
//
////////////////////////////////////////////////////////////////////////////////

class FieldDialogPanel : public DxuiPanel
{
public:
    void  Add      (DxuiLabel & label, IDxuiControl & field);
    void  SetError (DxuiLabel & error);
    void  Layout   (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;

    static constexpr int  kRowDip   = 30;
    static constexpr int  kGapDip   = 8;
    static constexpr int  kLabelDip = 110;

private:
    std::vector<std::pair<DxuiLabel *, IDxuiControl *>>  m_rows;
    DxuiLabel                                          * m_error = nullptr;
};
