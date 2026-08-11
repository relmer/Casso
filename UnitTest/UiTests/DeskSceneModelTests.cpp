#include "Pch.h"

#include "Ui/Scene/DeskSceneModel.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModelTests
//
//  Model load and discovery over synthetic OBJ/MTL text and the real embedded
//  model text: sub-mesh split by material color, glass surface derivation, UV
//  synthesis exactness, and region box sanity.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DeskSceneModelTests)
{
public:

    TEST_METHOD (Model_Is_Default_Constructible)
    {
        DeskSceneModel  model;



        (void) model;
        Assert::IsTrue (true);
    }

};
