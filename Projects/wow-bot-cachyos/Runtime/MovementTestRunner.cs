using WowBot.CachyOS.Input;
using WowBot.CachyOS.Objects;

namespace WowBot.CachyOS.Runtime;

public sealed class MovementTestRunner
{
    private readonly ObjectManager _objectManager;
    private readonly IInputDriver _input;
    private readonly TimeSpan _moveDuration;

    public MovementTestRunner(
        ObjectManager objectManager,
        IInputDriver input,
        TimeSpan moveDuration)
    {
        _objectManager = objectManager;
        _input = input;
        _moveDuration = moveDuration;
    }

    public async Task RunAsync(CancellationToken cancellationToken)
    {
        WorldSnapshot before = _objectManager.Capture();
        PlayerKinematics start = before.LocalPlayerKinematics
            ?? throw new InvalidOperationException("Local player position is unavailable.");

        Console.WriteLine(
            $"Start: X={start.Position.X:F3}, Y={start.Position.Y:F3}, " +
            $"Z={start.Position.Z:F3}, facing={start.FacingRadians:F4}");
        Console.WriteLine("Focus the WoW window now. Forward movement begins in 5 seconds.");

        for (int seconds = 5; seconds > 0; seconds--)
        {
            Console.WriteLine($"{seconds}...");
            await Task.Delay(TimeSpan.FromSeconds(1), cancellationToken);
        }

        try
        {
            Console.WriteLine($"Holding W for {_moveDuration.TotalMilliseconds:F0} ms...");
            await _input.TapAsync(GameKey.Forward, _moveDuration, cancellationToken);
        }
        finally
        {
            await _input.ReleaseAllAsync(CancellationToken.None);
        }

        await Task.Delay(TimeSpan.FromMilliseconds(400), cancellationToken);

        WorldSnapshot after = _objectManager.Capture();
        PlayerKinematics end = after.LocalPlayerKinematics
            ?? throw new InvalidOperationException("Local player position became unavailable.");
        float distance = start.Position.Distance2DTo(end.Position);

        Console.WriteLine(
            $"End:   X={end.Position.X:F3}, Y={end.Position.Y:F3}, " +
            $"Z={end.Position.Z:F3}, facing={end.FacingRadians:F4}");
        Console.WriteLine($"2D distance: {distance:F3}");

        if (distance < 0.20f)
            Console.WriteLine("Result: STUCK/NO MOVEMENT (distance below 0.20)");
        else
            Console.WriteLine("Result: MOVEMENT CONFIRMED");
    }
}
