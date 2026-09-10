using WowBot.CachyOS.Objects;
using WowBot.CachyOS.Questing;

namespace WowBot.CachyOS.Runtime;

public sealed class QuestScanner
{
    private readonly ObjectManager _objectManager;
    private readonly string _routePath;

    public QuestScanner(ObjectManager objectManager, string routePath)
    {
        _objectManager = objectManager;
        _routePath = routePath;
    }

    public void Run()
    {
        QuestLogSnapshot log = _objectManager.CaptureQuestLog();
        GuideRouteDefinition route = GuideRouteLoader.Load(_routePath);
        var entriesByQuest = log.Entries.ToDictionary(entry => entry.QuestId);
        var routeNames = route.Steps
            .SelectMany(step => step.QuestIds.Zip(step.QuestNames))
            .GroupBy(pair => pair.First)
            .ToDictionary(group => group.Key, group => group.First().Second);

        Console.WriteLine("Quest log descriptor scan succeeded");
        Console.WriteLine($"Player level:  {log.PlayerLevel}");
        Console.WriteLine($"Quest slots:   {log.Entries.Count}/20 active");
        Console.WriteLine($"Route:         {route.Name}");
        Console.WriteLine();

        if (log.Entries.Count == 0)
        {
            Console.WriteLine("The quest log contains no active quests.");
        }
        else
        {
            Console.WriteLine("Slot Quest  State Complete Failed Counts      Timer      Route name");
            foreach (QuestLogEntry entry in log.Entries)
            {
                string counts = string.Join("/", entry.ObjectiveCounts);
                string name = routeNames.GetValueOrDefault(entry.QuestId, "-");
                Console.WriteLine(
                    $"{entry.Slot,4} {entry.QuestId,5} 0x{entry.StateFlags:X2} " +
                    $"{(entry.IsComplete ? "yes" : "no"),8} " +
                    $"{(entry.IsFailed ? "yes" : "no"),6} " +
                    $"{counts,-11} {entry.TimerValue,10} {name}");
                Console.WriteLine($"     raw state/counts=0x{entry.RawStateAndCounts:X8}");
            }
        }

        Console.WriteLine();
        Console.WriteLine("Joana route state (absence cannot distinguish not accepted from turned in):");
        foreach (GuideRouteStep step in route.Steps)
        {
            string status = Evaluate(step, log.PlayerLevel, entriesByQuest);
            string creatures = step.AllowedCreatureIds.Count == 0
                ? "-"
                : string.Join(",", step.AllowedCreatureIds);
            Console.WriteLine(
                $"{step.Sequence,2}. {step.Action,-15} {status,-24} " +
                $"allowed-creatures=[{creatures}]");
        }

        Console.WriteLine();
        Console.WriteLine("No movement, NPC interaction, targeting, attacks, or memory writes were sent.");
    }

    private static string Evaluate(
        GuideRouteStep step,
        uint playerLevel,
        IReadOnlyDictionary<uint, QuestLogEntry> entries)
    {
        if (step.Action == GuideRouteAction.ReachLevel)
        {
            return step.RequiredLevel.HasValue && playerLevel >= step.RequiredLevel.Value
                ? "satisfied:level"
                : "blocked:level";
        }

        if (step.QuestIds.Count == 0)
            return "metadata-only";

        QuestLogEntry?[] matches = step.QuestIds
            .Select(id => entries.GetValueOrDefault(id))
            .ToArray();
        bool allPresent = matches.All(entry => entry is not null);
        bool allComplete = allPresent && matches.All(entry => entry!.IsComplete);

        return step.Action switch
        {
            GuideRouteAction.AcceptQuest => allPresent
                ? "satisfied:active"
                : "unknown:not-in-log",
            GuideRouteAction.CompleteQuest => !allPresent
                ? "blocked:not-in-log"
                : allComplete
                    ? "satisfied:complete"
                    : "in-progress",
            GuideRouteAction.TurnInQuest => !allPresent
                ? "unknown:not-in-log"
                : allComplete
                    ? "ready:turn-in"
                    : "blocked:incomplete",
            _ => "metadata-only"
        };
    }
}
