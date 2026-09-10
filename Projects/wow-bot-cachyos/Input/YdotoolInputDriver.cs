using System.ComponentModel;
using System.Diagnostics;

namespace WowBot.CachyOS.Input;

public sealed class YdotoolInputDriver : IScreenInputDriver
{
    private static readonly IReadOnlyDictionary<GameKey, int> LinuxKeyCodes =
        new Dictionary<GameKey, int>
        {
            [GameKey.Forward] = 17,      // KEY_W
            [GameKey.Backward] = 31,     // KEY_S
            [GameKey.TurnLeft] = 30,     // KEY_A
            [GameKey.TurnRight] = 32,    // KEY_D
            [GameKey.Jump] = 57,         // KEY_SPACE
            [GameKey.TargetNearest] = 15, // KEY_TAB
            [GameKey.Control] = 29,      // KEY_LEFTCTRL
            [GameKey.Alt] = 56           // KEY_LEFTALT
        };

    private readonly HashSet<GameKey> _heldKeys = new();

    public async Task KeyDownAsync(GameKey key, CancellationToken cancellationToken)
    {
        if (!_heldKeys.Add(key))
            return;

        try
        {
            await SendKeyEventAsync(key, isDown: true, cancellationToken);
        }
        catch
        {
            _heldKeys.Remove(key);
            throw;
        }
    }

    public async Task KeyUpAsync(GameKey key, CancellationToken cancellationToken)
    {
        _heldKeys.Remove(key);
        await SendKeyEventAsync(key, isDown: false, cancellationToken);
    }

    public async Task TapAsync(
        GameKey key,
        TimeSpan duration,
        CancellationToken cancellationToken)
    {
        await KeyDownAsync(key, cancellationToken);
        try
        {
            await Task.Delay(duration, cancellationToken);
        }
        finally
        {
            await KeyUpAsync(key, CancellationToken.None);
        }
    }

    public async Task ReleaseAllAsync(CancellationToken cancellationToken)
    {
        GameKey[] keys = _heldKeys.ToArray();
        foreach (GameKey key in keys)
        {
            try
            {
                await KeyUpAsync(key, CancellationToken.None);
            }
            catch (Exception exception)
            {
                Console.Error.WriteLine($"Could not release {key}: {exception.Message}");
            }
        }
    }

    public async Task MoveMouseAbsoluteAsync(
        int x,
        int y,
        CancellationToken cancellationToken)
    {
        if (x < 0 || y < 0)
            throw new ArgumentOutOfRangeException(nameof(x), "Mouse coordinates cannot be negative.");

        var startInfo = CreateYdotoolStartInfo();
        startInfo.ArgumentList.Add("mousemove");
        startInfo.ArgumentList.Add("--absolute");
        startInfo.ArgumentList.Add("-x");
        startInfo.ArgumentList.Add(x.ToString(System.Globalization.CultureInfo.InvariantCulture));
        startInfo.ArgumentList.Add("-y");
        startInfo.ArgumentList.Add(y.ToString(System.Globalization.CultureInfo.InvariantCulture));
        await RunYdotoolAsync(startInfo, cancellationToken);
    }

    public async Task ClickMouseAsync(
        MouseButton button,
        CancellationToken cancellationToken)
    {
        var startInfo = CreateYdotoolStartInfo();
        startInfo.ArgumentList.Add("click");
        startInfo.ArgumentList.Add(button == MouseButton.Left ? "0xC0" : "0xC1");
        await RunYdotoolAsync(startInfo, cancellationToken);
    }

    private static async Task SendKeyEventAsync(
        GameKey key,
        bool isDown,
        CancellationToken cancellationToken)
    {
        if (!LinuxKeyCodes.TryGetValue(key, out int keyCode))
            throw new ArgumentOutOfRangeException(nameof(key), key, "No Linux key code is configured.");

        var startInfo = CreateYdotoolStartInfo();
        startInfo.ArgumentList.Add("key");
        startInfo.ArgumentList.Add($"{keyCode}:{(isDown ? 1 : 0)}");

        await RunYdotoolAsync(startInfo, cancellationToken);
    }

    private static ProcessStartInfo CreateYdotoolStartInfo() => new()
    {
        FileName = "ydotool",
        UseShellExecute = false,
        RedirectStandardError = true,
        CreateNoWindow = true
    };

    private static async Task RunYdotoolAsync(
        ProcessStartInfo startInfo,
        CancellationToken cancellationToken)
    {
        Process process;
        try
        {
            process = Process.Start(startInfo)
                ?? throw new InvalidOperationException("ydotool did not start.");
        }
        catch (Win32Exception exception)
        {
            throw new InvalidOperationException(
                "ydotool was not found. Install it and start ydotoold before using an input mode.",
                exception);
        }

        using (process)
        {
            string error = await process.StandardError.ReadToEndAsync(cancellationToken);
            await process.WaitForExitAsync(cancellationToken);

            if (process.ExitCode != 0)
                throw new InvalidOperationException(
                    $"ydotool exited with code {process.ExitCode}: {error.Trim()}");
        }
    }
}
