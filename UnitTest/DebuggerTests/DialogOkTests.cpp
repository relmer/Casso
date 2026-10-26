#include "Pch.h"

#include "Ui/Debugger/ProjectSettingsDialog.h"
#include "Ui/Debugger/StepFiltersDialog.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DialogOkTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ProjectOkDialog, StepFiltersOkDialog
    //
    //  The dialogs with their controls built and no HWND, and what OK took.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ProjectOkDialog : public ProjectSettingsDialog
    {
    public:
        using ProjectSettingsDialog::OnCreate;
        using ProjectSettingsDialog::Layout;
        using ProjectSettingsDialog::GetResult;
    };


    class StepFiltersOkDialog : public StepFiltersDialog
    {
    public:
        using StepFiltersDialog::OnCreate;
        using StepFiltersDialog::Layout;
        using StepFiltersDialog::GetResult;
    };





    //  The first control under root, depth first, of type T that match
    //  accepts.
    template <typename T>
    static T * FindControl (IDxuiControl & root, const std::function<bool (T &)> & match)
    {
        T          * found = dynamic_cast<T *> (&root);
        DxuiPanel  * panel = dynamic_cast<DxuiPanel *> (&root);



        if (found != nullptr && match (*found))
        {
            return found;
        }

        for (size_t i = 0; panel != nullptr && i < panel->GetChildCount(); i++)
        {
            found = FindControl<T> (*panel->GetChild (i), match);

            if (found != nullptr)
            {
                return found;
            }
        }

        return nullptr;
    }


    static DxuiButton * FindOk (IDxuiControl & root)
    {
        return FindControl<DxuiButton> (root, [] (DxuiButton & button) { return button.GetAccessibleName() == L"OK"; });
    }


    static DxuiEditableGrid * FindGrid (IDxuiControl & root)
    {
        return FindControl<DxuiEditableGrid> (root, [] (DxuiEditableGrid &) { return true; });
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DialogOkTests
    //
    //  OK commits an edit open in the grid first; when the commit fails, the
    //  dialog stays open with the edit and takes nothing.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DialogOkTests)
    {
    public:

        TEST_METHOD (ProjectSettingsOkStaysOpenWhenTheEditFails)
        {
            ProjectOkDialog     dialog;
            DxuiEditableGrid  * grid   = nullptr;
            DxuiButton        * ok     = nullptr;
            DxuiDpiScaler       scaler;
            bool                isOpen = false;



            scaler.SetDpi (96);
            dialog.OnCreate();
            dialog.Layout (RECT { 0, 0, 640, 480 }, scaler);
            grid = FindGrid (dialog);
            ok   = FindOk   (dialog);
            Assert::IsNotNull (grid, L"the dialog's grid");
            Assert::IsNotNull (ok,   L"the dialog's OK");

            //  A mode no command has is the add row's pending text until its
            //  command is committed with it.
            isOpen = grid->BeginEdit (grid->GetRowCount() - 1, ProjectSettingsDialog::kModeColumn);
            Assert::IsTrue (isOpen, L"the add row's mode is edited");

            grid->GetEditor().SetText (L"Forth");
            (void) grid->EndEdit (true);

            isOpen = grid->BeginEdit (grid->GetRowCount() - 1, ProjectSettingsDialog::kCommandColumn);
            Assert::IsTrue (isOpen, L"the add row's command is edited");

            grid->GetEditor().SetText (L"BP 300");
            ok->Click();

            Assert::IsFalse (dialog.GetResult().has_value(), L"OK took the commands");
            Assert::IsTrue  (grid->IsEditing(),              L"the edit closed");

            grid->EndEdit (false);
            isOpen = grid->BeginEdit (grid->GetRowCount() - 1, ProjectSettingsDialog::kModeColumn);
            Assert::IsTrue (isOpen, L"the add row's mode is edited again");

            grid->GetEditor().SetText (L"Monitor");
            (void) grid->EndEdit (true);

            isOpen = grid->BeginEdit (grid->GetRowCount() - 1, ProjectSettingsDialog::kCommandColumn);
            Assert::IsTrue (isOpen, L"and its command");

            grid->GetEditor().SetText (L"300L");
            ok->Click();

            Assert::IsTrue   (dialog.GetResult().has_value(), L"OK with an edit that commits takes the commands");
            Assert::AreEqual ((size_t) 1, dialog.GetResult()->size(), L"the command the edit added");
        }


        TEST_METHOD (StepFiltersOkStaysOpenWhenTheEditFails)
        {
            StepFiltersOkDialog   dialog;
            DxuiEditableGrid    * grid   = nullptr;
            DxuiButton          * ok     = nullptr;
            DxuiDpiScaler         scaler;
            bool                  isOpen = false;



            scaler.SetDpi (96);
            dialog.OnCreate();
            dialog.Layout (RECT { 0, 0, 640, 480 }, scaler);
            grid = FindGrid (dialog);
            ok   = FindOk   (dialog);
            Assert::IsNotNull (grid, L"the dialog's grid");
            Assert::IsNotNull (ok,   L"the dialog's OK");

            //  With no check, a name is no symbol.
            isOpen = grid->BeginEdit (grid->GetRowCount() - 1, 0);
            Assert::IsTrue (isOpen, L"the add row is edited");

            grid->GetEditor().SetText (L"NOSUCH");
            ok->Click();

            Assert::IsFalse (dialog.GetResult().has_value(), L"OK took the filters");
            Assert::IsTrue  (grid->IsEditing(),              L"the edit closed");

            grid->GetEditor().SetText (L"FDED");
            ok->Click();

            Assert::IsTrue   (dialog.GetResult().has_value(), L"OK with an edit that commits takes the filters");
            Assert::AreEqual ((size_t) 1, dialog.GetResult()->size(), L"the filter the edit added");
        }
    };
}
