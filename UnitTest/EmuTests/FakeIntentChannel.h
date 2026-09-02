#pragma once

#include "Pch.h"
#include "Cli/IIntentChannel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeIntentChannel
//
//  Records what a writer stated, so a test can assert the intent reached the
//  channel without a window, a message pump or a second process.
//
//  IT CANNOT FAIL, and neither can the real one: StateIntent returns nothing
//  because an undelivered intent degrades to the fallback answer rather than
//  becoming an error somebody has to handle.
//
////////////////////////////////////////////////////////////////////////////////

class FakeIntentChannel : public IIntentChannel
{
public:

    struct Stated
    {
        std::string   imagePath;
        PickUpIntent  intent = PickUpIntent::Unstated;
    };

    std::vector<Stated>  stated;



    void  StateIntent (const std::string & imagePath, PickUpIntent intent) override
    {
        stated.push_back (Stated { imagePath, intent });

        return;
    }
};
