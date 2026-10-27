#include "Pch.h"

#include "HResultAssert.h"
#include "resource.h"
#include "Shell/CpuManager.h"
#include "Shell/DeploySave.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr DWORD  s_kRequestWaitMs = 5000;
static constexpr DWORD  s_kShortWaitMs   = 50;





////////////////////////////////////////////////////////////////////////////////
//
//  DeploySaveTests
//
//  The update installer's save behind live: what a return to live reports,
//  when the user is told the disks were saved behind live, and how a request
//  to return to live waits for its answer.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DeploySaveTests)
{
public:

    TEST_METHOD (ALiveReturnOutcomeFollowsTheReverseResult)
    {
        struct Row
        {
            std::optional<ReverseOutcome>  reverse;
            bool                           isBehindLive = false;
            LiveReturnOutcome              expected     = LiveReturnOutcome::NotReached;
            const wchar_t                * name         = nullptr;
        };

        const Row  rows[] =
        {
            { std::nullopt,                 false, LiveReturnOutcome::NotReached,     L"no outcome, as after a failed Execute"      },
            { ReverseOutcome::Moved,        false, LiveReturnOutcome::ReachedLiveEnd, L"moved, and live"                            },
            { ReverseOutcome::Moved,        true,  LiveReturnOutcome::NotReached,     L"moved, and behind live"                     },
            { ReverseOutcome::HistoryCut,   false, LiveReturnOutcome::HistoryCut,     L"the history cut"                            },
            { ReverseOutcome::AtHistoryGap, true,  LiveReturnOutcome::NotReached,     L"stopped at a gap"                           },
            { ReverseOutcome::Stopped,      true,  LiveReturnOutcome::NotReached,     L"stopped by the user"                        },
        };



        for (const Row & row : rows)
        {
            Assert::IsTrue (DeploySave::ToLiveReturnOutcome (row.reverse, row.isBehindLive) == row.expected, row.name);
        }
    }


    //  The user is told only when the disks were saved behind live: the CPU
    //  thread was behind live once frozen, or the return cut the history.
    TEST_METHOD (TheNoticeFollowsTheStateOnceFrozen)
    {
        const LiveReturnOutcome  outcomes[] =
        {
            LiveReturnOutcome::NotBehindLive,
            LiveReturnOutcome::ReachedLiveEnd,
            LiveReturnOutcome::HistoryCut,
            LiveReturnOutcome::NotReached,
            LiveReturnOutcome::TimedOut,
        };



        for (LiveReturnOutcome outcome : outcomes)
        {
            DeployFlushResult  live;
            DeployFlushResult  behind;

            behind.wasBehindLive = true;

            Assert::AreEqual (outcome == LiveReturnOutcome::HistoryCut, DeploySave::IsSavedBehindLive (outcome, live),
                              L"saved at the live end, told only of a cut history");
            Assert::IsTrue   (DeploySave::IsSavedBehindLive (outcome, behind), L"saved behind live, always told");
        }
    }


    //  The CPU thread completes the request's serial, and RequestLiveReturn
    //  returns that outcome.
    TEST_METHOD (ARequestGetsTheAnswerForItsSerial)
    {
        DeploySave         deploy;
        CpuManager         cpu;
        LiveReturnOutcome  outcome = LiveReturnOutcome::TimedOut;
        HRESULT            hr      = S_OK;



        hr = cpu.Start (nullptr,
                        [&] (const EmulatorCommand & cmd)
                        {
                            uint64_t  serial = 0;

                            if (cmd.id == IDM_DEPLOY_GO_LIVE && DeploySave::TryParseSerial (cmd.payload, serial) && deploy.IsAwaited (serial))
                            {
                                deploy.CompleteLiveReturn (serial, LiveReturnOutcome::ReachedLiveEnd);
                            }
                        },
                        nullptr,
                        nullptr);
        AssertSucceeded (hr, L"Start");

        outcome = deploy.RequestLiveReturn (cpu, s_kRequestWaitMs);

        cpu.Stop();

        Assert::IsTrue (outcome == LiveReturnOutcome::ReachedLiveEnd,                  L"the request gets the CPU thread's answer");
        Assert::IsTrue (deploy.GetLastOutcome() == LiveReturnOutcome::ReachedLiveEnd, L"and so does a read once frozen");
    }


    //  A request with no answer by the deadline is TimedOut and no longer
    //  awaited, so a Go live that starts after it does nothing; an outcome
    //  that arrives late becomes the last outcome.
    TEST_METHOD (ARequestThatTimesOutIsNoLongerAwaited)
    {
        DeploySave         deploy;
        CpuManager         cpu;
        std::string        payload;
        uint64_t           serial  = 0;
        LiveReturnOutcome  outcome = LiveReturnOutcome::NotBehindLive;



        cpu.SetCommandGate ([&] (WORD id, const std::string & text)
        {
            if (id == IDM_DEPLOY_GO_LIVE)
            {
                payload = text;
            }

            return false;
        });

        outcome = deploy.RequestLiveReturn (cpu, s_kShortWaitMs);

        Assert::IsTrue  (outcome == LiveReturnOutcome::TimedOut,                 L"no answer before the deadline");
        Assert::IsTrue  (DeploySave::TryParseSerial (payload, serial),           L"the request posted its serial");
        Assert::IsFalse (deploy.IsAwaited (serial),                              L"which is no longer awaited");
        Assert::IsTrue  (deploy.GetLastOutcome() == LiveReturnOutcome::TimedOut, L"and has no outcome yet");

        deploy.CompleteLiveReturn (serial, LiveReturnOutcome::HistoryCut);

        Assert::IsTrue (deploy.GetLastOutcome() == LiveReturnOutcome::HistoryCut, L"a late answer is the last outcome");
    }
};
