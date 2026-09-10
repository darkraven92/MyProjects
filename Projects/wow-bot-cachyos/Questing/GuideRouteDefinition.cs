using System.Text.Json.Serialization;

namespace WowBot.CachyOS.Questing;

[JsonConverter(typeof(JsonStringEnumConverter))]
public enum GuideRouteAction
{
    AcceptQuest,
    TurnInQuest,
    CompleteQuest,
    ReachLevel,
    TrainClass,
    UseHearthstone,
    Travel
}

[JsonConverter(typeof(JsonStringEnumConverter))]
public enum RouteAutomationState
{
    MetadataOnly,
    NeedsNpcResolution,
    NeedsObjectiveResolution,
    NeedsWorldPosition
}

public sealed record GuideMapCoordinate(float X, float Y);

public sealed record GuideRouteStep
{
    public required int Sequence { get; init; }
    public required string Id { get; init; }
    public required GuideRouteAction Action { get; init; }
    public IReadOnlyList<uint> QuestIds { get; init; } = [];
    public IReadOnlyList<string> QuestNames { get; init; } = [];
    public IReadOnlyList<uint> AllowedCreatureIds { get; init; } = [];
    public uint? RequiredLevel { get; init; }
    public GuideMapCoordinate? GuideCoordinate { get; init; }
    public required RouteAutomationState AutomationState { get; init; }
}

public sealed record GuideRouteDefinition
{
    public required string Name { get; init; }
    public required string Source { get; init; }
    public required string Faction { get; init; }
    public required string Race { get; init; }
    public required string Class { get; init; }
    public required uint MapId { get; init; }
    public required uint StartLevel { get; init; }
    public required uint EndLevel { get; init; }
    public required IReadOnlyList<GuideRouteStep> Steps { get; init; }

    public void Validate()
    {
        if (string.IsNullOrWhiteSpace(Name) || string.IsNullOrWhiteSpace(Source))
            throw new InvalidDataException("The quest route name and source are required.");
        if (StartLevel is < 1 or > 60 || EndLevel < StartLevel || EndLevel > 60)
            throw new InvalidDataException("The quest route level range is invalid.");
        if (Steps.Count == 0)
            throw new InvalidDataException("The quest route contains no steps.");

        var ids = new HashSet<string>(StringComparer.Ordinal);
        int previousSequence = 0;
        foreach (GuideRouteStep step in Steps)
        {
            if (step.Sequence <= previousSequence)
                throw new InvalidDataException("Quest route sequences must be strictly increasing.");
            if (string.IsNullOrWhiteSpace(step.Id) || !ids.Add(step.Id))
                throw new InvalidDataException($"Quest route step id '{step.Id}' is empty or duplicated.");
            if (step.QuestIds.Any(id => id == 0))
                throw new InvalidDataException($"Quest route step '{step.Id}' contains quest id zero.");
            if (step.QuestIds.Count != step.QuestNames.Count)
                throw new InvalidDataException(
                    $"Quest route step '{step.Id}' has mismatched quest ids and names.");
            if (step.AllowedCreatureIds.Any(id => id == 0))
                throw new InvalidDataException(
                    $"Quest route step '{step.Id}' contains creature id zero.");
            if (step.RequiredLevel is < 1 or > 60)
                throw new InvalidDataException($"Quest route step '{step.Id}' has an invalid level.");
            if (step.GuideCoordinate is { } coordinate &&
                (coordinate.X is < 0 or > 100 || coordinate.Y is < 0 or > 100))
            {
                throw new InvalidDataException(
                    $"Quest route step '{step.Id}' has an invalid guide coordinate.");
            }

            previousSequence = step.Sequence;
        }
    }
}
