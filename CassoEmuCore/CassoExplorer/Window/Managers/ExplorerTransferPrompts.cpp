#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerTransferPrompts.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/IExplorerWindow.h"
#include "CassoExplorer/CassoExplorerPromptDialog.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTransferPrompts::ExplorerTransferPrompts
//
////////////////////////////////////////////////////////////////////////////////

ExplorerTransferPrompts::ExplorerTransferPrompts (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTransferPrompts::MakeAddressPrompt
//
//  Asks for a binary's load address, prefilled with the content's
//  suggestion, and asks again after an address that does not parse.
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::AddressFn ExplorerTransferPrompts::MakeAddressPrompt()
{
    return [this] (const std::wstring & hostName, Word suggested, Word & outAddress)
    {
        std::wstring  text = std::format (L"${:04X}", suggested);

        for (;;)
        {
            if (!CassoExplorerPromptDialog::Ask (m_context.window.GetHwnd(), m_context.theme, L"Load address",
                                           hostName + L" is a binary. Load address:", text, 8, text))
            {
                return false;
            }

            if (ActionsGoTo::TryParseAddress (text, outAddress))
            {
                return true;
            }

            m_context.window.ShowMessage (L"Type an address from $0000 to $FFFF.", MB_ICONWARNING);
        }
    };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTransferPrompts::MakeCollisionPrompt
//
//  File Explorer's question for a file of the same name in the folder a
//  copy goes into. Keep both is the default, so Enter or Escape writes over
//  nothing.
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::CollisionFn ExplorerTransferPrompts::MakeCollisionPrompt()
{
    return [this] (const std::wstring & hostPath, int index, int count)
    {
        std::filesystem::path  path    (hostPath);
        std::wstring           ofCount = (count > 1) ? std::format (L" ({} of {})", index, count) : std::wstring();
        std::wstring           text    = std::format (L"{} already has a file called \"{}\".{}\n\nReplace it with the copy, skip this file, or keep both?",
                                                      path.parent_path().filename().wstring(), path.filename().wstring(), ofCount);
        int                    choice  = DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme, text.c_str(), L"Replace or skip files", MB_YESNOCANCEL | MB_DEFBUTTON3,
                                                         { L"Replace", L"Skip", L"Keep both" });

        return (choice == IDYES) ? CassoExplorerActions::Collision::Replace
             : (choice == IDNO)  ? CassoExplorerActions::Collision::Skip
                                 : CassoExplorerActions::Collision::KeepBoth;
    };
}
