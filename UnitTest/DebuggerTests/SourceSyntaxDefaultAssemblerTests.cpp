#include "Pch.h"

#include "Ui/Debugger/SourceSyntax.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SourceSyntaxDefaultAssemblerTests
//
//  When the text gives no assembler clue, or equal clues to more than one,
//  the source is read as as65's.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SourceSyntaxDefaultAssemblerTests)
{
public:

    using Assembler = SourceSyntax::Assembler;



    TEST_METHOD (NoClueIsAs65)
    {
        std::vector<std::wstring>  plain = { L"start   lda   #0", L"        rts" };



        Assert::IsTrue (Assembler::As65 == SourceSyntax::DetectAssembler (plain));
    }



    TEST_METHOD (EqualCluesAreAs65)
    {
        std::vector<std::wstring>  oneEach = { L"]loop   LDA   #0", L"@next:  rts" };
        std::vector<std::wstring>  twoEach = { L"]loop   LDA   #0", L"@next:  rts", L"]done   RTS", L"@last:  rts" };



        Assert::IsTrue (Assembler::As65 == SourceSyntax::DetectAssembler (oneEach), L"one clue each");
        Assert::IsTrue (Assembler::As65 == SourceSyntax::DetectAssembler (twoEach), L"two clues each");
    }



    TEST_METHOD (TheMostCluesStillDecide)
    {
        std::vector<std::wstring>  ca65 = { L"        .proc main", L"@loop:  lda   #0", L"        .endproc" };



        Assert::IsTrue (Assembler::Ca65 == SourceSyntax::DetectAssembler (ca65));
    }
};
