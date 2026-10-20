#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  RoutineKind
//
//  How a routine was entered: Top is the code outside any call.
//
////////////////////////////////////////////////////////////////////////////////

enum class RoutineKind : Byte
{
    Top,
    Call,
    Brk,
    Interrupt,
};





////////////////////////////////////////////////////////////////////////////////
//
//  RoutineProfile
//
//  The cycles of each routine behind PROFILE's routines and call tree. A JSR,
//  a BRK or an interrupt starts a routine at the address the next instruction
//  runs from, and the routine ends when the stack pointer rises back to its
//  value before the call, so RTS, RTI and a return address pulled off the
//  stack all end it. Each instruction's cycles are the self cycles of the
//  call tree node running it; a node is a routine under its caller, so a
//  routine called from two places has two nodes.
//
//  The last kFrameCount video frames have their cycles by routine, a few
//  routines each and the rest together; or, with SetFramesCoverAll, every
//  frame since the reset, in at most kFrameCount bars, two bars becoming one
//  each time there are too many.
//
////////////////////////////////////////////////////////////////////////////////

class RoutineProfile
{
public:
    struct Routine
    {
        Word         address = 0;
        RoutineKind  kind    = RoutineKind::Top;
    };

    struct Node
    {
        size_t      routine = 0;
        int         parent  = -1;
        uint64_t    self    = 0;
        uint64_t    calls   = 0;
    };

    struct Share
    {
        size_t      routine = 0;
        uint64_t    cycles  = 0;
    };

    struct Frame
    {
        uint64_t            index  = 0;     // the first frame of the bar
        std::vector<Share>  shares;
        uint64_t            other  = 0;
        uint64_t            count  = 1;     // the frames the bar covers
    };

    //  A frame is an NTSC frame: 65 cycles a scanline, 262 scanlines.
    static constexpr size_t    kMaxNodes       = 8192;
    static constexpr size_t    kMaxDepth       = 256;
    static constexpr size_t    kFrameCount     = 600;
    static constexpr size_t    kSharesPerFrame = 16;
    static constexpr uint64_t  kCyclesPerFrame = 17030;
    static constexpr Byte      kJsr            = 0x20;
    static constexpr Byte      kBrk            = 0x00;

    RoutineProfile ();

    //  One instruction: where it ran, its opcode, what it took, the stack
    //  pointer after it, and the cycle count when it started.
    void  Record      (Word pc, Byte opcode, uint64_t cycles, Byte spAfter, uint64_t startCycle, uint64_t cyclesPerFrame);

    //  An interrupt is taken before the next instruction, which starts its
    //  routine.
    void  OnInterrupt ();

    void  Reset       ();

    //  Every frame since the reset is in a bar, of more than one frame once
    //  there are too many, in place of only the last kFrameCount.
    void  SetFramesCoverAll (bool isAll) { m_isAllFrames = isAll; }

    //  A profile of what ran after this one's added to it: its routines, its
    //  call tree, joined by each node's chain of callers, and its frames.
    void  Merge       (const RoutineProfile & later);

    const std::vector<Routine> &  GetRoutines () const { return m_routines; }
    const std::vector<Node> &     GetNodes    () const { return m_nodes; }
    const std::deque<Frame> &     GetFrames   () const { return m_frames; }

    //  Each node's total: its own cycles and those of every node under it.
    std::vector<uint64_t>  GetNodeTotals    () const;

    //  Each routine's total, counted once per call chain, so a routine that
    //  calls itself is not counted again inside itself.
    std::vector<uint64_t>  GetRoutineTotals () const;

private:
    struct Active
    {
        int   node     = 0;
        Byte  returnSp = 0;
    };

    struct Pending
    {
        RoutineKind  kind     = RoutineKind::Call;
        Byte         returnSp = 0;
    };

    size_t  FindRoutine  (Word address, RoutineKind kind);
    int     FindChild    (int parent, size_t routine);
    void    Begin        (Word pc);
    void    EndReturned  (Byte sp);
    void    AddToFrame   (size_t routine, uint64_t cycles, uint64_t startCycle, uint64_t cyclesPerFrame);

    //  Whether the stack pointer is at or above the frame's return point,
    //  modulo the page: a frame uses far less than half the stack.
    static bool  HasReturned (Byte returnSp, Byte sp);

    //  The frames joined into bars of bucket frames each, in order.
    static void  Rebucket    (std::deque<Frame> & frames, uint64_t bucket);
    static void  AddShares   (Frame & into, const Frame & from, const std::vector<size_t> & routines);

    std::vector<Routine>                    m_routines;
    std::unordered_map<uint32_t, size_t>    m_routineIndex;
    std::vector<Node>                       m_nodes;
    std::unordered_map<uint64_t, int>       m_children;
    std::vector<Active>                     m_stack;
    std::optional<Pending>                  m_pending;
    std::deque<Frame>                       m_frames;
    Byte                                    m_lastSp      = 0xFF;
    bool                                    m_isAllFrames = false;
    uint64_t                                m_bucket      = 1;      // frames a bar covers, with m_isAllFrames
};
