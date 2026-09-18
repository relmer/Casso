#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueNamedControl
//
//  A Dxui control with an accessible name of its own. The widgets report
//  their role but name themselves generically, and the browser has two list
//  views that must be told apart.
//
////////////////////////////////////////////////////////////////////////////////

template <typename Control>
class CassqueNamedControl : public Control
{
public:
    using Control::Control;

    void  SetAccessibleName (std::wstring name) { m_accessibleName = std::move (name); }

    std::wstring  GetAccessibleName() const override
    {
        return m_accessibleName.empty() ? Control::GetAccessibleName() : m_accessibleName;
    }

private:
    std::wstring  m_accessibleName;
};
