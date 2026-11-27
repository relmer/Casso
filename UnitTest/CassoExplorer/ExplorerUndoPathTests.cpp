#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerUndo.h"
#include "CassoExplorer/Window/Managers/ExplorerUndoStep.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUndoPathTests
//
//  A pasted image replaced since by a .po: undoing the paste removes the
//  .po, and undoing a move puts the .po back where the image was.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ExplorerUndoPathTests)
{
public:

    TEST_METHOD (ReplacePathIn_PointsACopyAtTheReplacement)
    {
        std::vector<ExplorerUndoStep>  steps (1);



        steps[0].kind    = ExplorerUndoStep::Kind::HostCopy;
        steps[0].paths   = { L"E:\\games\\Zork.DSK", L"E:\\games\\notes.txt" };
        steps[0].sources = { L"C:\\Disks\\zork.dsk", L"C:\\Disks\\notes.txt" };

        ExplorerUndo::ReplacePathIn (steps, L"E:\\games\\zork.dsk", L"E:\\games\\zork.po");

        Assert::AreEqual (std::wstring (L"E:\\games\\zork.po"),   steps[0].paths[0]);
        Assert::AreEqual (std::wstring (L"E:\\games\\notes.txt"), steps[0].paths[1]);
        Assert::AreEqual (std::wstring (L"C:\\Disks\\zork.dsk"),  steps[0].sources[0]);
    }

    TEST_METHOD (ReplacePathIn_MovesTheReplacementBackUnderItsExtension)
    {
        std::vector<ExplorerUndoStep>  steps (1);



        steps[0].kind    = ExplorerUndoStep::Kind::HostMove;
        steps[0].paths   = { L"E:\\games\\zork.dsk" };
        steps[0].sources = { L"C:\\Disks\\zork.dsk" };

        ExplorerUndo::ReplacePathIn (steps, L"E:\\games\\zork.dsk", L"E:\\games\\zork.po");

        Assert::AreEqual (std::wstring (L"E:\\games\\zork.po"), steps[0].paths[0]);
        Assert::AreEqual (std::wstring (L"C:\\Disks\\zork.po"), steps[0].sources[0]);
    }
};
