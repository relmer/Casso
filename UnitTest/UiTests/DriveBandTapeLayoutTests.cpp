#include "Pch.h"
#include "Shell/EmulatorShell.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBandTapeLayoutTests
//
//  Where the flat drive band puts the cassette recorder: to the right of the
//  drives, with the row centered as one unit, on a machine with cassette
//  jacks and the recorder attached -- and nowhere on a //c, which has no
//  jacks, or with the recorder detached.
//
//  EmulatorShell is driven without Initialize, as ShellKeyWiringTests does:
//  the widgets lay out against a DPI and need no window.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DriveBandTapeLayoutTests)
{
public:

    class TestShell : public EmulatorShell
    {
    public:
        using EmulatorShell::LayoutDriveRowForTest;
        using EmulatorShell::GetDriveRectForTest;
        using EmulatorShell::GetTapeAnchorForTest;
        using EmulatorShell::SetRecorderAttachedForTest;
    };


    static constexpr int   kClientW = 1600;
    static constexpr int   kClientH = 1000;
    static constexpr UINT  kDpi     = 96;


    static std::unique_ptr<TestShell> MakeShell (const char * machineId)
    {
        std::unique_ptr<TestShell>  shell = std::make_unique<TestShell>();



        shell->GetMachine().GetConfig().machineId = machineId;

        return shell;
    }


    TEST_METHOD (TheRecorderSitsRightOfTheDrivesOnAMachineWithJacks)
    {
        std::unique_ptr<TestShell>  shell = MakeShell ("Apple2e");
        RECT                        d1    = {};
        RECT                        d2    = {};
        RECT                        tape  = {};



        shell->LayoutDriveRowForTest (kClientW, kClientH, kDpi, 2);

        d1   = shell->GetDriveRectForTest (0);
        d2   = shell->GetDriveRectForTest (1);
        tape = shell->GetTapeAnchorForTest();

        Assert::IsTrue (d1.right <= d2.left,  L"drive 1 is left of drive 2");
        Assert::IsTrue (tape.left >= d2.right, L"and the recorder is right of both");
        Assert::AreEqual (d2.top, tape.top,    L"hanging from the same top as the drives");
        Assert::IsTrue (d1.left > 0 && d1.left < kClientW / 2 - (d2.right - d1.left) / 2,
                        L"the row centers with the recorder in it, so the drives sit left of where they would alone");
    }


    TEST_METHOD (ACcHasNoRecorderInItsBand)
    {
        std::unique_ptr<TestShell>  shell = MakeShell ("Apple2c");
        RECT                        tape  = {};



        shell->LayoutDriveRowForTest (kClientW, kClientH, kDpi, 1);
        tape = shell->GetTapeAnchorForTest();

        Assert::IsTrue (IsRectEmpty (&tape) && tape.left == 0, L"no jacks, so no place is made for a recorder");
    }


    TEST_METHOD (ADetachedRecorderTakesNoPlaceInTheBand)
    {
        std::unique_ptr<TestShell>  shell = MakeShell ("Apple2e");
        std::unique_ptr<TestShell>  alone = MakeShell ("Apple2e");
        RECT                        tape  = {};



        shell->SetRecorderAttachedForTest (false);
        shell->LayoutDriveRowForTest (kClientW, kClientH, kDpi, 2);
        tape = shell->GetTapeAnchorForTest();

        Assert::IsTrue (tape.left == 0 && tape.top == 0, L"a detached recorder gets no anchor");

        alone->LayoutDriveRowForTest (kClientW, kClientH, kDpi, 2);
        Assert::IsTrue (shell->GetDriveRectForTest (0).left > alone->GetDriveRectForTest (0).left,
                        L"and the drives center on their own, further right than beside a recorder");
    }
};