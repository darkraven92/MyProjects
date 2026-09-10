using WowBot.CachyOS.Input;
using WowBot.CachyOS.Navigation;
using WowBot.CachyOS.Objects;
using WowBot.CachyOS.Routes;

namespace WowBot.CachyOS.Runtime;

public sealed class RouteFollower
{
    private readonly ObjectManager _objectManager;
    private readonly IInputDriver _input;
    private readonly string _routePath;
    private readonly bool _startAtNearest;
    private readonly bool _reverse;
    private readonly int _loopCount;

    public RouteFollower(
        ObjectManager objectManager,
        IInputDriver input,
        string routePath,
        bool startAtNearest,
        bool reverse,
        int loopCount)
    {
        _objectManager = objectManager;
        _input = input;
        _routePath = routePath;
        _startAtNearest = startAtNearest;
        _reverse = reverse;
        _loopCount = loopCount;
    }

    public async Task RunAsync(CancellationToken cancellationToken)
    {
        RouteDefinition route = await RouteDefinition.LoadAsync(
            _routePath,
            cancellationToken);
        List<RouteWaypoint> waypoints = _reverse
            ? route.Waypoints.AsEnumerable().Reverse().ToList()
            : new List<RouteWaypoint>(route.Waypoints);
        WorldPosition start = CapturePosition();
        int startIndex = _startAtNearest
            ? FindNearestWaypointIndex(start, waypoints)
            : 0;
        float distanceToStart = start.Distance2DTo(
            waypoints[startIndex].ToWorldPosition());

        Console.WriteLine($"Route: {route.Name}");
        Console.WriteLine($"Waypoints: {waypoints.Count}");
        Console.WriteLine($"Direction: {(_reverse ? "reverse" : "forward")}");
        Console.WriteLine($"Loops: {_loopCount}");
        Console.WriteLine(
            $"Selected start waypoint: {startIndex + 1}, distance={distanceToStart:F3}");

        if (distanceToStart > 10.0f)
            throw new InvalidOperationException(
                "The player is more than 10 units from the selected start waypoint. " +
                "Move to the route start before following it.");

        if (_loopCount > 1)
        {
            float closingDistance = waypoints[^1]
                .ToWorldPosition()
                .Distance2DTo(waypoints[0].ToWorldPosition());
            if (closingDistance > 10.0f)
                throw new InvalidOperationException(
                    $"The route cannot loop safely because its last and first " +
                    $"waypoints are {closingDistance:F1} units apart; maximum is 10.");
        }

        Console.WriteLine(
            "Focus WoW and do not provide manual movement input. " +
            "Route following begins in 5 seconds.");
        for (int seconds = 5; seconds > 0; seconds--)
        {
            Console.WriteLine($"{seconds}...");
            await Task.Delay(TimeSpan.FromSeconds(1), cancellationToken);
        }

        var navigator = new WaypointNavigator(
            _objectManager,
            _input,
            NavigationSettings.BoundedTest);

        try
        {
            int completedWaypoints = 0;
            for (int loop = 0; loop < _loopCount; loop++)
            {
                int firstIndexThisLoop = loop == 0 ? startIndex : 0;
                Console.WriteLine(
                    $"Route loop {loop + 1}/{_loopCount}, " +
                    $"starting at waypoint {firstIndexThisLoop + 1}.");

                for (int i = firstIndexThisLoop; i < waypoints.Count; i++)
                {
                    WorldPosition target = waypoints[i].ToWorldPosition();
                    float remaining = CapturePosition().Distance2DTo(target);
                    Console.WriteLine(
                        $"Waypoint {i + 1}/{waypoints.Count}: " +
                        $"X={target.X:F3}, Y={target.Y:F3}, remaining={remaining:F3}");

                    if (remaining <= NavigationSettings.BoundedTest.ArrivalRadius)
                    {
                        Console.WriteLine("Already inside arrival radius; skipping movement.");
                        completedWaypoints++;
                        continue;
                    }

                    NavigationResult result = await navigator.NavigateAsync(
                        target,
                        cancellationToken);
                    Console.WriteLine(
                        $"Waypoint {i + 1} result: {result.Outcome}, " +
                        $"remaining={result.RemainingDistance:F3}, " +
                        $"recoveries={result.Recoveries}");

                    if (result.Outcome != NavigationOutcome.Arrived)
                    {
                        Console.WriteLine(
                            $"Completed waypoints before abort: {completedWaypoints}");
                        Console.WriteLine("Result: ROUTE ABORTED");
                        return;
                    }

                    completedWaypoints++;
                }
            }

            Console.WriteLine($"Completed waypoint visits: {completedWaypoints}");
            Console.WriteLine("Result: ROUTE COMPLETED");
        }
        finally
        {
            await _input.ReleaseAllAsync(CancellationToken.None);
        }
    }

    private WorldPosition CapturePosition() =>
        _objectManager.Capture().LocalPlayerKinematics?.Position
        ?? throw new InvalidOperationException("Local player position is unavailable.");

    private static int FindNearestWaypointIndex(
        WorldPosition position,
        IReadOnlyList<RouteWaypoint> waypoints)
    {
        int nearestIndex = 0;
        float nearestDistance = float.MaxValue;

        for (int i = 0; i < waypoints.Count; i++)
        {
            float distance = position.Distance2DTo(waypoints[i].ToWorldPosition());
            if (distance >= nearestDistance)
                continue;

            nearestDistance = distance;
            nearestIndex = i;
        }

        return nearestIndex;
    }
}
