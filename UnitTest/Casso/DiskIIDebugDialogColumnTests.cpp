#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "DiskIIDebugDialogState.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIIDebugDialogColumnTests
//
//  Spec-006 T108. Headless coverage of the PlanVisibleColumns pure
//  helper (FR-026 / FR-027) -- given an in-memory LogicalColumn model,
//  the planner emits the ordered list of VisibleColumnSpecs the
//  ListView should hold. Exercises the model semantics that
//  RebuildListViewColumns and ToggleColumn rely on without
//  instantiating a real Win32 ListView.
//
////////////////////////////////////////////////////////////////////////////////

namespace DiskIIDebugDialogColumnTests
{
    TEST_CLASS (DiskIIDebugDialogColumnTests)
    {
    public:

        TEST_METHOD (PlanVisibleColumns_allVisibleNoneAutoSized_emitsSixSpecsDefaults)
        {
            std::array<LogicalColumn, kColumnCount>  model = {};

            SeedDefaultColumns (model);

            std::vector<VisibleColumnSpec>  plan = PlanVisibleColumns (model);

            Assert::AreEqual (static_cast<size_t> (kColumnCount), plan.size ());

            for (int i = 0; i < kColumnCount; i++)
            {
                Assert::AreEqual (i,                       plan[i].id);
                Assert::AreEqual (model[i].defaultWidth,   plan[i].width);
                Assert::IsTrue   (plan[i].needsAutoSize);
                Assert::IsNotNull (plan[i].headerText);
            }
        }



        TEST_METHOD (PlanVisibleColumns_oneHidden_skipsThatColumn)
        {
            std::array<LogicalColumn, kColumnCount>  model = {};

            SeedDefaultColumns (model);
            model[2].visible = false;

            std::vector<VisibleColumnSpec>  plan = PlanVisibleColumns (model);

            Assert::AreEqual (static_cast<size_t> (kColumnCount - 1), plan.size ());
            Assert::AreEqual (0, plan[0].id);
            Assert::AreEqual (1, plan[1].id);
            Assert::AreEqual (3, plan[2].id);
            Assert::AreEqual (4, plan[3].id);
            Assert::AreEqual (5, plan[4].id);
        }



        TEST_METHOD (PlanVisibleColumns_allHidden_emitsEmptyVector)
        {
            std::array<LogicalColumn, kColumnCount>  model = {};
            int                                      i     = 0;

            SeedDefaultColumns (model);

            for (i = 0; i < kColumnCount; i++)
            {
                model[i].visible = false;
            }

            std::vector<VisibleColumnSpec>  plan = PlanVisibleColumns (model);

            Assert::IsTrue (plan.empty ());
        }



        TEST_METHOD (PlanVisibleColumns_userDraggedDetail_emitsCustomWidth)
        {
            constexpr int                            kUserDraggedDetailWidth = 500;
            std::array<LogicalColumn, kColumnCount>  model                   = {};

            SeedDefaultColumns (model);
            model[kColumnCount - 1].savedWidth   = kUserDraggedDetailWidth;
            model[kColumnCount - 1].autoSizedYet = true;

            std::vector<VisibleColumnSpec>  plan = PlanVisibleColumns (model);

            Assert::AreEqual (static_cast<size_t> (kColumnCount),    plan.size ());
            Assert::AreEqual (kUserDraggedDetailWidth,               plan[kColumnCount - 1].width);
            Assert::IsFalse  (plan[kColumnCount - 1].needsAutoSize);
        }



        TEST_METHOD (PlanVisibleColumns_autoSizedYetTrue_preservesSavedWidth)
        {
            constexpr int                            kReshownColumnSavedWidth = 137;
            std::array<LogicalColumn, kColumnCount>  model                    = {};

            SeedDefaultColumns (model);
            model[1].savedWidth   = kReshownColumnSavedWidth;
            model[1].autoSizedYet = true;

            std::vector<VisibleColumnSpec>  plan = PlanVisibleColumns (model);

            Assert::AreEqual (1,                          plan[1].id);
            Assert::AreEqual (kReshownColumnSavedWidth,   plan[1].width);
            Assert::IsFalse  (plan[1].needsAutoSize);
        }
    };
}
