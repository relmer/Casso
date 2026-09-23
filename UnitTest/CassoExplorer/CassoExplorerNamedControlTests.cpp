#include "Pch.h"
#include "CassoExplorer/CassoExplorerNamedControl.h"
#include "Widgets/DxuiListView.h"
#include "Widgets/DxuiSplitter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerNamedControlTests
//
//  A named control reports its own name and keeps its widget's role; an
//  unnamed one falls back to the widget's own name.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoExplorerNamedControlTests)
{
public:

    TEST_METHOD (NameIsTheOneSetAndTheRoleIsTheWidgets)
    {
        CassoExplorerNamedControl<DxuiListView>  list;

        list.SetAccessibleName (L"Files");

        Assert::AreEqual (std::wstring (L"Files"), list.GetAccessibleName());
        Assert::IsTrue   (list.GetAccessibleRole() == DxuiAccessibleRole::ListView);
    }


    TEST_METHOD (UnnamedFallsBackToTheWidget)
    {
        CassoExplorerNamedControl<DxuiSplitter>  splitter;
        DxuiSplitter                             plain;

        Assert::AreEqual (plain.GetAccessibleName(), splitter.GetAccessibleName());
    }
};