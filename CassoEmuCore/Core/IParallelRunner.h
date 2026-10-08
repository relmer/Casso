#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  IParallelRunner
//
//  Runs a set of independent jobs, as many at once as it can, and returns
//  once every one has finished. A job must not touch anything another job in
//  the same set writes.
//
////////////////////////////////////////////////////////////////////////////////

class IParallelRunner
{
public:
    using Job = std::function<void()>;

    virtual      ~IParallelRunner() = default;
    virtual void  RunAll          (std::span<const Job> jobs) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  InlineParallelRunner
//
//  Runs the jobs one after another on the calling thread, for a caller with
//  no pool and for tests that want a fixed order.
//
////////////////////////////////////////////////////////////////////////////////

class InlineParallelRunner : public IParallelRunner
{
public:
    void  RunAll (std::span<const Job> jobs) override
    {
        for (const Job & job : jobs)
        {
            job();
        }
    }
};
