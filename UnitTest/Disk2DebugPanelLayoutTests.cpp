#include "Pch.h"

#include "../Casso/Ui/Disk2DebugPanelLayout.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2DebugPanelLayoutTests
//
//  Spec-011 T046. Pure layout helper for the themed Disk II Debug
//  panel. Verifies slot positions, ordering, DPI scaling, and the
//  ListView's flex-fill behaviour against legacy 96-DPI metrics.
//
////////////////////////////////////////////////////////////////////////////////

namespace UnitTest_011
{
    TEST_CLASS (Disk2DebugPanelLayoutTests)
    {
    public:

        TEST_METHOD (Default96Dpi_FirstEventCheckOriginsAfterLabel)
        {
            PanelLayoutSlots s = ComputeDisk2DebugPanelLayout (900, 600, 0, 96);

            Assert::AreEqual ((LONG) 8, s.diskEventsLabel.left);
            Assert::AreEqual ((LONG) 8, s.diskEventsLabel.top);
            Assert::AreEqual (s.diskEventsLabel.right + 4, s.eventTypeChecks[0].left);
            Assert::AreEqual ((LONG) 8, s.eventTypeChecks[0].top);
            Assert::AreEqual ((LONG) 22, s.eventTypeChecks[0].bottom - s.eventTypeChecks[0].top);
        }

        TEST_METHOD (Default96Dpi_DiskAndAudioRowsAlignInColumns)
        {
            PanelLayoutSlots s = ComputeDisk2DebugPanelLayout (900, 600, 0, 96);

            Assert::AreEqual (s.diskEventsLabel.left,   s.audioEventsLabel.left);
            Assert::AreEqual (s.diskEventsLabel.right,  s.audioEventsLabel.right);
            Assert::AreEqual (s.eventTypeChecks[0].left, s.audioMasterCheck.left);
            Assert::AreEqual (s.eventTypeChecks[1].left, s.audioSubChecks[0].left);
            Assert::AreEqual (s.eventTypeChecks[2].left, s.audioSubChecks[1].left);
            Assert::AreEqual (s.eventTypeChecks[3].left, s.audioSubChecks[2].left);
            Assert::AreEqual (s.eventTypeChecks[4].left, s.audioSubChecks[3].left);
        }

        TEST_METHOD (Default96Dpi_EventChecksContiguousAcrossTopRow)
        {
            PanelLayoutSlots s = ComputeDisk2DebugPanelLayout (900, 600, 0, 96);

            for (int i = 1; i < kEventTypeCheckCount; i++)
            {
                Assert::AreEqual (s.eventTypeChecks[i - 1].right, s.eventTypeChecks[i].left);
                Assert::AreEqual (s.eventTypeChecks[i - 1].top,   s.eventTypeChecks[i].top);
            }
        }

        TEST_METHOD (Default96Dpi_RowsStackInOrder)
        {
            PanelLayoutSlots s = ComputeDisk2DebugPanelLayout (900, 600, 0, 96);

            Assert::IsTrue (s.audioMasterCheck.top     >= s.eventTypeChecks[0].bottom);
            Assert::IsTrue (s.driveRadios[0].top       >= s.audioMasterCheck.bottom);
            Assert::IsTrue (s.rawQtCheck.top           >= s.driveRadios[0].bottom);
            Assert::IsTrue (s.trackInvalidLabel.top    >= s.rawQtCheck.bottom);
            Assert::IsTrue (s.pauseButton.top          >= s.trackInvalidLabel.bottom);
            Assert::IsTrue (s.listView.top             >= s.pauseButton.bottom);
        }

        TEST_METHOD (Default96Dpi_RawQtAlignedUnderTrackEdit)
        {
            PanelLayoutSlots s = ComputeDisk2DebugPanelLayout (900, 600, 0, 96);

            Assert::AreEqual (s.trackEdit.left, s.rawQtCheck.left);
        }

        TEST_METHOD (Default96Dpi_ButtonRowAtMarginLeft)
        {
            PanelLayoutSlots s = ComputeDisk2DebugPanelLayout (900, 600, 0, 96);

            Assert::AreEqual ((LONG) 8, s.pauseButton.left);
            Assert::AreEqual (s.pauseButton.right + 4, s.clearButton.left);
        }

        TEST_METHOD (Default96Dpi_ListViewFlexFillsRemainder)
        {
            PanelLayoutSlots s = ComputeDisk2DebugPanelLayout (900, 600, 0, 96);

            Assert::AreEqual ((LONG) 8,          s.listView.left);
            Assert::AreEqual ((LONG) 900 - 8,    s.listView.right);
            Assert::AreEqual ((LONG) 600 - 8,    s.listView.bottom);
            Assert::IsTrue   (s.listView.top    >  s.pauseButton.bottom);
            Assert::IsTrue   (s.listView.bottom >  s.listView.top);
        }

        TEST_METHOD (Dpi192_DoublesAllMetrics)
        {
            PanelLayoutSlots s96  = ComputeDisk2DebugPanelLayout (900, 600, 0, 96);
            PanelLayoutSlots s192 = ComputeDisk2DebugPanelLayout (900, 600, 0, 192);

            // Widths/heights of a checkbox double.
            LONG w96  = s96.eventTypeChecks[0].right  - s96.eventTypeChecks[0].left;
            LONG h96  = s96.eventTypeChecks[0].bottom - s96.eventTypeChecks[0].top;
            LONG w192 = s192.eventTypeChecks[0].right  - s192.eventTypeChecks[0].left;
            LONG h192 = s192.eventTypeChecks[0].bottom - s192.eventTypeChecks[0].top;
            Assert::AreEqual (w96 * 2, w192);
            Assert::AreEqual (h96 * 2, h192);
        }

        TEST_METHOD (Degenerate_TinyClient_ListViewClampsToOne)
        {
            PanelLayoutSlots s = ComputeDisk2DebugPanelLayout (10, 10, 0, 96);

            Assert::IsTrue (s.listView.right  > s.listView.left);
            Assert::IsTrue (s.listView.bottom > s.listView.top);
        }

        TEST_METHOD (DriveRadiosAlignUnderCheckboxColumns)
        {
            PanelLayoutSlots s = ComputeDisk2DebugPanelLayout (900, 600, 0, 96);



            // The drive radios (All / Drive 1 / Drive 2) share the checkbox
            // rows' column pitch and first-column origin, so each sits
            // directly under the checkbox column above it.
            for (int i = 0; i < kDriveRadioCount; i++)
            {
                Assert::AreEqual (s.eventTypeChecks[i].left, s.driveRadios[i].left);
                Assert::AreEqual (s.driveRadios[0].top,      s.driveRadios[i].top);
            }
        }

        TEST_METHOD (AudioSubChecksAfterMaster)
        {
            PanelLayoutSlots s = ComputeDisk2DebugPanelLayout (900, 600, 0, 96);

            Assert::AreEqual (s.audioMasterCheck.right, s.audioSubChecks[0].left);
            for (int i = 1; i < kAudioSubCheckCount; i++)
            {
                Assert::AreEqual (s.audioSubChecks[i - 1].right, s.audioSubChecks[i].left);
            }
        }

        TEST_METHOD (TopOffsetShiftsAllSlots)
        {
            PanelLayoutSlots s0   = ComputeDisk2DebugPanelLayout (900, 600,  0, 96);
            PanelLayoutSlots s40  = ComputeDisk2DebugPanelLayout (900, 600, 40, 96);

            Assert::AreEqual (s0.eventTypeChecks[0].top + 40, s40.eventTypeChecks[0].top);
            Assert::AreEqual (s0.pauseButton.top         + 40, s40.pauseButton.top);
            Assert::AreEqual (s0.listView.top            + 40, s40.listView.top);
        }
    };
}
