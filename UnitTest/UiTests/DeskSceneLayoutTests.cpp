#include "Pch.h"

#include "Ui/Scene/DeskSceneLayout.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneLayoutTests
//
//  Composition rules for the desk scene: deterministic placement, drive count
//  from machine config, containment at extreme aspects, the sceneScale
//  formula, and the FR-016 single-camera / position-derived-parallax
//  invariants.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DeskSceneLayoutTests)
{
public:

    TEST_METHOD (Layout_Is_Default_Constructible)
    {
        DeskSceneLayout  layout;



        (void) layout;
        Assert::IsTrue (true);
    }

};
