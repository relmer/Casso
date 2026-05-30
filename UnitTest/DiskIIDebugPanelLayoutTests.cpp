#include "Pch.h"

#include "../Casso/Ui/DiskIIDebugPanelLayout.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIIDebugPanelLayoutTests
//
//  Spec-011 T046. Pure layout helper for the themed Disk II Debug
//  panel. Verifies slot positions, ordering, DPI scaling, and the
//  ListView's flex-fill behaviour against legacy 96-DPI metrics.
//
////////////////////////////////////////////////////////////////////////////////

namespace UnitTest_011
{
    TEST_CLASS (DiskIIDebugPanelLayoutTests)
    {
    public:

        TEST_METHOD (Default96Dpi_FirstEventCheckOriginsAtMargin)
        {
            PanelLayoutSlots s = ComputeDiskIIDebugPanelLayout (900, 600, 96);

            Assert::AreEqual ((LONG) 8, s.eventTypeChecks[0].left);
            Assert::AreEqual ((LONG) 8, s.eventTypeChecks[0].top);
            Assert::AreEqual ((LONG) 8 + 92, s.eventTypeChecks[0].right);
            Assert::AreEqual ((LONG) 8 + 22, s.eventTypeChecks[0].bottom);
        }

        TEST_METHOD (Default96Dpi_EventChecksContiguousAcrossTopRow)
        {
            PanelLayoutSlots s = ComputeDiskIIDebugPanelLayout (900, 600, 96);

            for (int i = 1; i < kEventTypeCheckCount; i++)
            {
                Assert::AreEqual (s.eventTypeChecks[i - 1].right, s.eventTypeChecks[i].left);
                Assert::AreEqual (s.eventTypeChecks[i - 1].top,   s.eventTypeChecks[i].top);
            }
        }

        TEST_METHOD (Default96Dpi_RowsStackInOrder)
        {
            PanelLayoutSlots s = ComputeDiskIIDebugPanelLayout (900, 600, 96);

            Assert::IsTrue (s.audioMasterCheck.top     >= s.eventTypeChecks[0].bottom);
            Assert::IsTrue (s.driveRadios[0].top       >= s.audioMasterCheck.bottom);
            Assert::IsTrue (s.rawQtCheck.top           >= s.driveRadios[0].bottom);
            Assert::IsTrue (s.trackInvalidLabel.top    >= s.rawQtCheck.bottom);
            Assert::IsTrue (s.pauseButton.top          >= s.trackInvalidLabel.bottom);
            Assert::IsTrue (s.listView.top             >= s.pauseButton.bottom);
        }

        TEST_METHOD (Default96Dpi_RawQtAlignedUnderTrackEdit)
        {
            PanelLayoutSlots s = ComputeDiskIIDebugPanelLayout (900, 600, 96);

            Assert::AreEqual (s.trackEdit.left, s.rawQtCheck.left);
        }

        TEST_METHOD (Default96Dpi_ButtonRowAtMarginLeft)
        {
            PanelLayoutSlots s = ComputeDiskIIDebugPanelLayout (900, 600, 96);

            Assert::AreEqual ((LONG) 8, s.pauseButton.left);
            Assert::AreEqual (s.pauseButton.right + 4, s.clearButton.left);
        }

        TEST_METHOD (Default96Dpi_ListViewFlexFillsRemainder)
        {
            PanelLayoutSlots s = ComputeDiskIIDebugPanelLayout (900, 600, 96);

            Assert::AreEqual ((LONG) 8,          s.listView.left);
            Assert::AreEqual ((LONG) 900 - 8,    s.listView.right);
            Assert::AreEqual ((LONG) 600 - 8,    s.listView.bottom);
            Assert::IsTrue   (s.listView.top    >  s.pauseButton.bottom);
            Assert::IsTrue   (s.listView.bottom >  s.listView.top);
        }

        TEST_METHOD (Dpi192_DoublesAllMetrics)
        {
            PanelLayoutSlots s96  = ComputeDiskIIDebugPanelLayout (900, 600, 96);
            PanelLayoutSlots s192 = ComputeDiskIIDebugPanelLayout (900, 600, 192);

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
            PanelLayoutSlots s = ComputeDiskIIDebugPanelLayout (10, 10, 96);

            Assert::IsTrue (s.listView.right  > s.listView.left);
            Assert::IsTrue (s.listView.bottom > s.listView.top);
        }

        TEST_METHOD (DriveRadiosContiguousLeftToRight)
        {
            PanelLayoutSlots s = ComputeDiskIIDebugPanelLayout (900, 600, 96);

            for (int i = 1; i < kDriveRadioCount; i++)
            {
                Assert::AreEqual (s.driveRadios[i - 1].right, s.driveRadios[i].left);
                Assert::AreEqual (s.driveRadios[i - 1].top,   s.driveRadios[i].top);
            }
        }

        TEST_METHOD (AudioSubChecksAfterMaster)
        {
            PanelLayoutSlots s = ComputeDiskIIDebugPanelLayout (900, 600, 96);

            Assert::AreEqual (s.audioMasterCheck.right, s.audioSubChecks[0].left);
            for (int i = 1; i < kAudioSubCheckCount; i++)
            {
                Assert::AreEqual (s.audioSubChecks[i - 1].right, s.audioSubChecks[i].left);
            }
        }
    };
}
