namespace WowBot.CachyOS.Input;

public interface IInputDriver
{
    Task KeyDownAsync(GameKey key, CancellationToken cancellationToken);
    Task KeyUpAsync(GameKey key, CancellationToken cancellationToken);
    Task TapAsync(GameKey key, TimeSpan duration, CancellationToken cancellationToken);
    Task ReleaseAllAsync(CancellationToken cancellationToken);
}

public enum GameKey
{
    Forward,
    Backward,
    TurnLeft,
    TurnRight,
    Jump,
    TargetNearest,
    Control,
    Alt
}

public enum MouseButton
{
    Left,
    Right
}

public interface IScreenInputDriver : IInputDriver
{
    Task MoveMouseAbsoluteAsync(int x, int y, CancellationToken cancellationToken);
    Task ClickMouseAsync(MouseButton button, CancellationToken cancellationToken);
}

public sealed class NullInputDriver : IInputDriver
{
    public Task KeyDownAsync(GameKey key, CancellationToken cancellationToken) => Task.CompletedTask;
    public Task KeyUpAsync(GameKey key, CancellationToken cancellationToken) => Task.CompletedTask;
    public Task TapAsync(GameKey key, TimeSpan duration, CancellationToken cancellationToken) => Task.CompletedTask;
    public Task ReleaseAllAsync(CancellationToken cancellationToken) => Task.CompletedTask;
}
