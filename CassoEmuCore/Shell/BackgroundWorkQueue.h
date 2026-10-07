#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  BackgroundWorkQueue
//
//  One background thread that runs posted jobs in order, for work that must
//  keep both the UI and the emulation responsive: reading and decoding a file,
//  for instance. Jobs run one at a time, so two of them never race each other.
//  The destructor finishes the jobs already queued, then joins.
//
////////////////////////////////////////////////////////////////////////////////

class BackgroundWorkQueue
{
public:
    using Job = std::function<void ()>;

    BackgroundWorkQueue  ();
    ~BackgroundWorkQueue ();

    BackgroundWorkQueue (const BackgroundWorkQueue &)             = delete;
    BackgroundWorkQueue & operator= (const BackgroundWorkQueue &) = delete;

    void  Post (Job job);

private:
    void  Run ();

    std::mutex               m_lock;
    std::condition_variable  m_wake;
    std::deque<Job>          m_jobs;
    bool                     m_isStopping = false;
    std::thread              m_thread;
};
