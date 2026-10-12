// This project only produces the .exe from the CassoEmuCore.lib
//
// There is no code here
// The entry point is CassoEmuCore/CassoExplorer/CassoExplorerMain.cpp





// Common Controls 6: without it the shell's dialogs, drag images and drop
// descriptions are drawn with the old controls, unlike File Explorer's.
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
