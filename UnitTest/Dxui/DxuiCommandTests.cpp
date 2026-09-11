#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCommandTests
//
//  The declaration every command surface reads through: what the optional
//  functors mean when they are absent, and which text wins when more than one
//  source of it is present.
//
//  The absent-functor defaults carry real weight. Most commands are always
//  enabled and never checked, so most declarations supply neither functor, and
//  a wrong default would silently disable or check the whole menu rather than
//  fail visibly anywhere.
//
//  The last case pins that the functors are consulted on every call rather
//  than sampled once. Every surface is required to read the command at paint
//  and at click time, and a command that cached its own answer would defeat
//  that from the other side.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiCommandTests)
{
public:

    TEST_METHOD (AbsentCheckedFunctor_ReportsUnchecked)
    {
        DxuiCommand  cmd;


        Assert::IsFalse (cmd.IsChecked());
    }


    TEST_METHOD (AbsentEnabledFunctor_ReportsEnabled)
    {
        DxuiCommand  cmd;


        Assert::IsTrue (cmd.IsEnabled());
    }


    TEST_METHOD (PresentFunctors_AreConsulted)
    {
        DxuiCommand  cmd;


        cmd.isChecked = [] { return true;  };
        cmd.isEnabled = [] { return false; };

        Assert::IsTrue  (cmd.IsChecked());
        Assert::IsFalse (cmd.IsEnabled());
    }


    TEST_METHOD (GetLabelText_FallsBackToStaticLabel)
    {
        DxuiCommand  cmd;


        cmd.label = L"Reset";

        Assert::AreEqual (std::wstring (L"Reset"), cmd.GetLabelText());
    }


    TEST_METHOD (GetLabelText_PrefersLabelFunctor)
    {
        DxuiCommand  cmd;


        cmd.label     = L"Reset";
        cmd.labelText = [] { return std::wstring (L"Reset Apple //e"); };

        Assert::AreEqual (std::wstring (L"Reset Apple //e"), cmd.GetLabelText());
    }


    TEST_METHOD (GetShortText_PrefersShortLabel)
    {
        DxuiCommand  cmd;


        cmd.label      = L"Take a screenshot";
        cmd.shortLabel = L"Screenshot";

        Assert::AreEqual (std::wstring (L"Screenshot"), cmd.GetShortText());
    }


    //
    //  With no short label the toolbar shows whatever a menu row would, which
    //  means the DYNAMIC label when one is supplied, not the static one behind
    //  it. A toolbar that fell back to `label` here would keep showing the
    //  stale text for exactly the commands that bother to compute theirs.
    //
    TEST_METHOD (GetShortText_WithoutShortLabel_FollowsLabelText)
    {
        DxuiCommand  cmd;


        cmd.label     = L"Reset";
        cmd.labelText = [] { return std::wstring (L"Reset Apple //e"); };

        Assert::AreEqual (std::wstring (L"Reset Apple //e"), cmd.GetShortText());
    }


    TEST_METHOD (Functors_AreReadOnEveryCall)
    {
        DxuiCommand   cmd;
        bool          checked = false;
        std::wstring  text    = L"first";


        cmd.isChecked = [&checked] { return checked; };
        cmd.labelText = [&text]    { return text;    };

        Assert::IsFalse  (cmd.IsChecked());
        Assert::AreEqual (std::wstring (L"first"), cmd.GetLabelText());

        checked = true;
        text    = L"second";

        Assert::IsTrue   (cmd.IsChecked());
        Assert::AreEqual (std::wstring (L"second"), cmd.GetLabelText());
    }
};
