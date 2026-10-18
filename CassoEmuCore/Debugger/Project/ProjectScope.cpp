#include "Pch.h"

#include "Debugger/Project/ProjectScope.h"

#include "Core/TextEncoding.h"
#include "Sha1.h"





static constexpr const char * s_kKindNames[] = { "debugFile", "binary", "machineBoot", "machine" };
static constexpr size_t       s_kKeyDigits   = 16;





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectScope::Choose
//
////////////////////////////////////////////////////////////////////////////////

ProjectScope ProjectScope::Choose (
    const std::string  & debugFileKey,
    const std::string  & debugFileName,
    const std::string  & binaryKey,
    const std::string  & binaryName,
    const std::string  & machine,
    const std::wstring & bootDisk)
{
    std::string  disk = TextEncoding::WideToNarrow (bootDisk);



    if (!debugFileKey.empty())
    {
        return { ProjectScopeKind::DebugFile, debugFileKey, "debug file " + debugFileName };
    }

    if (!binaryKey.empty())
    {
        return { ProjectScopeKind::Binary, binaryKey, "binary " + binaryName };
    }

    if (!bootDisk.empty())
    {
        return { ProjectScopeKind::MachineBoot, machine + "|" + disk, std::format ("{} booting {}", machine, disk.substr (disk.find_last_of ("/\\") + 1)) };
    }

    return { ProjectScopeKind::Machine, machine, machine };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectScope::GetFileName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ProjectScope::GetFileName() const
{
    std::string  hash = Sha1::ComputeTextHex (std::string (GetKindName (kind)) + ":" + key);



    return TextEncoding::NarrowToWide (std::format ("{}-{}.cassoproj", GetKindName (kind), hash.substr (0, s_kKeyDigits)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectScope::GetKindName
//
////////////////////////////////////////////////////////////////////////////////

const char * ProjectScope::GetKindName (ProjectScopeKind kind)
{
    return s_kKindNames[(size_t) kind];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectScope::TryParseKind
//
////////////////////////////////////////////////////////////////////////////////

bool ProjectScope::TryParseKind (const std::string & name, ProjectScopeKind & kind)
{
    for (size_t i = 0; i < std::size (s_kKindNames); i++)
    {
        if (name == s_kKindNames[i])
        {
            kind = (ProjectScopeKind) i;
            return true;
        }
    }

    return false;
}
