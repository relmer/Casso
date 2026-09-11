#include "Pch.h"
#include "Cassque/CassqueNamedControl.h"
#include "Widgets/DxuiListView.h"
#include "Widgets/DxuiSplitter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueNamedControlTests
//
//  A named control reports its own name and keeps its widget's role; an
//  unnamed one falls back to the widget's own name.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassqueNamedControlTests)
{
public:

    TEST_METHOD (NameIsTheOneSetAndTheRoleIsTheWidgets)
    {
        CassqueNamedControl<DxuiListView>  list;

        list.SetAccessibleName (L"Files");

        Assert::AreEqual (std::wstring (L"Files"), list.GetAccessibleName());
        Assert::IsTrue   (list.GetAccessibleRole() == DxuiAccessibleRole::ListView);
    }


    TEST_METHOD (UnnamedFallsBackToTheWidget)
    {
        CassqueNamedControl<DxuiSplitter>  splitter;
        DxuiSplitter                       plain;

        Assert::AreEqual (plain.GetAccessibleName(), splitter.GetAccessibleName());
    }
};