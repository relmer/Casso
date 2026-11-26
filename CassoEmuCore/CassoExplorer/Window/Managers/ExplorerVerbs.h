#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerRawDialog.h"

struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerVerbs
//
//  Runs the item verbs, the Recycle Bin verbs and an inspector's raw sector
//  and block transfers, and reports their outcomes.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerVerbs
{
public:
    explicit ExplorerVerbs (ExplorerContext & context);

    //  The raw read and write pickers' history, apart from other pickers.
    static constexpr GUID  s_kRawPickerGuid = { 0xe347dd39, 0x4a18, 0x4a0d, { 0x93, 0x4a, 0xfd, 0x63, 0xb7, 0x9c, 0xba, 0x3e } };

    static std::wstring        GetVerbTitle       (CassoExplorerActions::Verb verb);
    HostFileNaming::Style      GetNamingStyle     () const;
    void                       RunVerb            (CassoExplorerActions::Verb verb);
    bool                       RunRecycleBinVerb  (CassoExplorerActions::Verb verb);
    std::vector<std::wstring>  GetRecycledPaths   (const std::vector<std::wstring> & ids);
    void                       ReportOutcome      (const CassoExplorerActions::Outcome & outcome, const wchar_t * verbName);
    void                       OpenCopyFolder     (const CassoExplorerActions::Outcome & outcome, const std::wstring & folder);

    //  A raw save or overwrite of an inspector window's image, starting at
    //  its selection.
    void  RunRawTransfer (HWND                                       owner,
                          const std::wstring                       & target,
                          bool                                       sectors,
                          bool                                       reading,
                          const CassoExplorerRawDialog::Outcome    & initial);

private:
    void  ReportRawOutcome (HWND owner, const CassoExplorerActions::Outcome & outcome, const std::wstring & title);

    ExplorerContext  & m_context;
};
