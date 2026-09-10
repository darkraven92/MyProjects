using WowBot.CachyOS.Objects;
using WowBot.CachyOS.Routes;

namespace WowBot.CachyOS.Runtime;

public sealed class RouteRecorder
{
    private readonly ObjectManager _objectManager;
    private readonly string _outputPath;
    private readonly float _spacing;

    public RouteRecorder(ObjectManager objectManager, string outputPath, float spacing)
    {
        _objectManager = objectManager;
        _outputPath = outputPath;
        _spacing = spacing;
    }

    public async Task RunAsync(CancellationToken cancellationToken)
    {
        var waypoints = new List<RouteWaypoint>();
        WorldPosition first = CapturePosition();
        AddWaypoint(waypoints, first);

        Console.WriteLine($"Recording route to: {_outputPath}");
        Console.WriteLine($"Waypoint spacing: {_spacing:F1} game units");
        Console.WriteLine("Walk the route manually in WoW. Press Ctrl+C to save and stop.");

        try
        {
            using var timer = new PeriodicTimer(TimeSpan.FromMilliseconds(200));
            while (await timer.WaitForNextTickAsync(cancellationToken))
            {
                WorldPosition current = CapturePosition();
                WorldPosition previous = waypoints[^1].ToWorldPosition();
                if (previous.Distance3DTo(current) >= _spacing)
                    AddWaypoint(waypoints, current);
            }
        }
        catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
        {
            // Ctrl+C is the normal way to finish a recording.
        }

        WorldPosition final = CapturePosition();
        if (waypoints[^1].ToWorldPosition().Distance3DTo(final) >= 0.25f)
            AddWaypoint(waypoints, final);

        if (waypoints.Count < 2)
            throw new InvalidOperationException(
                "The recording contains only one waypoint. Move at least 0.25 units before stopping.");

        var route = new RouteDefinition
        {
            Name = Path.GetFileNameWithoutExtension(_outputPath),
            CreatedAt = DateTimeOffset.UtcNow,
            Waypoints = waypoints
        };
        await route.SaveAsync(_outputPath, CancellationToken.None);

        Console.WriteLine($"Saved {waypoints.Count} waypoints to {_outputPath}");
    }

    private WorldPosition CapturePosition() =>
        _objectManager.Capture().LocalPlayerKinematics?.Position
        ?? throw new InvalidOperationException("Local player position is unavailable.");

    private static void AddWaypoint(List<RouteWaypoint> waypoints, WorldPosition position)
    {
        waypoints.Add(RouteWaypoint.FromWorldPosition(position));
        Console.WriteLine(
            $"Waypoint {waypoints.Count,3}: " +
            $"X={position.X:F3}, Y={position.Y:F3}, Z={position.Z:F3}");
    }
}
