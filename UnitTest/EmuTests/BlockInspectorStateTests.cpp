#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorState.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorStateTests
//
//  Block mode's title, edit bar and status band from a model over a small
//  ProDOS image in a fake file system.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BlockInspectorStateTests)
{
public:

    static constexpr size_t    kBlockBytes = 512;
    static constexpr size_t    kBlocks     = 280;
    static constexpr int64_t   kTime       = 1'700'000'000;
    static constexpr uint32_t  kEdited     = 2;

    static void OpenModel (FakeDiskFileIo & io, BlockInspectorModel & model)
    {
        vector<Byte>                     buffer (kBlocks * kBlockBytes, 0);
        std::unique_ptr<BlockImageFile>  file;
        std::wstring                     error;



        AssertSucceeded (ProDosSkeleton::Write (buffer, "STATE"));

        io.files["vol.po"]  = buffer;
        io.stamps["vol.po"] = { buffer.size(), kTime };

        AssertSucceeded (BlockImageFile::Open (io, "vol.po", file, error));
        AssertSucceeded (model.Open (std::move (file)));
    }


    static void EditOneByte (BlockInspectorModel & model)
    {
        int  overflow = 0;



        model.SelectBlock (kEdited);
        Assert::AreEqual (1, model.Paste (0, L"A5", false, overflow));
    }


    TEST_METHOD (Title_HasTheNameAndTheNoteWithNoMark)
    {
        size_t  markAt = 0;



        Assert::AreEqual (std::wstring (L"Block inspector - vol.po compared with b.po"), BlockInspectorState::BuildTitle (L"vol.po", L" compared with b.po", false, markAt));
        Assert::IsTrue (markAt == std::wstring::npos);
    }


    TEST_METHOD (Title_WithEditsMarksAfterTheName)
    {
        size_t        markAt = 0;
        std::wstring  title  = BlockInspectorState::BuildTitle (L"vol.po", L" (the file can't be read)", true, markAt);



        Assert::AreEqual (std::wstring (L"Block inspector - vol.po").size(), markAt);
        Assert::AreEqual (std::wstring (L" (the file can't be read)"), title.substr (markAt));
    }


    TEST_METHOD (EditBar_ClosedModelCantEdit)
    {
        BlockInspectorModel  model;
        EditBarState         state = BlockInspectorState::BuildEditBarState (model, false, false, false, L"");



        Assert::IsFalse (state.canEdit);
        Assert::IsFalse (state.canApply);
        Assert::AreEqual (0, state.bytes);
        Assert::AreEqual (0, state.sectors);
        Assert::IsFalse (state.hasChecksum);
    }


    TEST_METHOD (EditBar_CountsAPastedByte)
    {
        FakeDiskFileIo       io;
        BlockInspectorModel  model;
        EditBarState         state;



        OpenModel (io, model);
        EditOneByte (model);

        state = BlockInspectorState::BuildEditBarState (model, false, true, false, L"Saved to vol.po");

        Assert::IsTrue (state.canEdit);
        Assert::IsTrue (state.isEditing);
        Assert::IsTrue (state.hasSectorEdit);
        Assert::IsTrue (state.canSaveCopy);
        Assert::AreEqual (1, state.bytes);
        Assert::AreEqual (1, state.sectors);
        Assert::AreEqual (std::wstring (L"Saved to vol.po"), state.note);
    }


    TEST_METHOD (EditBar_BusyOrOnACardLimitsTheActions)
    {
        FakeDiskFileIo       io;
        BlockInspectorModel  model;
        EditBarState         busy;
        EditBarState         card;



        OpenModel (io, model);
        EditOneByte (model);

        busy = BlockInspectorState::BuildEditBarState (model, true, false, false, L"");
        card = BlockInspectorState::BuildEditBarState (model, false, false, true, L"");

        Assert::IsFalse (busy.canEdit);
        Assert::IsFalse (busy.hasSectorEdit);
        Assert::AreEqual (1, busy.sectors, L"the count is drawn while a job runs");
        Assert::IsFalse (card.canSaveCopy);
        Assert::IsTrue (card.canApply);
    }


    TEST_METHOD (Lead_ClosedModelIsOpening)
    {
        BlockInspectorModel                   model;
        vector<InspectorStatusBar::LeadItem>  items = BlockInspectorState::BuildLead (model, false, L"Selected: block 2");



        Assert::AreEqual ((size_t) 1, items.size());
        Assert::AreEqual (std::wstring (L"Opening"), items[0].text);
    }


    TEST_METHOD (Lead_BusyHasOnlyTheUnsavedCount)
    {
        FakeDiskFileIo                        io;
        BlockInspectorModel                   model;
        vector<InspectorStatusBar::LeadItem>  items;



        OpenModel (io, model);
        EditOneByte (model);

        items = BlockInspectorState::BuildLead (model, true, L"Selected: block 2");

        Assert::AreEqual ((size_t) 1, items.size());
        Assert::AreEqual (std::wstring (L"1 unsaved block"), items[0].text);
    }


    TEST_METHOD (Lead_EndsWithTheSelection)
    {
        FakeDiskFileIo                        io;
        BlockInspectorModel                   model;
        vector<InspectorStatusBar::LeadItem>  items;



        OpenModel (io, model);

        items = BlockInspectorState::BuildLead (model, false, L"Selected: block 2");

        Assert::IsTrue (items.size() > 1, L"the volume's chips come first");
        Assert::AreEqual (std::wstring (L"Selected: block 2"), items.back().text);
        Assert::IsTrue (items.back().kind == InspectorStatusBar::LeadKind::Selection);
    }
};
