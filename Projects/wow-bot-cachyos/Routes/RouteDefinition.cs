using System.Text.Json;
using WowBot.CachyOS.Objects;

namespace WowBot.CachyOS.Routes;

public sealed record RouteWaypoint(float X, float Y, float Z)
{
    public WorldPosition ToWorldPosition() => new(X, Y, Z);

    public static RouteWaypoint FromWorldPosition(WorldPosition position) =>
        new(position.X, position.Y, position.Z);
}

public sealed class RouteDefinition
{
    public string Name { get; init; } = string.Empty;
    public DateTimeOffset CreatedAt { get; init; }
    public List<RouteWaypoint> Waypoints { get; init; } = new();

    public void Validate()
    {
        if (string.IsNullOrWhiteSpace(Name))
            throw new InvalidDataException("Route name cannot be empty.");
        if (Waypoints.Count is < 2 or > 500)
            throw new InvalidDataException("A route must contain between 2 and 500 waypoints.");

        for (int i = 0; i < Waypoints.Count; i++)
        {
            RouteWaypoint waypoint = Waypoints[i];
            if (!float.IsFinite(waypoint.X) ||
                !float.IsFinite(waypoint.Y) ||
                !float.IsFinite(waypoint.Z))
                throw new InvalidDataException($"Waypoint {i + 1} contains a non-finite coordinate.");

            if (i == 0)
                continue;

            float segmentLength = Waypoints[i - 1]
                .ToWorldPosition()
                .Distance3DTo(waypoint.ToWorldPosition());
            if (segmentLength > 25.0f)
                throw new InvalidDataException(
                    $"Segment {i}->{i + 1} is {segmentLength:F1} units; maximum is 25.");
        }
    }

    public static async Task<RouteDefinition> LoadAsync(
        string path,
        CancellationToken cancellationToken)
    {
        await using FileStream stream = File.OpenRead(path);
        RouteDefinition route = await JsonSerializer.DeserializeAsync<RouteDefinition>(
            stream,
            cancellationToken: cancellationToken)
            ?? throw new InvalidDataException("Route JSON could not be deserialized.");
        route.Validate();
        return route;
    }

    public async Task SaveAsync(string path, CancellationToken cancellationToken)
    {
        Validate();
        string? directory = Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(directory))
            Directory.CreateDirectory(directory);

        var options = new JsonSerializerOptions { WriteIndented = true };
        await using FileStream stream = File.Create(path);
        await JsonSerializer.SerializeAsync(stream, this, options, cancellationToken);
        await stream.FlushAsync(cancellationToken);
    }
}
