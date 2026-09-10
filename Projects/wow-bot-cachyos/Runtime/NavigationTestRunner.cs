using WowBot.CachyOS.Input;
using WowBot.CachyOS.Navigation;
using WowBot.CachyOS.Objects;

namespace WowBot.CachyOS.Runtime;

public sealed class NavigationTestRunner
{
    private readonly ObjectManager _objectManager;
    private readonly IInputDriver _input;
    private readonly float _distance;
    private readonly float _bearingDegrees;

    public NavigationTestRunner(
        ObjectManager objectManager,
        IInputDriver input,
        float distance,
        float bearingDegrees)
    {
        _objectManager = objectManager;
        _input = input;
        _distance = distance;
        _bearingDegrees = bearingDegrees;
    }

    public async Task RunAsync(CancellationToken cancellationToken)
    {
        PlayerKinematics start = _objectManager.Capture().LocalPlayerKinematics
            ?? throw new InvalidOperationException("Local player position is unavailable.");

        float targetHeading = AngleMath.NormalizePositive(
            start.FacingRadians + _bearingDegrees * MathF.PI / 180.0f);
        var target = new WorldPosition(
            start.Position.X + MathF.Cos(targetHeading) * _distance,
            start.Position.Y + MathF.Sin(targetHeading) * _distance,
            start.Position.Z);

        Console.WriteLine(
            $"Start:  X={start.Position.X:F3}, Y={start.Position.Y:F3}, " +
            $"facing={start.FacingRadians:F4}");
        Console.WriteLine(
            $"Target: X={target.X:F3}, Y={target.Y:F3}, " +
            $"distance={_distance:F1}, relative bearing={_bearingDegrees:F1} degrees");
        Console.WriteLine(
            "Focus WoW and do not provide manual movement input. " +
            "Navigation begins in 5 seconds.");

        for (int seconds = 5; seconds > 0; seconds--)
        {
            Console.WriteLine($"{seconds}...");
            await Task.Delay(TimeSpan.FromSeconds(1), cancellationToken);
        }

        var navigator = new WaypointNavigator(
            _objectManager,
            _input,
            NavigationSettings.BoundedTest);
        NavigationResult result = await navigator.NavigateAsync(target, cancellationToken);

        Console.WriteLine(
            $"Final:  X={result.FinalPosition.X:F3}, Y={result.FinalPosition.Y:F3}");
        Console.WriteLine($"Remaining distance: {result.RemainingDistance:F3}");
        Console.WriteLine($"Elapsed: {result.Elapsed.TotalSeconds:F1} seconds");
        Console.WriteLine($"Recoveries: {result.Recoveries}");
        Console.WriteLine($"Result: {result.Outcome.ToString().ToUpperInvariant()}");
    }
}
