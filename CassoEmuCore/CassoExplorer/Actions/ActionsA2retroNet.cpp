#include "Pch.h"

#include "CassoExplorer/Actions/ActionsA2retroNet.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/A2retroNetConfig.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsA2retroNet::GetConfigPath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ActionsA2retroNet::GetConfigPath (const std::wstring & imagePath)
{
    std::filesystem::path  root = std::filesystem::path (imagePath).root_path();



    return (root / A2retroNetConfig::kpszFileName).wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsA2retroNet::CanAdd
//
////////////////////////////////////////////////////////////////////////////////

bool ActionsA2retroNet::CanAdd (const std::wstring & imagePath) const
{
    return !imagePath.empty() && m_actions.m_fs.Exists (GetConfigPath (imagePath));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsA2retroNet::Add
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsA2retroNet::Add (const std::wstring & imagePath)
{
    ActionsTypes::Outcome  outcome;
    IDiskFileIo          & io       = m_actions.m_browser.GetOperations().GetFileIo();
    std::wstring           leaf     = CassoExplorerActions::GetLeafName (imagePath);
    std::string            config   = TextEncoding::WideToNarrow (GetConfigPath (imagePath));
    std::vector<Byte>      image;
    std::vector<Byte>      text;
    std::string            problem;
    std::string            path;
    std::string            written;
    A2retroNetConfig       settings;
    int                    drive    = 0;
    bool                   isSet    = false;
    HRESULT                hr       = S_OK;



    hr = io.ReadAllBytes (TextEncoding::WideToNarrow (imagePath), image);

    if (SUCCEEDED (hr))
    {
        hr = io.ReadAllBytes (config, text);
    }

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, leaf + L" or A2retroNET.txt could not be read.");
        return outcome;
    }

    if (!A2retroNetConfig::CheckImage (TextEncoding::WideToNarrow (imagePath), image, problem))
    {
        ActionsCopy::AppendMessage (outcome, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), std::format (L"{} wasn't added. {}", leaf, TextEncoding::NarrowToWide (problem)));
        return outcome;
    }

    settings = A2retroNetConfig::Parse (std::string (text.begin(), text.end()));
    path     = A2retroNetConfig::MakePath (std::filesystem::path (imagePath).root_path(), imagePath);

    for (int i = 0; !isSet && i < settings.number; i++)
    {
        isSet = _stricmp (settings.paths[i].c_str(), path.c_str()) == 0;
        drive = isSet ? i : drive;
    }

    if (isSet)
    {
        ActionsCopy::AppendMessage (outcome, S_OK, std::format (L"{} is drive {} already.", leaf, drive + 1));
        return outcome;
    }

    if (!settings.TryFindFreeDrive (drive))
    {
        ActionsCopy::AppendMessage (outcome, HRESULT_FROM_WIN32 (ERROR_DISK_FULL), std::format (L"Each of the {} drives has an image, so {} wasn't added.", settings.number, leaf));
        return outcome;
    }

    settings.paths[drive] = path;
    written               = settings.Write();

    text.assign (written.begin(), written.end());

    hr = io.WriteAllBytes (config, text);

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"A2retroNET.txt could not be written.");
        return outcome;
    }

    ActionsCopy::AppendMessage (outcome, S_OK, std::format (L"{} is drive {}.", leaf, drive + 1));
    outcome.written = 1;

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsA2retroNet::CreateConfig
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsA2retroNet::CreateConfig (const std::wstring & root)
{
    ActionsTypes::Outcome  outcome;
    IDiskFileIo          & io     = m_actions.m_browser.GetOperations().GetFileIo();
    std::wstring           path   = (std::filesystem::path (root) / A2retroNetConfig::kpszFileName).wstring();
    std::string            text   = A2retroNetConfig().Write();
    std::vector<Byte>      bytes (text.begin(), text.end());
    HRESULT                hr     = S_OK;



    if (m_actions.m_fs.Exists (path))
    {
        ActionsCopy::AppendMessage (outcome, S_OK, L"The card has an A2retroNET.txt already.");
        return outcome;
    }

    hr = io.WriteAllBytes (TextEncoding::WideToNarrow (path), bytes);

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"A2retroNET.txt could not be written.");
        return outcome;
    }

    ActionsCopy::AppendMessage (outcome, S_OK, L"The card has an A2retroNET.txt with no drives set.");
    outcome.written = 1;

    return outcome;
}
