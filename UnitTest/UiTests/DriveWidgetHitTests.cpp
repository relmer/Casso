#include "Pch.h"

#include "Ui/Chrome/DriveWidget.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





TEST_CLASS (DriveWidgetHitTests)
{
public:

    TEST_METHOD (HitTest_Returns_Expected_Regions)
    {
        DriveWidget  drive;
        RECT         body  = {};
        RECT         eject = {};



        drive.Initialize (6, 0, nullptr);
        drive.Layout (100, 200, 96);
        body  = drive.BodyRect();
        eject = drive.EjectRect();

        Assert::IsTrue (drive.HitTest ((body.left + body.right) / 2,
                                       (body.top + body.bottom) / 2) == DriveWidgetRegion::Body);
        Assert::IsTrue (drive.HitTest ((eject.left + eject.right) / 2,
                                       (eject.top + eject.bottom) / 2) == DriveWidgetRegion::Eject);
        Assert::IsTrue (drive.HitTest (body.left - 1, body.top - 1) == DriveWidgetRegion::None);
    }
};
