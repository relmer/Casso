#include "Pch.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Dialogs/SalvageDialogContent.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SalvageDialogContentTests
//
//  The salvage dialog's body: the source path above the sector table.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SalvageDialogContentTests)
{
public:

    //  The path is drawn semibold, so it is measured semibold. In the regular
    //  face a long path measured a line shorter than it drew, and the table
    //  below it was drawn over its last line.
    TEST_METHOD (ThePathIsMeasuredInTheWeightItIsDrawnIn)
    {
        static constexpr const wchar_t *  kPath = L"C:\\Users\\someone\\Documents\\Apple II\\Disks\\Collections\\Utilities\\A disk with a long name.dsk";

        SalvageDialogContent  content;
        SalvageAssessment     assessment;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        CassoTheme            theme      = CassoTheme::MakeSkeuomorphic();
        DxuiDpiScaler         scaler;
        bool                  isDrawn    = false;
        bool                  isMeasured = false;
        DxuiFontWeight        drawn      = DxuiFontWeight::Normal;
        DxuiFontWeight        measured   = DxuiFontWeight::Normal;



        assessment.totalSectors = 560;

        content.SetAssessment (kPath, L"A disk with a long name.salvaged.woz", assessment);
        content.Layout (RECT { 0, 0, 420, 400 }, scaler);
        content.Paint (painter, text, theme);

        for (const RecordedTextCall & call : text.Calls())
        {
            if (call.kind == RecordedTextKind::DrawString && call.text == kPath)
            {
                isDrawn = true;
                drawn   = call.weight;
            }
        }

        for (const auto & [measuredText, weight] : text.WrappedMeasures())
        {
            if (measuredText == kPath)
            {
                isMeasured = true;
                measured   = weight;
            }
        }

        Assert::IsTrue (isDrawn,    L"the path is drawn");
        Assert::IsTrue (isMeasured, L"and measured wrapped");
        Assert::IsTrue (drawn == DxuiFontWeight::SemiBold, L"semibold");
        Assert::IsTrue (measured == drawn, L"in the weight it is drawn in");
    }
};
