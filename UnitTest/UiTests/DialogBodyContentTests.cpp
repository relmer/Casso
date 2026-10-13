#include "Pch.h"

#include "Ui/Dialogs/DialogBodyContent.h"
#include "Ui/Dialogs/DialogDefinition.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContentTests
//
//  A dialog's picture is drawn at its own aspect: its longer side is the
//  display size and the shorter side is scaled to match, so a screen
//  thumbnail is not squashed into a square.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DialogBodyContentTests)
{
public:

    TEST_METHOD (AWidePictureIsMeasuredAtItsOwnAspect)
    {
        constexpr int     kDisplayDip  = 280;
        constexpr int     kThumbWidth  = 280;
        constexpr int     kThumbHeight = 192;
        DialogBodyContent wide;
        DialogBodyContent square;



        Assert::IsTrue (wide.SetImage   (MakeImage (kThumbWidth, kThumbHeight, kDisplayDip)), L"the wide picture is accepted");
        Assert::IsTrue (square.SetImage (MakeImage (kThumbWidth, kThumbWidth,  kDisplayDip)), L"the square picture is accepted");

        Assert::AreEqual (kThumbWidth - kThumbHeight,
                          square.GetPreferredHeightDip() - wide.GetPreferredHeightDip(),
                          L"the wide picture is its own height, not the display size");
    }


private:

    static DialogImage MakeImage (int width, int height, int displayDip)
    {
        constexpr size_t  kChannels = 4;
        constexpr Byte    kOpaque   = 0xFF;
        DialogImage       image;



        image.width     = width;
        image.height    = height;
        image.displayDp = (float) displayDip;
        image.rgba.assign ((size_t) width * (size_t) height * kChannels, kOpaque);

        return image;
    }
};





