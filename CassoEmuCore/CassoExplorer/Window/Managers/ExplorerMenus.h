#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "Seams/Win32PackagedCommands.h"

struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus
//
//  The window's menus: the list's, the tree's and the header's, Open
//  with, New, the pins, packaged and Windows commands, and each verb's
//  label, glyph and File Explorer icon.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerMenus
{
public:
    explicit ExplorerMenus (ExplorerContext & context);

    static constexpr const wchar_t *  kExplorerIconFolder    = L"\\SystemApps\\MicrosoftWindows.Client.FileExp_cw5n1h2txyewy\\FileExplorerExtensions\\Assets\\images\\contrast-standard\\";

    //  A 5.25-inch floppy in File Explorer's icon hand: a 16-unit jacket with
    //  the write-protect notch cut in its edge, its hub ring and index hole,
    //  and the head slot in the accent. {INK}, {ACC}
    //  and {BODY} take the theme's outline, accent and fill.
    static constexpr const char *     kFloppy525Svg =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" fill=\"none\" viewBox=\"0 0 16 16\">"
        "<path fill=\"{BODY}\" stroke=\"{INK}\" d=\"M13.5 1.5h-11a1 1 0 0 0-1 1v11a1 1 0 0 0 1 1h11a1 1 0 0 0 1-1V5.5h-1V3.5h1v-1a1 1 0 0 0-1-1Z\"/>"
        "<circle cx=\"8\" cy=\"6.5\" r=\"2.25\" stroke=\"{INK}\"/>"
        "<circle cx=\"4.5\" cy=\"6.5\" r=\".6\" fill=\"{INK}\"/>"
        "<rect x=\"7.25\" y=\"10\" width=\"1.5\" height=\"4\" rx=\".75\" fill=\"{ACC}\"/>"
        "</svg>";
    static constexpr int       kMenuIconDip        = 16;     // DxuiPopupMenu's row icon

    //  The commands of the menu open now, alive for as long as it is open.
    std::vector<std::shared_ptr<DxuiCommand>> &  GetMenuCommands () { return m_menuCommands; }

    //  The commands installed apps add to Explorer's menus, and New's choices,
    //  read once.
    void  StartPackagedCommands (HWND hwnd, UINT message) { m_packagedCommands.Start (hwnd, message); }
    void  AddPinMenuCommand     (std::vector<DxuiPopupMenuItem> & items, const std::wstring & folder);
    void  AddOpenRows           (std::vector<DxuiPopupMenuItem> & items, const Location & location, bool withOpen);
    void  AddNewChoices     (std::vector<DxuiPopupMenuItem> & items, const Location & target,
                             std::shared_ptr<const DxuiCommand> folder, std::shared_ptr<const DxuiCommand> disk);
    void  RunNewChoice      (const Location & target, const std::wstring & verb);

    //  The rows of the Sort, Group by and View menus, shared by the command
    //  bar and the list's background menu.
    std::vector<DxuiPopupMenuItem>  GetSortChoices         ();
    std::vector<DxuiPopupMenuItem>  GetGroupChoices        ();
    std::vector<DxuiPopupMenuItem>  GetViewChoices         ();
    void                            AddViewMenus           (std::vector<DxuiPopupMenuItem> & items);
    bool                            ShowScrollbarMenu      (const DxuiMouseEvent & ev);
    void                            AddPackagedCommands    (std::vector<DxuiPopupMenuItem> & items);
    void                            AddPackagedCommandsFor (std::vector<DxuiPopupMenuItem> & items, const std::vector<std::wstring> & paths, Win32PackagedCommands::ItemKind kind);
    void                            AddPackagedEntry       (std::vector<DxuiPopupMenuItem> & items, const Win32PackagedCommands::Entry & entry, const std::vector<std::wstring> & paths);
    bool                            AddWindowsCommands     (std::vector<DxuiPopupMenuItem> & items, const std::vector<std::wstring> & paths, Win32PackagedCommands::ItemKind kind);
    bool                            GetSelectionKind       (std::vector<std::wstring> & outPaths, Win32PackagedCommands::ItemKind & outKind) const;
    void                            ShowListHeaderMenu     (int x, int y, int column);
    void                            ShowListContextMenu    (int x, int y, int group = -1);
    void  AddMenuCommand         (std::vector<DxuiPopupMenuItem> & items, const wchar_t * label, std::function<void()> dispatch, const wchar_t * accelerator = L"", const wchar_t * menuGlyph = nullptr);
    void                                    AddOpenWithMenu        (std::vector<DxuiPopupMenuItem> & items);
    static const wchar_t                  * GetVerbGlyph           (CassoExplorerActions::Verb verb);
    static int                              GetIconOrder           (CassoExplorerActions::Verb verb);
    void                                    ShowViewOnButton       ();
    bool                                    IsListVerbOffered      (CassoExplorerActions::Verb verb) const;
    void                                    AddCopyAsMenu          (std::vector<DxuiPopupMenuItem> & items);
    void                                    SetCommandBarDropDowns ();
    static const wchar_t                  * GetVerbLabel           (CassoExplorerActions::Verb verb);
    static const wchar_t                  * GetVerbMenuGlyph       (CassoExplorerActions::Verb verb);
    static const wchar_t                  * GetVerbMenuSvgName     (CassoExplorerActions::Verb verb);
    static std::wstring                     GetIconButtonTip       (const DxuiCommand & command);
    void                                    ApplyMenuSvgs          ();
    const std::string                     * GetMenuSvg             (const wchar_t * name);
    std::shared_ptr<const DxuiIconImage>    GetOpenMenuImage       (bool browsable, const std::wstring & path);

private:
    ExplorerContext  & m_context;
    std::vector<std::shared_ptr<DxuiCommand>>  m_menuCommands;
    Win32PackagedCommands                      m_packagedCommands;
    std::map<std::wstring, std::unique_ptr<std::string>>  m_menuSvgs;
    std::map<std::wstring, std::shared_ptr<const DxuiIconImage>>  m_openMenuImages;
};
