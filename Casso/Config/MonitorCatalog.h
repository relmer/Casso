#pragma once

#include "resource.h"
#include "UiCommandTypes.h"
#include "Ui/Scene/DeskSceneModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorSpec / MonitorCatalog
//
//  THE MONITORS THEMSELVES, as things with properties -- rather than as a
//  boolean asked of the machine at each site that needs to know.
//
//  A phosphor is a fact about a tube, not about a computer. Before this the
//  two were split: the desk scene picked its monitor mesh by asking whether
//  the machine was a //c, while the screen's color came from a per-machine UI
//  preference that had nothing to do with which monitor was standing there.
//  Nothing tied them, so a //c could show white and an //e could show the
//  Monitor II housing lit any color at all, and neither could be called wrong
//  because neither was derived from anything.
//
//  Here a monitor owns its phosphor and its meshes, a machine names the
//  monitor it ships with, and the screen's default color is whatever that
//  tube actually glows. A user override still wins -- the per-machine store
//  keeps whatever differs from this -- but the baseline now comes from the
//  hardware rather than from a table of defaults written for no machine in
//  particular.
//
//  Machines name their monitor with the `monitor` key at the top level of
//  their JSON. An absent key means the default, which is what every machine
//  but the //c shipped alongside.
//
////////////////////////////////////////////////////////////////////////////////

struct MonitorSpec
{
    std::string_view  configName;      // as written in a machine's `monitor` key
    DeskDeviceKind    sceneKind;
    int               meshResourceId;  // the baked mesh, built by MeshBake
    ColorMode         phosphor;        // what this tube actually glows
};


inline constexpr MonitorSpec s_kMonitors[] =
{
    // The Apple Monitor II (A2M2010): the beige 12-inch, a P1 green tube.
    { "MonitorII",  DeskDeviceKind::Monitor2,  IDR_MODEL_MONITOR2_MESH,  ColorMode::GreenMono },

    // The Monitor //c (A2M4090): the platinum 9-inch that shipped with the
    // //c, green as well.
    { "MonitorIIc", DeskDeviceKind::Monitor2c, IDR_MODEL_MONITOR2C_MESH, ColorMode::GreenMono },
};





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorCatalog
//
////////////////////////////////////////////////////////////////////////////////

namespace MonitorCatalog
{
    // The monitor a machine gets when its JSON names none. Every machine but
    // the //c stood in front of one of these.
    inline constexpr const MonitorSpec &  Default()
    {
        return s_kMonitors[0];
    }


    // The monitor by the name a machine's `monitor` key carries. An
    // unrecognized name is a config that names hardware this build does not
    // have, which is a content error rather than a runtime condition -- it
    // recovers to the default so the scene still comes up.
    inline const MonitorSpec &  ByName (const std::string & name)
    {
        for (const MonitorSpec & spec : s_kMonitors)
        {
            if (spec.configName == name)
            {
                return spec;
            }
        }

        return Default();
    }


    // The monitor named by an already-merged machine config. Reads the same
    // document the rest of the switch path reads, so the scene's mesh and the
    // screen's color cannot disagree about which monitor is present.
    inline const MonitorSpec &  ForMachineJson (const JsonValue & mergedJson)
    {
        std::string  name;

        if (mergedJson.GetType() == JsonType::Object &&
            mergedJson.HasString ("monitor", name))
        {
            return ByName (name);
        }

        return Default();
    }


    // The View command that lights a tube its own color, for the paths that
    // drive the shell by posted command rather than by direct call.
    inline WORD  PhosphorCommand (const MonitorSpec & spec)
    {
        switch (spec.phosphor)
        {
            case ColorMode::GreenMono:  return IDM_VIEW_GREEN;
            case ColorMode::AmberMono:  return IDM_VIEW_AMBER;
            case ColorMode::WhiteMono:  return IDM_VIEW_WHITE;
            default:                    return IDM_VIEW_COLOR;
        }
    }


    // The same answer as an index into SettingsColorMode (Color=0, Green=1,
    // Amber=2, White=3), for the paths that call SetColorModeLive directly.
    inline int  PhosphorSettingsIndex (const MonitorSpec & spec)
    {
        switch (spec.phosphor)
        {
            case ColorMode::GreenMono:  return 1;
            case ColorMode::AmberMono:  return 2;
            case ColorMode::WhiteMono:  return 3;
            default:                    return 0;
        }
    }
}
