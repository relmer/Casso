#include "Pch.h"

#include "Ui/Debugger/SourceSyntax.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SourceSyntaxBlendTests
//
//  The instruction rows under a source line fade each color halfway toward
//  the background: lighter on a light theme, darker on a dark one.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SourceSyntaxBlendTests)
{
public:

    TEST_METHOD (ADarkBackgroundDarkensHalfway)
    {
        SourceSyntax::Colors  colors;



        colors.mnemonic = 0xFF6496C8;
        colors          = colors.GetBlended (0xFF000000);

        Assert::AreEqual (0xFF324B64u, colors.mnemonic);
        Assert::AreEqual (0u,          colors.string, L"no color stays none");
    }



    TEST_METHOD (ALightBackgroundLightensHalfway)
    {
        SourceSyntax::Colors  colors;



        colors.mnemonic = 0xFF0000FF;
        colors          = colors.GetBlended (0xFFFFFFFF);

        Assert::AreEqual (0xFF7F7FFFu, colors.mnemonic);
    }



    TEST_METHOD (AlphaIsKept)
    {
        SourceSyntax::Colors  colors;



        colors.comment = 0x80204060;
        colors         = colors.GetBlended (0xFF204060);

        Assert::AreEqual (0x80204060u, colors.comment);
    }
};
