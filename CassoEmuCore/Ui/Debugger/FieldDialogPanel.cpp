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
//  FieldDialogPanel::SetDescription
//
////////////////////////////////////////////////////////////////////////////////

void FieldDialogPanel::SetDescription (DxuiLabel & description)
{
    m_description = &description;
    Adopt (description);
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
//  FieldDialogPanel::GetLabelDip
//
//  A label is measured wrapped at each width in turn until it is one line
//  high, as it is at a width it can't fill.
//
////////////////////////////////////////////////////////////////////////////////

int FieldDialogPanel::GetLabelDip() const
{
    static constexpr int    kStepDip   = 10;
    static constexpr int    kMostDip   = 300;
    static constexpr float  kOneLineAt = 10000.0f;
    int                     widest     = kLabelDip;



    for (const auto & [label, field] : m_rows)
    {
        float    oneLine   = 0.0f;
        float    height    = 0.0f;
        int      width     = widest;
        HRESULT  hrMeasure = label->MeasureWrappedDip (kOneLineAt, oneLine);



        while (SUCCEEDED (hrMeasure) && width < kMostDip)
        {
            hrMeasure = label->MeasureWrappedDip ((float) width, height);

            if (FAILED (hrMeasure) || height <= oneLine)
            {
                break;
            }

            width += kStepDip;
        }

        widest = width;
    }

    return widest;
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
    int  split = boundsDip.left + scaler.ToPx (GetLabelDip());
    int  y     = boundsDip.top;



    SetBounds (boundsDip);

    if (m_description != nullptr)
    {
        float    heightDip = 0.0f;
        HRESULT  hrMeasure = m_description->MeasureWrappedDip ((float) MulDiv (boundsDip.right - boundsDip.left, 96, (int) scaler.GetDpi()), heightDip);



        IGNORE_RETURN_VALUE (hrMeasure, S_OK);
        m_description->Layout (RECT { boundsDip.left, y, boundsDip.right, y + scaler.ToPx ((int) heightDip + 1) }, scaler);
        y += scaler.ToPx ((int) heightDip + 1) + gap;
    }

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
