#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoLink/FullSend/FullSendQuips.h"
#include "CassoLink/FullSend/FullSendScreen.h"
#include "CassoLink/Session/CassoLinkSessionTypes.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendScreenTests
//
//  The Apple's Full Send screen: its track map and the order of its
//  prompt lines.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FullSendScreenTests)
{
public:

    TEST_METHOD (ATrackMap_HasEachTracksWorstSector_OnTheRuler)
    {
        std::string                       map;
        std::vector<SectorMark>           marks      = { { 3, 1, 'R' }, { 3, 2, 'U' }, { 4, 0, 'U' }, { 4, 1, 'R' }, { 12, 0, 'R' } };
        std::vector<std::pair<int, int>>  unreadable = { { 7, 5 } };

        FullSendScreen::MakeTrackMap (map, marks, unreadable);

        Assert::AreEqual (std::string ("0..UU:.X..1.R..:....2....:....3...."), map);
    }

    TEST_METHOD (TheQuips_PickEveryLineOnce_ThenNeverRepeatTheLastAtOnce)
    {
        FullSendQuips  quips (1);
        Byte           last = 0;

        for (int round = 0; round < 50; round++)
        {
            std::set<Byte>  picked;

            for (int line = 0; line < FullSendScreen::kQuipCount; line++)
            {
                Byte  quip = quips.Pick();

                Assert::IsTrue (quip >= 1 && quip <= FullSendScreen::kQuipCount);
                Assert::IsTrue (line != 0 || quip != last, L"a round begins with a line other than the last one");

                picked.insert (quip);
                last = quip;
            }

            Assert::AreEqual ((size_t) FullSendScreen::kQuipCount, picked.size());
        }
    }
};
