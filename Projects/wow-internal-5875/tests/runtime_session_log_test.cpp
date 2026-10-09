#include "../src/Debug/SessionLog.h"
#include "../src/Bot/QuestDeferPolicy.h"

#include <cassert>
#include <chrono>
#include <iterator>

static std::string Read(const std::filesystem::path& path)
{
    std::ifstream input(path);
    return {std::istreambuf_iterator<char>(input), {}};
}

int main()
{
    const Debug::LogSessionIdentity session{123, 456, 1000, 7, 0x4000, "WoW.exe"};
    const auto fields = session.Fields();
    assert(fields == "pid=123 session=123.456.1000.7 dllBase=0x4000 process=\"WoW.exe\"");
    assert(session.Fields() == fields);
    auto reloaded = session;
    reloaded.startedMs += 1000;
    assert(reloaded.Fields() != fields); // same PID/base, different DLL session
    auto reusedPid = session;
    ++reusedPid.processCreated;
    assert(reusedPid.Fields() != fields);
    auto gui = session;
    gui.pid = 124;
    gui.process = "wow_gui.exe";
    assert(gui.Fields() != fields);

    const auto directory = std::filesystem::temp_directory_path() /
        ("wow-runtime-session-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    assert(std::filesystem::create_directory(directory));
    const auto log = directory / "runtime.log";
    const auto journal = directory / "runtime.lifecycle.log";
    Bot::QuestDeferPolicy difficulty;
    constexpr int quest = 1234; // synthetic, no profile dependency
    assert(!difficulty.ObserveObjectiveFailure(quest, true, 469, 20, 0,
        std::uint64_t{3}));
    assert(Debug::AppendSessionLog(log, fields, "first_failure"));
    const auto first = Read(log);
    assert(first.find("LOGGER SESSION " + fields) != std::string::npos);
    assert(first.find("openMode=append") != std::string::npos);
    assert(Debug::AppendSessionLog(log, fields, "reopen"));
    assert(Read(log).substr(0, first.size()) == first); // no truncation
    assert(Read(log).find("LOGGER SESSION", first.size()) == std::string::npos);
    assert(difficulty.Find(quest)->objectiveFailures == 1);

    // GUI journal reconnect/clear and ordinary log rotation have no policy
    // dependency. Exercise the actual append helper used by Logger, not a
    // substitute reset implementation.
    assert(Debug::AppendSessionLog(journal, gui.Fields(), "GUI display_connected"));
    std::filesystem::rename(log, directory / "rotated.log");
    assert(Debug::AppendSessionLog(log, fields, "after_rotation"));
    assert(Read(log).find("LOGGER SESSION " + fields) != std::string::npos);
    assert(Read(log).find("runtimeReset=no") != std::string::npos);
    assert(difficulty.Find(quest)->objectiveFailures == 1);
    assert(std::filesystem::remove(log));
    assert(Debug::AppendSessionLog(log, fields, "after_deletion"));
    assert(!Read(journal).empty());
    assert(!difficulty.ObserveObjectiveFailure(quest, true, 944, 20, 0,
        std::uint64_t{478}));
    assert(difficulty.Find(quest)->objectiveFailures == 2);
    assert(difficulty.ObserveObjectiveFailure(quest, true, 1419, 20, 0,
        std::uint64_t{953}));
    assert(difficulty.IsDeferred(quest));
    assert(difficulty.RevisitEligible(quest, 1800, 20, 0));
    const Bot::QuestRevisitBoundary owned{true, false, false, false, false};
    assert(!Bot::CanEvaluateQuestRevisit(owned));
    assert(difficulty.Find(quest)->objectiveFailures == 3);
    assert(Bot::CanEvaluateQuestRevisit({}));

    // Only actual controller/session reconstruction creates an empty policy;
    // logging a different session itself does not mutate the old policy.
    assert(Debug::AppendSessionLog(log, reloaded.Fields(), "new_bootstrap " + reloaded.Fields()));
    Bot::QuestDeferPolicy newRuntime;
    assert(newRuntime.Find(quest) == nullptr);
    assert(difficulty.IsDeferred(quest));
    assert(!Debug::AppendSessionLog(directory / "missing" / "log", fields, "failed_open"));
    assert(difficulty.IsDeferred(quest));
    std::filesystem::remove_all(directory);
}
