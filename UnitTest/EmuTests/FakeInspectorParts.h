#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Components/InspectorCommandInterfaces.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeInspectorParts
//
//  The disk inspector's parts for a test of a command that doesn't call
//  them: a call to any part fails the test.
//
////////////////////////////////////////////////////////////////////////////////

class FakeInspectorParts : public IInspectorParts
{
public:
    InspectorDialogs  & GetDialogs     () override { return Fail<InspectorDialogs>(); }
    EditingPart       & GetEditing     () override { return Fail<EditingPart>(); }
    SelectionPart     & GetSelection   () override { return Fail<SelectionPart>(); }
    FilesView         & GetFiles       () override { return Fail<FilesView>(); }
    FindPart          & GetFind        () override { return Fail<FindPart>(); }
    InspectorToolbar  & GetToolbar     () override { return Fail<InspectorToolbar>(); }
    PlatterCorners    & GetCorners     () override { return Fail<PlatterCorners>(); }
    TrackColumn       & GetTrackColumn () override { return Fail<TrackColumn>(); }
    ComparisonPart    & GetComparison  () override { return Fail<ComparisonPart>(); }
    HostLink          & GetHostLink    () override { return Fail<HostLink>(); }

private:
    template <typename T>
    static T & Fail()
    {
        throw std::logic_error ("the command called a part of the window");
    }
};
