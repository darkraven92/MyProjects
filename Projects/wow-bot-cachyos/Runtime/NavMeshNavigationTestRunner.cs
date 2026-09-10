using System.Diagnostics;
using WowBot.CachyOS.Input;
using WowBot.CachyOS.Navigation;
using WowBot.CachyOS.Objects;

namespace WowBot.CachyOS.Runtime;

public sealed class NavMeshNavigationTestRunner
{
    private const float MaximumTargetDistance = 50.0f;
    private const int MaximumPathPoints = 16;
    private static readonly TimeSpan MaximumTotalDuration = TimeSpan.FromSeconds(90);

    private readonly ObjectManager _objectManager;
    private readonly IInputDriver _input;
    private readonly string _mmapsDirectory;
    private readonly uint _mapId;
    private readonly WorldPosition _requestedTarget;

    public NavMeshNavigationTestRunner(
        ObjectManager objectManager,
        IInputDriver input,
        string mmapsDirectory,
        uint mapId,
        WorldPosition requestedTarget)
    {
        _objectManager = objectManager;
        _input = input;
        _mmapsDirectory = mmapsDirectory;
        _mapId = mapId;
        _requestedTarget = requestedTarget;
    }

    public async Task RunAsync(CancellationToken cancellationToken)
    {
        PlayerKinematics player = CapturePlayer();
        float requestedDistance = player.Position.Distance2DTo(_requestedTarget);
        if (requestedDistance > MaximumTargetDistance)
            throw new InvalidOperationException(
                $"The requested target is {requestedDistance:F1} units away; " +
                $"the bounded NavMesh movement test permits at most {MaximumTargetDistance:F0}.");

        Console.WriteLine($"NavMesh directory: {_mmapsDirectory}");
        Console.WriteLine($"Map:               {_mapId}");
        Console.WriteLine(
            $"Start:             X={player.Position.X:F3}, " +
            $"Y={player.Position.Y:F3}, Z={player.Position.Z:F3}");
        Console.WriteLine(
            $"Requested target:  X={_requestedTarget.X:F3}, " +
            $"Y={_requestedTarget.Y:F3}, Z={_requestedTarget.Z:F3}");

        using var navMesh = new NavMeshClient(_mmapsDirectory, _mapId);
        NavMeshPath path = navMesh.FindPath(player.Position, _requestedTarget);
        if (path.IsPartial)
            throw new InvalidOperationException(
                "Detour returned a partial path. The bounded movement test will not follow it.");
        if (path.Points.Count is < 2 or > MaximumPathPoints)
            throw new InvalidOperationException(
                $"The path contains {path.Points.Count} points; the bounded test requires " +
                $"between 2 and {MaximumPathPoints}.");

        Console.WriteLine($"Loaded tiles:       {path.LoadedTiles}");
        Console.WriteLine($"Path points:        {path.Points.Count}");
        for (int index = 0; index < path.Points.Count; index++)
        {
            WorldPosition point = path.Points[index];
            Console.WriteLine(
                $"  [{index,2}] X={point.X,10:F3}, Y={point.Y,10:F3}, Z={point.Z,8:F3}");
        }

        ValidateSegments(path.Points);

        Console.WriteLine(
            "Focus WoW and do not provide manual movement input. " +
            "NavMesh movement begins in 5 seconds.");
        for (int seconds = 5; seconds > 0; seconds--)
        {
            Console.WriteLine($"{seconds}...");
            await Task.Delay(TimeSpan.FromSeconds(1), cancellationToken);
        }

        using var totalTimeout = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        totalTimeout.CancelAfter(MaximumTotalDuration);
        var stopwatch = Stopwatch.StartNew();
        var navigator = new WaypointNavigator(
            _objectManager,
            _input,
            NavigationSettings.BoundedTest);

        try
        {
            // Point zero is Detour's projection of the current player position.
            for (int index = 1; index < path.Points.Count; index++)
            {
                WorldPosition target = path.Points[index];
                float remaining = CapturePlayer().Position.Distance2DTo(target);
                Console.WriteLine(
                    $"NavMesh point {index}/{path.Points.Count - 1}: " +
                    $"X={target.X:F3}, Y={target.Y:F3}, remaining={remaining:F3}");

                if (remaining <= NavigationSettings.BoundedTest.ArrivalRadius)
                {
                    Console.WriteLine("Already inside arrival radius; skipping point.");
                    continue;
                }

                NavigationResult result = await navigator.NavigateAsync(
                    target,
                    totalTimeout.Token);
                Console.WriteLine(
                    $"Point result: {result.Outcome}, " +
                    $"remaining={result.RemainingDistance:F3}, " +
                    $"recoveries={result.Recoveries}");

                if (result.Outcome != NavigationOutcome.Arrived)
                {
                    Console.WriteLine("Result: NAVMESH MOVEMENT ABORTED");
                    return;
                }
            }

            WorldPosition finalPosition = CapturePlayer().Position;
            float finalDistance = finalPosition.Distance2DTo(_requestedTarget);
            Console.WriteLine(
                $"Final:              X={finalPosition.X:F3}, " +
                $"Y={finalPosition.Y:F3}, Z={finalPosition.Z:F3}");
            Console.WriteLine($"Distance to target: {finalDistance:F3}");
            Console.WriteLine($"Elapsed movement:   {stopwatch.Elapsed.TotalSeconds:F1} seconds");
            Console.WriteLine("Result: NAVMESH MOVEMENT COMPLETED");
        }
        catch (OperationCanceledException) when (
            totalTimeout.IsCancellationRequested && !cancellationToken.IsCancellationRequested)
        {
            Console.WriteLine(
                $"Result: NAVMESH MOVEMENT TIMED OUT after " +
                $"{MaximumTotalDuration.TotalSeconds:F0} seconds");
        }
        finally
        {
            await _input.ReleaseAllAsync(CancellationToken.None);
        }
    }

    private PlayerKinematics CapturePlayer() =>
        _objectManager.Capture().LocalPlayerKinematics
        ?? throw new InvalidOperationException("Local player position is unavailable.");

    private static void ValidateSegments(IReadOnlyList<WorldPosition> points)
    {
        for (int index = 1; index < points.Count; index++)
        {
            float horizontal = points[index - 1].Distance2DTo(points[index]);
            float vertical = MathF.Abs(points[index].Z - points[index - 1].Z);
            if (horizontal > 35.0f)
                throw new InvalidOperationException(
                    $"Path segment {index} is {horizontal:F1} units long; " +
                    "the bounded movement test permits at most 35.");
            if (vertical > 8.0f)
                throw new InvalidOperationException(
                    $"Path segment {index} changes height by {vertical:F1} units; " +
                    "the bounded movement test permits at most 8.");
        }
    }
}
