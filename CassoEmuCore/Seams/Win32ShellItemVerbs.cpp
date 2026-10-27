#include "Pch.h"

#pragma comment(lib, "propsys.lib")

#include "Seams/Win32ShellItemVerbs.h"
#include "CassoExplorer/Model/CatalogModel.h"
#include "CassoExplorer/Model/DiskOperations.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "CassoExplorer/Model/TreeModel.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/AppleFileName.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Seams/Win32DiskFileIo.h"





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
