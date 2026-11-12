#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockFieldList.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFieldsView
//
//  Block mode's list of the current block's fields, a row each: the field's
//  offset, its label and its value. A field that links to a block has the
//  block after its value, in the theme's link color, and a click on it follows the link.
//  Pointing at a row reports the field, so its bytes can be marked. The
//  wheel scrolls the list.
//
////////////////////////////////////////////////////////////////////////////////

class BlockFieldsView : public IDxuiControl
{
public:
    static constexpr float  kRowDip     = 18.0f;
    static constexpr float  kFontDip    = 12.0f;
    static constexpr float  kOffsetDip  = 44.0f;
    static constexpr int    kWheelRows  = 3;

    using FieldFn = std::function<void (const BlockField * field)>;

    void  SetFields    (vector<BlockField> fields);
    void  SetOnFollow  (FieldFn fn) { m_onFollow = std::move (fn); }
    void  SetOnHover   (FieldFn fn) { m_onHover  = std::move (fn); }
    void  SetHeading   (const std::wstring & heading) { m_heading = heading; }

    const vector<BlockField> &  GetFields () const { return m_fields; }

    //  The text of a field's row after its offset.
    static std::wstring  FormatRow (const BlockField & field);

    void  Layout  (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

    LPCWSTR             GetCursorForPoint (POINT clientPx) const override;
    std::wstring        GetAccessibleName () const override { return L"Block fields"; }
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::ListView; }

private:
    static constexpr const wchar_t *  s_kpszFont = L"Segoe UI";

    int  FindRowAt    (POINT pointPx) const;
    int  GetRowsShown () const;

    vector<BlockField>  m_fields;
    std::wstring        m_heading;
    DxuiDpiScaler       m_scaler;
    FieldFn             m_onFollow;
    FieldFn             m_onHover;
    int                 m_top      = 0;
    int                 m_hoverRow = -1;
};
