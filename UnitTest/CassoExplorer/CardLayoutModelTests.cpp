#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoExplorer/Model/CardLayoutModel.h"
#include "CassoExplorer/CardCompatibilityGrid.h"
#include "Seams/RawDevice/RawDeviceProtocol.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





TEST_CLASS (CardLayoutModelTests)
{
public:

    static constexpr uint64_t  kCard = 976ull * 1024 * 1024;

    TEST_METHOD (Positions_AreTheVolumeCountsThenFat32)
    {
        CardLayoutModel  model (kCard, 13, true);

        Assert::AreEqual (14, model.GetPositionCount());
        Assert::AreEqual (4,  model.GetDefaultPosition());
        Assert::IsFalse  (model.IsFat32 (13));
        Assert::IsTrue   (model.IsFat32 (14));
        Assert::AreEqual ((uint64_t) 7, model.GetLayout (7));
        Assert::AreEqual (RawDeviceProtocol::kLayoutFat32, model.GetLayout (14));
    }


    TEST_METHOD (Positions_LeaveOutFat32ForACardTooSmall)
    {
        CardLayoutModel  model (32ull * 1024 * 1024, 1, false);

        Assert::AreEqual (1, model.GetPositionCount());
        Assert::AreEqual (1, model.GetDefaultPosition());
        Assert::AreEqual (std::wstring (L"One ProDOS volume of the whole card"), model.DescribePosition (1));
    }


    TEST_METHOD (Slots_AreUniversalThenNeedSetupThenUnused)
    {
        CardLayoutModel  model (kCard, 13, true);

        Assert::IsTrue (model.GetSlotKind (6, 4)  == CardLayoutModel::Segment::Universal);
        Assert::IsTrue (model.GetSlotKind (6, 5)  == CardLayoutModel::Segment::NeedsSetup);
        Assert::IsTrue (model.GetSlotKind (6, 7)  == CardLayoutModel::Segment::Unused);
        Assert::IsTrue (model.GetSlotKind (14, 1) == CardLayoutModel::Segment::Fat32);
        Assert::AreEqual (kCard - 13ull * CardLayoutModel::kSliceBytes, model.GetUnavailableBytes());
    }


    //  Every CFFA adapter. The A2Pico doesn't read a CFFA layout.
    TEST_METHOD (Rows_AllCffaWorkUpToFourVolumes)
    {
        CardLayoutModel                    model (kCard, 13, true);
        std::vector<CardLayoutModel::Row>  rows;

        model.GetRows (2, rows);

        for (const CardLayoutModel::Row & row : rows)
        {
            bool  isCffa = row.adapter.starts_with (L"CFFA");

            Assert::IsTrue (!isCffa || row.support == CardLayoutModel::Support::Works || row.support == CardLayoutModel::Support::Unknown, row.adapter.c_str());
            Assert::IsTrue (isCffa  || row.support == CardLayoutModel::Support::Fails, row.adapter.c_str());
        }
    }


    TEST_METHOD (Rows_ThirteenVolumesFailTheGsosDriverAndNeedSetupOnCffa2)
    {
        CardLayoutModel                    model (kCard, 13, true);
        std::vector<CardLayoutModel::Row>  rows;

        model.GetRows (13, rows);

        Assert::IsTrue (rows[2].support == CardLayoutModel::Support::Fails);
        Assert::IsTrue (rows[3].support == CardLayoutModel::Support::Step);
        Assert::IsTrue (rows[3].note.find (L"13") != std::wstring::npos);
    }


    TEST_METHOD (Text_NamesASingleVolumeWithoutARange)
    {
        CardLayoutModel                    model (kCard, 13, true);
        std::vector<CardLayoutModel::Row>  rows;

        model.GetRows (5, rows);

        Assert::IsTrue (rows[2].note.find (L"to 5") == std::wstring::npos);
        Assert::IsTrue (model.DescribeSpace (6).starts_with (L"Uses "));
        Assert::IsTrue (model.DescribeSpace (6).ends_with (L"unused and inaccessible."));
        Assert::IsTrue (model.DescribeSpace (14).starts_with (L"Creates a single "));
    }


    TEST_METHOD (Choices_OfferFat32FirstForACffa3000Only)
    {
        CardLayoutModel   model (kCard, 13, true);
        std::vector<int>  positions;

        model.GetChoices (CardLayoutModel::Adapter::Cffa3000, positions);
        Assert::AreEqual ((size_t) 14, positions.size());
        Assert::IsTrue (model.IsFat32 (positions[0]));
        Assert::AreEqual (positions[0], model.GetRecommendedPosition (CardLayoutModel::Adapter::Cffa3000));

        model.GetChoices (CardLayoutModel::Adapter::Cffa2Enhanced, positions);
        Assert::AreEqual ((size_t) 13, positions.size());
        Assert::AreEqual (1, positions[0]);
    }


    TEST_METHOD (Recommended_FitsEachAdapter)
    {
        CardLayoutModel  model (kCard, 13, true);

        Assert::AreEqual (4,  model.GetRecommendedPosition (CardLayoutModel::Adapter::Unsure));
        Assert::AreEqual (4,  model.GetRecommendedPosition (CardLayoutModel::Adapter::Cffa1Gsos));
        Assert::AreEqual (8,  model.GetRecommendedPosition (CardLayoutModel::Adapter::Cffa12EightDrive));
        Assert::AreEqual (13, model.GetRecommendedPosition (CardLayoutModel::Adapter::Cffa2Enhanced));
        Assert::AreEqual (2,  model.GetRecommendedPosition (CardLayoutModel::Adapter::Cffa2Original));
    }


    TEST_METHOD (Rows_ShowOnlyTheChosenAdapter)
    {
        CardLayoutModel                    model (kCard, 13, true);
        std::vector<CardLayoutModel::Row>  rows;

        model.GetRows (13, CardLayoutModel::Adapter::Cffa2Enhanced, rows);
        Assert::AreEqual ((size_t) 1, rows.size());
        Assert::IsTrue (rows[0].support == CardLayoutModel::Support::Step);

        model.GetRows (6, CardLayoutModel::Adapter::Cffa1Gsos, rows);
        Assert::AreEqual ((size_t) 2, rows.size());

        model.GetRows (6, CardLayoutModel::Adapter::Unsure, rows);
        Assert::AreEqual ((size_t) 7, rows.size());

        model.GetRows (6, CardLayoutModel::Adapter::A2Pico, rows);
        Assert::AreEqual ((size_t) 1, rows.size());
        Assert::IsTrue (rows[0].support == CardLayoutModel::Support::Fails, L"an A2Pico reads a FAT volume only");
    }


    TEST_METHOD (Grid_IsTallEnoughForEveryRow)
    {
        CardLayoutModel        model (kCard, 13, true);
        CardCompatibilityGrid  grid (model);
        DxuiDpiScaler          scaler;

        scaler.SetDpi (144);
        grid.SetChoice (4, CardLayoutModel::Adapter::Unsure);

        Logger::WriteMessage (std::format (L"grid height {}", grid.MeasureHeightPx (600, scaler)).c_str());
        Assert::IsTrue (grid.MeasureHeightPx (600, scaler) > 6 * 36);
    }


    TEST_METHOD (Rows_Fat32WorksOnTheCffa3000AndTheA2Pico)
    {
        CardLayoutModel                    model (kCard, 13, true);
        std::vector<CardLayoutModel::Row>  rows;

        model.GetRows (14, rows);

        Assert::AreEqual ((size_t) 4, rows.size());
        Assert::IsTrue (rows[0].support == CardLayoutModel::Support::Fails);
        Assert::IsTrue (rows[2].support == CardLayoutModel::Support::Works);
        Assert::IsTrue (rows[3].support == CardLayoutModel::Support::Works);

        model.GetRows (14, CardLayoutModel::Adapter::A2Pico, rows);
        Assert::AreEqual ((size_t) 1, rows.size());
        Assert::AreEqual (std::wstring (L"A2Pico, A2retroNET"), rows[0].adapter);
    }


    //  An A2Pico reads disk images from one FAT volume, so FAT32 is its only
    //  layout. A card too small for FAT32 has no layout for it.
    TEST_METHOD (Choices_OfferOnlyFat32ForAnA2Pico)
    {
        CardLayoutModel   model (kCard, 13, true);
        CardLayoutModel   tiny  (16ull * 1024 * 1024, 1, false);
        std::vector<int>  positions;

        model.GetChoices (CardLayoutModel::Adapter::A2Pico, positions);
        Assert::AreEqual ((size_t) 1, positions.size());
        Assert::IsTrue   (model.IsFat32 (positions[0]));
        Assert::AreEqual (positions[0], model.GetRecommendedPosition (CardLayoutModel::Adapter::A2Pico));
        Assert::AreEqual (std::wstring (L"A2Pico with the A2retroNET firmware"), CardLayoutModel::GetAdapterLabel (CardLayoutModel::Adapter::A2Pico));

        tiny.GetChoices (CardLayoutModel::Adapter::A2Pico, positions);
        Assert::IsTrue (positions.empty());
    }
};