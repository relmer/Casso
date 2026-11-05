#include "Pch.h"

#include "Ui/Debugger/Panes/SourceDocumentPanes.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentTabMarkTests
//
//  A source document's tab has the PC's marker at a stop where the PC is in
//  its file. While the machine runs, the marker is as it was at the last
//  stop: the PC passing in and out of the file at every frame added and
//  removed it, and each change moved every tab after it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SourceDocumentTabMarkTests)
{
public:

    TEST_METHOD (AtAStopTheMarkIsWhereThePcIs)
    {
        Assert::IsTrue  (SourceDocumentPane::HasPcMark (false, true, true));
        Assert::IsFalse (SourceDocumentPane::HasPcMark (true,  true, false));
    }


    TEST_METHOD (WhileRunningTheMarkIsAsItWasAtTheLastStop)
    {
        Assert::IsTrue  (SourceDocumentPane::HasPcMark (true,  false, false), L"the PC left the file");
        Assert::IsFalse (SourceDocumentPane::HasPcMark (false, false, true),  L"the PC came into the file");
    }
};
