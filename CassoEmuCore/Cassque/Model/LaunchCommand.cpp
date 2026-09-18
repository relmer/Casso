#include "Pch.h"

#include "Cassque/Model/LaunchCommand.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LaunchCommand::QuoteArgument
//
//  Quoted when it holds a space, a tab or a quote, or is empty; embedded
//  quotes escaped and a trailing run of backslashes doubled, since a
//  backslash before the closing quote would otherwise escape it.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring LaunchCommand::QuoteArgument (const std::wstring & argument)
{
    std::wstring  quoted     = L"\"";
    size_t        backslashes = 0;
    bool          needsQuotes = argument.empty() || argument.find_first_of (L" \t\"") != std::wstring::npos;



    if (!needsQuotes)
    {
        return argument;
    }

    for (wchar_t c : argument)
    {
        if (c == L'\\')
        {
            backslashes++;
            continue;
        }

        if (c == L'"')
        {
            quoted.append (backslashes * 2 + 1, L'\\');
        }
        else
        {
            quoted.append (backslashes, L'\\');
        }

        backslashes = 0;
        quoted     += c;
    }

    quoted.append (backslashes * 2, L'\\');
    quoted += L'"';

    return quoted;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LaunchCommand::GetSiblingPath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring LaunchCommand::GetSiblingPath (const std::wstring & moduleDirectory, const wchar_t * exeName)
{
    std::wstring  path = moduleDirectory;



    if (!path.empty() && path.back() != L'\\' && path.back() != L'/')
    {
        path += L'\\';
    }

    return path + exeName;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LaunchCommand::MakeCassqueArguments
//
////////////////////////////////////////////////////////////////////////////////

std::wstring LaunchCommand::MakeCassqueArguments (HWND owner, const std::wstring & titlePrefix)
{
    std::wstring  arguments = L"--owner " + std::to_wstring ((uint64_t) (UINT_PTR) owner);



    if (!titlePrefix.empty())
    {
        arguments += L" --title " + QuoteArgument (titlePrefix);
    }

    return arguments;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LaunchCommand::MakeCassoArguments
//
//  The path is always quoted, whether or not it needs to be, as the contract
//  between the two executables states.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring LaunchCommand::MakeCassoArguments (const std::wstring & diskPath, const std::wstring & titlePrefix)
{
    std::wstring  quoted    = QuoteArgument (diskPath);
    std::wstring  arguments;



    if (quoted.empty() || quoted.front() != L'"')
    {
        quoted = L"\"" + quoted + L"\"";
    }

    arguments = L"--disk1 " + quoted;

    if (!titlePrefix.empty())
    {
        arguments += L" --title " + QuoteArgument (titlePrefix);
    }

    return arguments;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LaunchCommand::DescribeMissing
//
////////////////////////////////////////////////////////////////////////////////

std::wstring LaunchCommand::DescribeMissing (const std::wstring & exePath)
{
    return L"Cassque is not installed\n"
           L"The disk browser was expected at " + exePath + L". Build or install it "
           L"beside Casso and try again.";
}





////////////////////////////////////////////////////////////////////////////////
//
//  LaunchCommand::LaunchCassque
//
////////////////////////////////////////////////////////////////////////////////

LaunchCommand::Outcome LaunchCommand::LaunchCassque (
    IProcessLauncher    & launcher,
    const std::wstring  & moduleDirectory,
    HWND                  owner,
    const std::wstring  & titlePrefix)
{
    std::wstring  exePath = GetSiblingPath (moduleDirectory, kCassqueExe);
    HRESULT       hr      = S_OK;



    if (!launcher.Exists (exePath))
    {
        return Outcome::Missing;
    }

    hr = launcher.Launch (exePath, MakeCassqueArguments (owner, titlePrefix));

    return SUCCEEDED (hr) ? Outcome::Launched : Outcome::Failed;
}
