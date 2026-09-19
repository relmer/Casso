#pragma once

#include "Debugger/Reply.h"
#include "Widgets/DxuiButton.h"
#include "Widgets/DxuiListView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackPane
//
//  The chain of calls to PC (FR-067), separate from the raw stack pane: a row
//  a frame, innermost first, with the call site, the routine and which
//  mechanism found it, and a break as a separator row where the chain cannot
//  be trusted (FR-069). A button above the list shows the mechanism and
//  moves to the next one (FR-068).
//
//  Activating a row -- a double-click or Enter -- moves the disassembly to
//  the frame's call site, or to the instruction that broke the chain.
//
//  The window owns the list and the button as child controls; the pane holds
//  pointers to them. The rows come from the snapshot's CALLS reply, and the
//  button sends CALLS MODE, so the pane shows nothing CALLS would not.
//
////////////////////////////////////////////////////////////////////////////////

class CallStackPane
{
public:
    using RunFn  = std::function<void (const std::string & line)>;
    using ShowFn = std::function<void (Word address)>;

    //  One row as the list shows it. address is where activating it moves
    //  the disassembly.
    struct Row
    {
        std::wstring  site;
        std::wstring  routine;
        std::wstring  foundBy;
        bool          isBreak   = false;
        bool          isDim     = false;
        Word          address   = 0;
    };

    CallStackPane (DxuiListView * list, DxuiButton * modeButton, RunFn run, ShowFn showCode);

    DxuiListView *  GetList       () const { return m_list; }
    DxuiButton *    GetModeButton () const { return m_modeButton; }

    void  Configure ();
    void  Apply     (const CallStackData & data);

    static std::vector<Row>  GetRows      (const CallStackData & data);
    static std::string       GetNextModeLine (CallStackMechanism current);
    static std::wstring      GetModeLabel (CallStackMechanism mechanism);

private:
    DxuiListView        * m_list       = nullptr;
    DxuiButton          * m_modeButton = nullptr;
    RunFn                 m_run;
    ShowFn                m_showCode;
    std::vector<Row>      m_rows;
    CallStackMechanism    m_mechanism  = CallStackMechanism::Hybrid;
};
