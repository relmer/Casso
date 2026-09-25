#include "Pch.h"

#include "Ui/Settings/JoyportSwitchView.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  JoyportSwitchViewTests
//
//  The Atari joystick the Controllers page draws while the Joyport is
//  attached, checked as geometry: the markings a CX40 has, where it has them.
//  Painting is not exercised; these pin the shapes the painter is handed.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (JoyportSwitchViewTests)
{
public:

    static constexpr float  kSide = 200.0f;


    static float DistanceFrom (DxuiPointF center, DxuiPointF point)
    {
        float  dx = point.x - center.x;
        float  dy = point.y - center.y;

        return std::sqrt (dx * dx + dy * dy);
    }


    TEST_METHOD (TheRingHasSevenDashesBetweenEachPairOfCardinals)
    {
        JoyportStickArt  art = JoyportSwitchView::BuildArt (0.0f, 0.0f, kSide);

        Assert::AreEqual (static_cast<size_t> (4 * JoyportSwitchView::kDashesPerQuadrant), art.dashes.size());

        for (const std::vector<DxuiPointF> & dash : art.dashes)
        {
            for (const DxuiPointF & point : dash)
            {
                float  radius = DistanceFrom (art.ringCenter, point);

                Assert::IsTrue (radius > art.ringInner - 0.01f && radius < art.ringOuter + 0.01f,
                                L"every dash lies on the ring");
            }
        }
    }


    TEST_METHOD (LeftRightAndDownHaveMarkersAndUpHasTop)
    {
        JoyportStickArt  art      = JoyportSwitchView::BuildArt (0.0f, 0.0f, kSide);
        bool             hasLeft  = false;
        bool             hasRight = false;
        bool             hasDown  = false;

        Assert::AreEqual (static_cast<size_t> (3), art.markers.size(), L"three chevrons; up is the word TOP");

        for (const JoyportStickMarker & marker : art.markers)
        {
            hasLeft  = hasLeft  || marker.direction == JoystickSwitch::Left;
            hasRight = hasRight || marker.direction == JoystickSwitch::Right;
            hasDown  = hasDown  || marker.direction == JoystickSwitch::Down;

            Assert::IsTrue (marker.direction != JoystickSwitch::Up && marker.direction != JoystickSwitch::Fire);
        }

        Assert::IsTrue (hasLeft && hasRight && hasDown);
        Assert::IsTrue (art.topCenter.y < art.ringCenter.y, L"TOP is at the top of the ring");
        Assert::AreEqual (art.ringCenter.x, art.topCenter.x, 0.01f);
    }


    TEST_METHOD (EachMarkerSitsOnItsOwnSideOfTheRing)
    {
        JoyportStickArt  art = JoyportSwitchView::BuildArt (0.0f, 0.0f, kSide);

        for (const JoyportStickMarker & marker : art.markers)
        {
            for (const std::vector<DxuiPointF> & piece : marker.pieces)
            {
                for (const DxuiPointF & point : piece)
                {
                    if (marker.direction == JoystickSwitch::Left)  { Assert::IsTrue (point.x < art.ringCenter.x); }
                    if (marker.direction == JoystickSwitch::Right) { Assert::IsTrue (point.x > art.ringCenter.x); }
                    if (marker.direction == JoystickSwitch::Down)  { Assert::IsTrue (point.y > art.ringCenter.y); }
                }
            }
        }
    }


    TEST_METHOD (AMarkerPieceIsDeepestBesideItsCardinal)
    {
        //  A dash extended inward by a right triangle whose deep end is at the
        //  cardinal: its innermost point is inside the ring, and it is the
        //  piece's nearest point to the cardinal's axis.
        JoyportStickArt             art   = JoyportSwitchView::BuildArt (0.0f, 0.0f, kSide);
        const JoyportStickMarker  & right = art.markers[0];

        Assert::IsTrue (right.direction == JoystickSwitch::Right);

        for (const std::vector<DxuiPointF> & piece : right.pieces)
        {
            size_t  deepest = 0;
            size_t  i       = 0;

            for (i = 1; i < piece.size(); i++)
            {
                if (DistanceFrom (art.ringCenter, piece[i]) < DistanceFrom (art.ringCenter, piece[deepest]))
                {
                    deepest = i;
                }
            }

            Assert::IsTrue (DistanceFrom (art.ringCenter, piece[deepest]) < art.ringInner - 1.0f, L"it reaches inside the ring");

            for (i = 0; i < piece.size(); i++)
            {
                Assert::IsTrue (std::abs (piece[deepest].y - art.ringCenter.y) <= std::abs (piece[i].y - art.ringCenter.y) + 0.01f,
                                L"and its deep end is the end beside the cardinal");
            }
        }
    }


    TEST_METHOD (TheFireButtonIsInTheTopLeftCornerClearOfTheRing)
    {
        JoyportStickArt  art = JoyportSwitchView::BuildArt (10.0f, 20.0f, kSide);

        Assert::IsTrue (art.fireCenter.x < 10.0f + kSide / 4.0f && art.fireCenter.y < 20.0f + kSide / 4.0f, L"top-left");
        Assert::IsTrue (DistanceFrom (art.ringCenter, art.fireCenter) - art.fireRadius > art.ringOuter, L"clear of the ring");
    }


    TEST_METHOD (EverythingFitsInsideTheSquare)
    {
        JoyportStickArt  art = JoyportSwitchView::BuildArt (10.0f, 20.0f, kSide);

        for (const DxuiPointF & point : art.body)
        {
            Assert::IsTrue (point.x >= 10.0f - 0.01f && point.x <= 10.0f + kSide + 0.01f);
            Assert::IsTrue (point.y >= 20.0f - 0.01f && point.y <= 20.0f + kSide + 0.01f);
        }

        Assert::IsTrue (art.ringCenter.x + art.ringOuter < 10.0f + kSide);
        Assert::IsTrue (art.ringCenter.y + art.ringOuter < 20.0f + kSide);
        Assert::IsTrue (art.shaftTravel + art.shaftRadius < art.bootRadius, L"a leaning stick stays on its boot");
    }
};
