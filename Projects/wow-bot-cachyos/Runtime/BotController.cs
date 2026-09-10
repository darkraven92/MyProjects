using System.Diagnostics;
using WowBot.CachyOS.Input;
using WowBot.CachyOS.Objects;

namespace WowBot.CachyOS.Runtime;

public sealed class BotController
{
    private readonly ObjectManager _objectManager;
    private readonly IInputDriver _input;
    private readonly TimeSpan _tickInterval;
    private IBotState _state;

    public BotController(
        ObjectManager objectManager,
        IInputDriver input,
        IBotState initialState,
        TimeSpan tickInterval)
    {
        _objectManager = objectManager;
        _input = input;
        _state = initialState;
        _tickInterval = tickInterval;
    }

    public async Task RunAsync(CancellationToken cancellationToken)
    {
        try
        {
            using var timer = new PeriodicTimer(_tickInterval);
            var stopwatch = Stopwatch.StartNew();
            TimeSpan previousTick = stopwatch.Elapsed;

            while (await timer.WaitForNextTickAsync(cancellationToken))
            {
                TimeSpan now = stopwatch.Elapsed;
                TimeSpan delta = now - previousTick;
                previousTick = now;

                WorldSnapshot world = _objectManager.Capture();
                var context = new BotContext(world, _input, delta);
                _state = await _state.TickAsync(context, cancellationToken);
            }
        }
        finally
        {
            await _input.ReleaseAllAsync(CancellationToken.None);
        }
    }
}

public sealed record BotContext(
    WorldSnapshot World,
    IInputDriver Input,
    TimeSpan DeltaTime);

public interface IBotState
{
    string Name { get; }
    ValueTask<IBotState> TickAsync(BotContext context, CancellationToken cancellationToken);
}

public sealed class ObserveState : IBotState
{
    private DateTimeOffset _nextLog = DateTimeOffset.MinValue;

    public string Name => "Observe";

    public ValueTask<IBotState> TickAsync(BotContext context, CancellationToken cancellationToken)
    {
        if (DateTimeOffset.UtcNow >= _nextLog)
        {
            WowObject? player = context.World.LocalPlayer;
            string playerText = player is null
                ? "local player not found"
                : $"player=0x{player.Address:X8}, guid=0x{player.Guid:X16}";
            string positionText = context.World.LocalPlayerKinematics is null
                ? "position unavailable"
                : $"pos=({context.World.LocalPlayerKinematics.Position.X:F3}, " +
                  $"{context.World.LocalPlayerKinematics.Position.Y:F3}, " +
                  $"{context.World.LocalPlayerKinematics.Position.Z:F3}), " +
                  $"facing={context.World.LocalPlayerKinematics.FacingRadians:F4}";

            Console.WriteLine(
                $"[{context.World.CapturedAt:HH:mm:ss}] objects={context.World.Objects.Count}, " +
                $"{playerText}, {positionText}");
            _nextLog = DateTimeOffset.UtcNow.AddSeconds(1);
        }

        return ValueTask.FromResult<IBotState>(this);
    }
}
