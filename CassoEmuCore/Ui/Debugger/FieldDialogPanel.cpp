#include "Pch.h"

#include "Ui/Debugger/FieldDialogPanel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FieldDialogPanel::Add
//
////////////////////////////////////////////////////////////////////////////////

void FieldDialogPanel::Add (DxuiLabel & label, IDxuiControl & field)
{
    m_rows.push_back ({ &label, &field });
    Adopt (label);
    Adopt (field);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FieldDialogPanel::SetError
//
////////////////////////////////////////////////////////////////////////////////

void FieldDialogPanel::SetError (DxuiLabel & error)
{
    m_error = &error;
    Adopt (error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FieldDialogPanel::Layout
//
////////////////////////////////////////////////////////////////////////////////

void FieldDialogPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int  row   = scaler.ToPx (kRowDip);
    int  gap   = scaler.ToPx (kGapDip);
    int  split = boundsDip.left + scaler.ToPx (kLabelDip);
    int  y     = boundsDip.top;



    SetBounds (boundsDip);

    for (const auto & [label, field] : m_rows)
    {
        label->Layout (RECT { boundsDip.left, y, split, y + row }, scaler);
        field->Layout (RECT { split, y, boundsDip.right, y + row }, scaler);
        y += row + gap;
    }

    if (m_error != nullptr)
    {
        m_error->Layout (RECT { boundsDip.left, y, boundsDip.right, y + row }, scaler);
    }
}
