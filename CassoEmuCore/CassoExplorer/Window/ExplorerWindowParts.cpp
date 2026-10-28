#include "Pch.h"

#include "CassoExplorer/Window/ExplorerWindowParts.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerWindowParts::ExplorerWindowParts
//
//  The context's references to the components bind before the components
//  are constructed; each component only stores the context.
//
////////////////////////////////////////////////////////////////////////////////

ExplorerWindowParts::ExplorerWindowParts (IExplorerWindow             & window,
                                          CassoExplorerBrowser        & browser,
                                          CassoExplorerActions        & actions,
                                          CassoExplorerPrefs          & prefs,
                                          CassoExplorerWindowContext  & app,
                                          DxuiDpiScaler               & scaler,
                                          const DxuiTheme *           & theme)
    : commands (CassoExplorerCommands::Handlers {
                    [this] (int id)       { commandManager.Dispatch (id); },
                    [this] (int id)       { return commandManager.IsEnabled (id); },
                    [this] (int id)       { return commandManager.IsChecked (id); },
                    [this] (int id)       { return commandManager.GetCommandLabel (id); } }),
      context  { .window           = window,
                 .browser          = browser,
                 .actions          = actions,
                 .prefs            = prefs,
                 .app              = app,
                 .explorerOptions  = explorerOptions,
                 .dialogs          = dialogs,
                 .shellIcons       = shellIcons,
                 .infoTips         = infoTips,
                 .shellVerbs       = shellVerbs,
                 .commands         = commands,
                 .scaler           = scaler,
                 .theme            = theme,
                 .layout           = layout,
                 .listPane         = listPane,
                 .folderSync       = folderSync,
                 .cassoLinkManager = cassoLinkManager,
                 .dragDrop         = dragDrop,
                 .tabManager       = tabManager,
                 .preview          = preview,
                 .addressBar       = addressBar,
                 .menus            = menus,
                 .commandManager   = commandManager,
                 .fileOps          = fileOps,
                 .input            = input,
                 .tips             = tips,
                 .themes           = themes,
                 .statusBar        = statusBar,
                 .toolbars         = toolbars,
                 .treePane         = treePane }
{
}
