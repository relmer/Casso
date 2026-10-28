#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorCommands
//
//  Every command of the disk inspector, each declared once and shared by the
//  menu bar, the toolbar and the context menus (FR-158). The window fills in
//  each command's action and state; the menus and the toolbar read them.
//
////////////////////////////////////////////////////////////////////////////////

struct InspectorCommands
{
    using Command = std::shared_ptr<DxuiCommand>;

    //  File
    Command  exportFile;
    Command  saveEditedCopy;
    Command  close;

    //  Edit
    Command  undoApplied;
    Command  redoApplied;
    Command  copy;
    Command  copySector;
    Command  copyMap;
    Command  selectAll;
    Command  editSector;
    Command  applyEdits;
    Command  discardEdits;
    Command  find;
    Command  findNext;
    Command  findPrevious;

    //  View
    Command          structure;
    Command          timing;
    Command          widerRange;
    Command          narrowerRange;
    Command          zoomIn;
    Command          zoomOut;
    Command          fit;
    Command          flat;
    Command          tilted;
    vector<Command>  overlays;
    vector<Command>  views;
    Command          legend;

    //  Go
    Command  back;
    Command  forward;
    Command  goTo;
    Command  goToHead;
    Command  followHead;
    Command  previousSector;
    Command  nextSector;
    Command  previousQuarterTrack;
    Command  nextQuarterTrack;
    Command  previousTrack;
    Command  nextTrack;
    Command  previousDifference;
    Command  nextDifference;
    Command  nextPane;

    //  Disk
    vector<Command>  drives;
    Command          compareWith;
    Command          swapSides;
    Command          bSettings;
    Command          endComparison;
    Command          decodeSettings;

    //  Window
    Command  windowZoomIn;
    Command  windowZoomOut;
    Command  actualSize;
    Command  defaultSize;

    //  Help
    Command  walkthrough;
};
