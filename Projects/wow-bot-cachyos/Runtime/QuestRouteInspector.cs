using WowBot.CachyOS.Questing;

namespace WowBot.CachyOS.Runtime;

public sealed class QuestRouteInspector
{
    private readonly string _path;

    public QuestRouteInspector(string path)
    {
        _path = path;
    }

    public void Run()
    {
        GuideRouteDefinition route = GuideRouteLoader.Load(_path);

        Console.WriteLine("Quest route validation succeeded");
        Console.WriteLine($"Name:         {route.Name}");
        Console.WriteLine($"Source:       {route.Source}");
        Console.WriteLine($"Character:    {route.Faction} {route.Race} {route.Class}");
        Console.WriteLine($"Map:          {route.MapId}");
        Console.WriteLine($"Level range:  {route.StartLevel}-{route.EndLevel}");
        Console.WriteLine($"Steps:        {route.Steps.Count}");
        Console.WriteLine();

        foreach (GuideRouteStep step in route.Steps)
        {
            string quests = step.QuestIds.Count == 0
                ? "-"
                : string.Join(", ", step.QuestIds.Zip(
                    step.QuestNames,
                    (id, name) => $"{id}:{name}"));
            string coordinate = step.GuideCoordinate is null
                ? "-"
                : $"{step.GuideCoordinate.X:F0},{step.GuideCoordinate.Y:F0}";
            string creatures = step.AllowedCreatureIds.Count == 0
                ? "-"
                : string.Join(",", step.AllowedCreatureIds);

            Console.WriteLine(
                $"{step.Sequence,2}. {step.Action,-15} quests=[{quests}] " +
                $"creatures=[{creatures}] guide={coordinate,-5} " +
                $"state={step.AutomationState}");
        }

        Console.WriteLine();
        Console.WriteLine(
            "Guide coordinates are zone-map percentages, not world coordinates. " +
            "This command validates metadata only and does not attach to WoW or send input.");
    }
}
