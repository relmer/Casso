#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../EmuTests/PartitionCard.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Model/BrowserModel.h"
#include "CassoExplorer/Model/DiskMessageText.h"
#include "CassoExplorer/Model/TreeModel.h"
#include "Devices/Disk/Archives/ContainerDiskFileIo.h"
#include "Devices/Disk/Partitions/PartitionAccess.h"
#include "CassoExplorer/Browser/BrowserRowPaths.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPartitionTests
//
//  A MicroDrive card image in a folder of the PC: its partitions as tree
//  nodes and as list rows, the address of an open partition, the partition
//  commands, a partition copied to an image file, and a partition replaced
//  from an image file.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ExplorerPartitionTests)
{
public:

    static constexpr const wchar_t *  kSecondPath  = L"C:\\Disks\\card.po|#2.po";
    static constexpr const wchar_t *  kDamagedPath = L"C:\\Disks\\card.po|#3.po";
    static constexpr const wchar_t *  kNewPath     = L"C:\\Disks\\new.po";
    static constexpr const char *     kNewName     = "C:\\Disks\\new.po";


    struct CardHost
    {
        InMemoryFileSystem    fs;
        FakeDiskFileIo        io;
        ContainerDiskFileIo   nested  { io };
        CassoExplorerBrowser  browser { fs, nested };
        CassoExplorerActions  actions { browser, fs };
        std::vector<Byte>     card    = PartitionCard::MakeCard();

        CardHost()
        {
            Seed (PartitionCard::kCardPath, PartitionCard::kCardName, card);
        }

        void Seed (const wchar_t * path, const char * name, const std::vector<Byte> & bytes)
        {
            AssertSucceeded (fs.WriteAllText (path, std::string (bytes.begin(), bytes.end())));

            io.files[name]  = bytes;
            io.stamps[name] = FileStamp { bytes.size(), 100 };
        }

        void OpenCard()
        {
            browser.GetNavigator().NavigateToLocation (Location::MakeDiskImage (PartitionCard::kCardPath));
        }

        bool HasVerb (ActionsTypes::Verb verb)
        {
            std::vector<ActionsTypes::Verb>  verbs = actions.GetListVerbs();

            return std::find (verbs.begin(), verbs.end(), verb) != verbs.end();
        }
    };


    static bool HasRow (const CassoExplorerBrowser & browser, const wchar_t * name)
    {
        for (const CatalogRow & row : browser.GetRows())
        {
            if (row.name == name)
            {
                return true;
            }
        }

        return false;
    }


    TEST_METHOD (Label_HasTheIndexTheVolumeNameAndTheSize)
    {
        std::vector<PartitionEntry>  entries;
        PartitionLayoutKind          kind = PartitionLayoutKind::None;

        AssertSucceeded (PartitionAccess::List (PartitionCard::MakeCard(), PartitionCard::kCardName, kind, entries));

        Assert::AreEqual (std::string ("Partition 2: SECOND"), PartitionAccess::GetLabel (entries[1], false));
        Assert::AreEqual (std::string ("Partition 2: SECOND (") + PartitionAccess::FormatSize (PartitionCard::kSecondBytes) + ")",
                          PartitionAccess::GetLabel (entries[1], true));
    }


    //  Up from a partition's volume goes to its partition, and from a member
    //  of an archive to the folder.
    TEST_METHOD (Up_PartitionVolumeGoesToItsPartition)
    {
        Assert::IsTrue  (BrowserRowPaths::IsPartitionVolume (L"C:\\walk\\MultiPart.hdv|#1.po"));
        Assert::IsTrue  (BrowserRowPaths::IsPartitionVolume (L"C:\\walk\\MultiPart.hdv|#1.po|#3.do"));
        Assert::IsFalse (BrowserRowPaths::IsPartitionVolume (L"C:\\walk\\Masters.zip|Master.dsk"));
        Assert::IsFalse (BrowserRowPaths::IsPartitionVolume (L"C:\\walk\\MultiPart.hdv"));
    }


    //  Each partition is a node, and the volume in it is its child.
    TEST_METHOD (Tree_CardHasOneNodeForEachPartition)
    {
        CardHost                   host;
        std::vector<DxuiTreeNode>  nodes   = host.browser.GetTreeChildren (TreeModel::MakeImageId (false, PartitionCard::kCardPath));
        std::vector<DxuiTreeNode>  volumes;

        Assert::AreEqual ((size_t) 3, nodes.size());
        Assert::AreEqual (std::wstring (L"Partition 1"), nodes[0].label);
        Assert::AreEqual (std::wstring (L"Partition 2"), nodes[1].label);
        Assert::IsFalse (nodes[1].iconBroken);
        Assert::IsTrue (nodes[2].iconBroken, L"The third partition ends past the end of the card");

        volumes = host.browser.GetTreeChildren (nodes[1].id);

        Assert::AreEqual ((size_t) 1, volumes.size());
        Assert::AreEqual (std::wstring (L"SECOND"), volumes[0].label);
        Assert::AreEqual (TreeModel::MakeImageId (false, kSecondPath), volumes[0].id);
    }


    TEST_METHOD (Address_OpenPartitionHasASegmentAfterTheCard)
    {
        std::vector<BrowserModel::AddressSegment>  segments = BrowserModel::GetAddressSegments (Location::MakeDiskImage (kSecondPath));

        Assert::IsTrue (segments.size() >= 2);
        Assert::AreEqual (std::wstring (L"Partition 2"), segments.back().label);
        Assert::AreEqual (std::wstring (L"card.po"), segments[segments.size() - 2].label);
        Assert::AreEqual (std::wstring (kSecondPath), segments.back().location.path);
    }


    TEST_METHOD (List_CardHasOneRowForEachPartition)
    {
        CardHost  host;
        Location  target;

        host.OpenCard();

        Assert::IsTrue (host.browser.IsPartitionListLocation());
        Assert::AreEqual ((size_t) 3, host.browser.GetRows().size());
        Assert::AreEqual (std::wstring (L"Partition 2"), host.browser.GetRows()[1].name);
        Assert::AreEqual (std::wstring (L"MicroDrive partition"), host.browser.GetRows()[1].typeText);
        Assert::AreEqual (std::wstring (L"MicroDrive partition, damaged"), host.browser.GetRows()[2].typeText);
        Assert::AreEqual ((uint64_t) PartitionCard::kSecondBytes, (uint64_t) host.browser.GetRows()[1].sizeBytes);

        //  A partition opens as the list of its volume, and the volume as its
        //  catalog.
        Assert::IsTrue (BrowserRowPaths::TryGetRowLocation (host.browser.GetRowListing(), 1, target));
        Assert::IsTrue (target.kind == Location::Kind::Partition);
        Assert::AreEqual (std::wstring (kSecondPath), target.path);

        host.browser.GetNavigator().NavigateToLocation (target);

        Assert::AreEqual ((size_t) 1, host.browser.GetRows().size());
        Assert::AreEqual (std::wstring (L"SECOND"), host.browser.GetRows()[0].name);
        Assert::AreEqual (std::wstring (L"ProDOS volume"), host.browser.GetRows()[0].typeText);
        Assert::IsTrue (BrowserRowPaths::TryGetRowLocation (host.browser.GetRowListing(), 0, target));
        Assert::IsTrue (target.kind == Location::Kind::DiskImage);
        Assert::AreEqual (std::wstring (kSecondPath), target.path);
    }


    TEST_METHOD (List_PartitionRowOpensItsVolume)
    {
        CardHost  host;

        host.OpenCard();

        Assert::IsTrue (host.browser.OpenRow (1));
        Assert::IsTrue (host.browser.GetLocation().kind == Location::Kind::Partition);
        Assert::IsTrue (host.browser.OpenRow (0));
        Assert::IsFalse (host.browser.IsPartitionListLocation());
        Assert::IsTrue (HasRow (host.browser, L"HELLO"));
    }


    TEST_METHOD (List_DamagedPartitionOpensWithItsReason)
    {
        CardHost  host;

        host.browser.GetNavigator().NavigateToLocation (Location::MakeDiskImage (kDamagedPath));

        Assert::IsTrue (host.browser.GetRows().empty());
        Assert::IsFalse (host.browser.GetListError().empty());
    }


    TEST_METHOD (Problem_DamagedPartitionHasTheDamagedReason)
    {
        CardHost      host;
        std::wstring  problem = host.browser.GetImageProblem (kDamagedPath);

        Assert::IsTrue (problem.ends_with (DiskMessageText::kDamagedPartitionText), problem.c_str());
        Assert::IsTrue (problem.find (L'|') == std::wstring::npos, problem.c_str());
    }


    TEST_METHOD (List_DamagedPartitionRowOpensWithTheTreesReason)
    {
        CardHost  host;
        Location  target;

        host.OpenCard();
        Assert::IsTrue (BrowserRowPaths::TryGetRowLocation (host.browser.GetRowListing(), 2, target));

        host.browser.GetNavigator().NavigateToLocation (target);

        Assert::IsTrue (host.browser.GetRows().empty());
        Assert::IsTrue (host.browser.GetListError().ends_with (DiskMessageText::kDamagedPartitionText), host.browser.GetListError().c_str());
    }


    TEST_METHOD (Verbs_PartitionRowHasCopyAndReplace)
    {
        CardHost      host;
        std::wstring  partition;

        host.OpenCard();
        host.browser.GetSelection().SetSelectedRows ({ 1 });

        Assert::IsTrue (host.actions.TryGetPartition (partition));
        Assert::AreEqual (std::wstring (kSecondPath), partition);
        Assert::IsTrue (host.HasVerb (ActionsTypes::Verb::Open));
        Assert::IsTrue (host.HasVerb (ActionsTypes::Verb::CopyPartitionToImage));
        Assert::IsTrue (host.HasVerb (ActionsTypes::Verb::ReplacePartition));
        Assert::IsTrue (host.HasVerb (ActionsTypes::Verb::InspectDiskImage), L"a partition opens in the disk inspector");
        Assert::IsTrue (host.actions.CanInspect());
    }


    //  With no partition selected, the card image is what Inspect opens.
    TEST_METHOD (CanInspect_TheOpenCardWithNoPartitionSelected)
    {
        CardHost  host;

        host.OpenCard();

        Assert::IsTrue (host.browser.IsPartitionListLocation());
        Assert::IsTrue (host.browser.GetSelectedRows().empty());
        Assert::IsTrue (host.actions.CanInspect(), L"the card image itself");
    }


    TEST_METHOD (Verbs_OpenPartitionHasCopyAndReplaceAndNoFormat)
    {
        CardHost      host;
        std::wstring  partition;

        host.browser.GetNavigator().NavigateToLocation (Location::MakeDiskImage (kSecondPath));

        Assert::IsTrue (host.actions.TryGetPartition (partition));
        Assert::AreEqual (std::wstring (kSecondPath), partition);
        Assert::IsTrue (host.HasVerb (ActionsTypes::Verb::CopyPartitionToImage));
        Assert::IsTrue (host.HasVerb (ActionsTypes::Verb::ReplacePartition));
        Assert::IsFalse (host.HasVerb (ActionsTypes::Verb::Format));
    }


    TEST_METHOD (Verbs_FileOfAPartitionIsNotAPartition)
    {
        CardHost      host;
        std::wstring  partition;

        host.OpenCard();

        Assert::IsFalse (host.actions.TryGetPartition (partition), L"No row is selected");
        Assert::IsFalse (host.HasVerb (ActionsTypes::Verb::CopyPartitionToImage));
    }


    TEST_METHOD (Copy_WritesThePartitionBesideTheCard)
    {
        CardHost               host;
        ActionsTypes::Outcome  outcome;
        std::wstring           created;

        outcome = host.actions.CopyPartitionToImage (kSecondPath, created);

        AssertSucceeded (outcome.hr);
        Assert::AreEqual (std::wstring (L"C:\\Disks\\card partition 2.po"), created);
        Assert::IsTrue (host.io.files["C:\\Disks\\card partition 2.po"] == PartitionCard::GetSecond (host.card));
        Assert::IsTrue (host.io.files[PartitionCard::kCardName] == host.card);
    }


    TEST_METHOD (Copy_DamagedPartitionFailsWithAMessage)
    {
        CardHost               host;
        ActionsTypes::Outcome  outcome;
        std::wstring           created;

        outcome = host.actions.CopyPartitionToImage (kDamagedPath, created);

        AssertFailed (outcome.hr);
        Assert::IsTrue (created.empty());
        Assert::IsFalse (outcome.message.empty());
    }


    TEST_METHOD (Replace_QuestionHasThePartitionTheCardAndTheImage)
    {
        CardHost               host;
        ActionsTypes::Outcome  outcome;
        std::wstring           question;

        host.Seed (kNewPath, kNewName, PartitionCard::MakeProDos (PartitionCard::kSecondBlocks, "FRESH"));

        outcome = host.actions.DescribePartitionReplace (kSecondPath, kNewPath, question);

        AssertSucceeded (outcome.hr);
        Assert::IsTrue (question.starts_with (L"Replace Partition 2: SECOND ("));
        Assert::IsTrue (question.find (L" of card.po with new.po (") != std::wstring::npos);
        Assert::IsTrue (question.ends_with (L"Everything on the partition is overwritten."));
    }


    TEST_METHOD (Replace_ChangesThatPartitionOnly)
    {
        CardHost               host;
        ActionsTypes::Outcome  outcome;
        std::vector<Byte>      fresh    = PartitionCard::MakeProDos (PartitionCard::kSecondBlocks, "FRESH");
        std::vector<Byte>      replaced = host.card;

        host.Seed (kNewPath, kNewName, fresh);
        PartitionCard::Place (replaced, PartitionCard::kSecondStart, fresh);

        outcome = host.actions.ReplacePartition (kSecondPath, kNewPath);

        AssertSucceeded (outcome.hr);
        Assert::IsTrue (host.io.files[PartitionCard::kCardName] == replaced);

        host.browser.GetNavigator().NavigateToLocation (Location::MakePartition (kSecondPath));

        Assert::AreEqual (std::wstring (L"FRESH"), host.browser.GetRows()[0].name);
    }


    TEST_METHOD (Replace_ImageLargerThanThePartitionFails)
    {
        CardHost               host;
        ActionsTypes::Outcome  outcome;
        std::wstring           question;

        host.Seed (kNewPath, kNewName, PartitionCard::MakeProDos (PartitionCard::kCardBlocks, "LARGE"));

        outcome = host.actions.DescribePartitionReplace (kSecondPath, kNewPath, question);

        AssertFailed (outcome.hr);
        Assert::IsTrue (question.empty());

        outcome = host.actions.ReplacePartition (kSecondPath, kNewPath);

        AssertFailed (outcome.hr);
        Assert::IsFalse (outcome.message.empty());
        Assert::IsTrue (host.io.files[PartitionCard::kCardName] == host.card);
    }
};
