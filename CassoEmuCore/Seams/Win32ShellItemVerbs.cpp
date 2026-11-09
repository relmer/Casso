#include "Pch.h"

#pragma comment(lib, "propsys.lib")

#include "Seams/Win32ShellItemVerbs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::~Win32ShellItemVerbs
//
////////////////////////////////////////////////////////////////////////////////

Win32ShellItemVerbs::~Win32ShellItemVerbs()
{
    if (m_menuHost != nullptr)
    {
        DestroyWindow (m_menuHost);
    }
}
