#include "Pch.h"

#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Disk2DebugPanel.h"
#include "Ui/InputDebugPanel.h"
#include "Ui/PrinterPanel.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelTooltipTests
//
//  The Disk II, input and printer panels' tips are the emulator's: each
//  takes the theme's tooltip colors when the panel takes the theme, again
//  when the theme changes, and Visual Studio's frame. No panel window is
//  opened.
//
////////////////////////////////////////////////////////////////////////////////

namespace UiTests
{
    TEST_CLASS (DevicePanelTooltipTests)
    {
    public:

        template <class Panel>
        static void  CheckTheTipFollowsTheTheme (const wchar_t * name)
        {
            std::unique_ptr<Panel>  owner = std::make_unique<Panel>();
            Panel                 & panel = *owner;
            CassoTheme              skeuo = CassoTheme::MakeSkeuomorphic();
            CassoTheme              retro = CassoTheme::MakeRetroTerminal();
            std::wstring            of    = std::wstring (L" of the ") + name + L" panel";



            panel.SetTheme (&skeuo);

            Assert::AreEqual (skeuo.TooltipBackground(), panel.GetTooltip().GetBackgroundArgb(), (L"the fill" + of).c_str());
            Assert::AreEqual (skeuo.TooltipBorder(),     panel.GetTooltip().GetBorderArgb(),     (L"the border" + of).c_str());
            Assert::AreEqual (skeuo.TooltipForeground(), panel.GetTooltip().GetTextArgb(),       (L"the text" + of).c_str());

            panel.SetTheme (&retro);

            Assert::AreEqual (retro.TooltipBackground(), panel.GetTooltip().GetBackgroundArgb(), (L"the new theme's fill" + of).c_str());
            Assert::AreEqual (retro.TooltipBorder(),     panel.GetTooltip().GetBorderArgb(),     (L"the new theme's border" + of).c_str());
            Assert::AreEqual (retro.TooltipForeground(), panel.GetTooltip().GetTextArgb(),       (L"the new theme's text" + of).c_str());

            Assert::AreEqual (6.0f, panel.GetTooltip().GetCornerRadiusPx(), (L"Visual Studio's 6.4-DIP corner at 96 DPI" + of).c_str());
            Assert::AreEqual (9.0f, panel.GetTooltip().GetPadXPx(),          (L"its text inside a whole-pixel border" + of).c_str());
        }



        TEST_METHOD (TheDiskIiPanelsTipFollowsTheTheme)
        {
            CheckTheTipFollowsTheTheme<Disk2DebugPanel> (L"Disk II");
        }



        TEST_METHOD (TheInputPanelsTipFollowsTheTheme)
        {
            CheckTheTipFollowsTheTheme<InputDebugPanel> (L"input");
        }



        TEST_METHOD (ThePrinterPanelsTipFollowsTheTheme)
        {
            CheckTheTipFollowsTheTheme<PrinterPanel> (L"printer");
        }
    };
}
