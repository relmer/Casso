#include "Pch.h"

#include "Devices/Disk/WriteProtectChange.h"




static constexpr const wchar_t *  s_kpszWriteProtectLabel = L"Write-protect \"{}\"";
static constexpr const wchar_t *  s_kpszWriteEnableLabel  = L"Write-enable \"{}\"";
static constexpr const wchar_t *  s_kpszSetFlag           = L"Set WOZ write-protect flag for \"{}\"";
static constexpr const wchar_t *  s_kpszSetAttribute      = L"Set read-only attribute for \"{}\"";
static constexpr const wchar_t *  s_kpszClearedFlag       = L"Cleared WOZ write-protect flag for \"{}\"";
static constexpr const wchar_t *  s_kpszClearedAttribute  = L"Cleared read-only attribute for \"{}\"";
static constexpr const wchar_t *  s_kpszClearedBoth       = L"Cleared read-only attribute and WOZ write-protect flag for \"{}\"";





////////////////////////////////////////////////////////////////////////////////
//
//  MakePlan
//
//  Protecting sets exactly one mechanism: the flag on a WOZ, the attribute on
//  anything else. A WOZ is not also marked read-only, because the flag is
//  the protection the format defines and it travels with the file.
//
//  Write-enabling clears whatever is set. The attribute goes first when both
//  are, since the flag lives inside the file and writing it needs the file
//  writable; the order is the caller's to honor, and this only records which
//  of the two apply.
//
////////////////////////////////////////////////////////////////////////////////

WriteProtectChange WriteProtectChange::MakePlan (
    bool            isWoz,
    bool            imageFlag,
    bool            readOnlyAttribute,
    const wstring & fileName)
{
    WriteProtectChange  plan;



    plan.fileName   = fileName;
    plan.protecting = !(imageFlag || readOnlyAttribute);

    if (plan.protecting)
    {
        plan.changesImageFlag = isWoz;
        plan.changesAttribute = !isWoz;
    }
    else
    {
        plan.changesImageFlag = imageFlag;
        plan.changesAttribute = readOnlyAttribute;
    }

    return plan;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsImageProtected
//
//  Only the two mechanisms the command can change count. The drive preference
//  in Settings, a permission denial and a damaged image protect the disk too,
//  but this command toggles none of them, so none of them may flip its label.
//
////////////////////////////////////////////////////////////////////////////////

bool WriteProtectChange::IsImageProtected (const WriteProtectInfo & wp)
{
    return wp.imageFlag || wp.readOnlyFile;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMenuLabel
//
//  Very long names are shortened in the middle, keeping the extension visible,
//  so the row stays inside the dropdown.
//
////////////////////////////////////////////////////////////////////////////////

wstring WriteProtectChange::GetMenuLabel (bool isProtected, const wstring & fileName)
{
    constexpr size_t  kMaxNameChars  = 20;
    constexpr size_t  kKeepHeadChars = 10;
    constexpr size_t  kKeepTailChars = 7;
    wstring           name           = fileName;



    if (name.size() > kMaxNameChars)
    {
        name = name.substr (0, kKeepHeadChars) + L"..."
             + name.substr (name.size() - kKeepTailChars);
    }

    return std::vformat (isProtected ? s_kpszWriteEnableLabel : s_kpszWriteProtectLabel,
                         std::make_wformat_args (name));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DescribeResult
//
//  States which mechanism changed, so a user who sees "read-only attribute"
//  knows the change is visible in Explorer, and one who sees "WOZ
//  write-protect flag" knows it travels with the file.
//
////////////////////////////////////////////////////////////////////////////////

wstring WriteProtectChange::DescribeResult (const WriteProtectChange & change)
{
    const wchar_t *  format = s_kpszClearedAttribute;



    if (change.protecting)
    {
        format = change.changesImageFlag ? s_kpszSetFlag : s_kpszSetAttribute;
    }
    else if (change.changesImageFlag && change.changesAttribute)
    {
        format = s_kpszClearedBoth;
    }
    else if (change.changesImageFlag)
    {
        format = s_kpszClearedFlag;
    }

    return std::vformat (format, std::make_wformat_args (change.fileName));
}
