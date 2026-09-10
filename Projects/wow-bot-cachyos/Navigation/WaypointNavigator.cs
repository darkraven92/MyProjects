using System.Diagnostics;
using WowBot.CachyOS.Input;
using WowBot.CachyOS.Objects;

namespace WowBot.CachyOS.Navigation;

public sealed record NavigationSettings(
    float ArrivalRadius,
    float TurnToleranceRadians,
    TimeSpan MinimumForwardPulse,
    TimeSpan ForwardPulse,
    float InitialForwardSpeed,
    TimeSpan MaximumDuration,
    int MaximumRecoveries)
{
    public static NavigationSettings BoundedTest { get; } = new(
        ArrivalRadius: 0.75f,
        TurnToleranceRadians: 0.12f,
        MinimumForwardPulse: TimeSpan.FromMilliseconds(65),
        ForwardPulse: TimeSpan.FromMilliseconds(250),
        InitialForwardSpeed: 7.4f,
        MaximumDuration: TimeSpan.FromSeconds(30),
        MaximumRecoveries: 2);
}

public enum NavigationOutcome
{
    Arrived,
    Stuck,
    TimedOut
}

public enum MovementState
{
    Idle,
    Calibrating,
    Turning,
    Moving,
    Recovering,
    Arrived,
    Stuck,
    TimedOut
}

public sealed record NavigationResult(
    NavigationOutcome Outcome,
    WorldPosition FinalPosition,
    float RemainingDistance,
    TimeSpan Elapsed,
    int Recoveries);

public sealed class WaypointNavigator
{
    private const double MinimumTurnPulseMilliseconds = 85.0;
    private const double TurnPulseGain = 0.82;
    private const float MaximumAdaptiveTurnToleranceRadians = 0.24f;

    private readonly ObjectManager _objectManager;
    private readonly IInputDriver _input;
    private readonly NavigationSettings _settings;
    private GameKey? _positiveTurnKey;
    private float _estimatedForwardSpeed;
    private MovementState _state = MovementState.Idle;
    private int _failedTurnPulses;
    private float _effectiveTurnToleranceRadians;
    private float? _measuredTurnRateRadiansPerSecond;

    public WaypointNavigator(
        ObjectManager objectManager,
        IInputDriver input,
        NavigationSettings settings)
    {
        _objectManager = objectManager;
        _input = input;
        _settings = settings;
        _estimatedForwardSpeed = settings.InitialForwardSpeed;
        _effectiveTurnToleranceRadians = settings.TurnToleranceRadians;
    }

    public async Task<NavigationResult> NavigateAsync(
        WorldPosition target,
        CancellationToken cancellationToken)
    {
        TransitionTo(MovementState.Idle);
        _failedTurnPulses = 0;
        if (_positiveTurnKey is null)
            TransitionTo(MovementState.Calibrating);

        GameKey positiveTurnKey = _positiveTurnKey ??
            await CalibrateTurnDirectionAsync(cancellationToken);
        _positiveTurnKey = positiveTurnKey;
        GameKey negativeTurnKey = positiveTurnKey == GameKey.TurnLeft
            ? GameKey.TurnRight
            : GameKey.TurnLeft;

        Console.WriteLine(
            $"Turn calibration: positive angle = {positiveTurnKey}, " +
            $"negative angle = {negativeTurnKey}");

        var stopwatch = Stopwatch.StartNew();
        int stalledPulses = 0;
        int recoveries = 0;

        try
        {
            while (stopwatch.Elapsed < _settings.MaximumDuration)
            {
                cancellationToken.ThrowIfCancellationRequested();

                PlayerKinematics current = CapturePlayer();
                float remaining = current.Position.Distance2DTo(target);
                if (remaining <= _settings.ArrivalRadius)
                {
                    TransitionTo(MovementState.Arrived);
                    return Result(NavigationOutcome.Arrived, current.Position, remaining);
                }

                float desiredHeading = AngleMath.HeadingTo(current.Position, target);
                float headingError = AngleMath.NormalizeSigned(
                    desiredHeading - current.FacingRadians);

                Console.WriteLine(
                    $"[{stopwatch.Elapsed.TotalSeconds,5:F1}s] " +
                    $"pos=({current.Position.X:F3}, {current.Position.Y:F3}), " +
                    $"remaining={remaining:F3}, error={headingError:F3}");

                if (MathF.Abs(headingError) > _effectiveTurnToleranceRadians)
                {
                    TransitionTo(MovementState.Turning);
                    GameKey turnKey = headingError > 0.0f
                        ? positiveTurnKey
                        : negativeTurnKey;
                    bool facingChanged = await ExecuteVerifiedTurnAsync(
                        turnKey,
                        MathF.Abs(headingError),
                        current.FacingRadians,
                        cancellationToken);
                    if (!facingChanged && _failedTurnPulses >= 6)
                    {
                        TransitionTo(MovementState.Stuck);
                        return Result(
                            NavigationOutcome.Stuck,
                            current.Position,
                            remaining);
                    }
                    continue;
                }

                TransitionTo(MovementState.Moving);
                WorldPosition beforePulse = current.Position;
                float beforeRemaining = remaining;

                TimeSpan forwardPulse = CalculateForwardPulse(remaining);
                Console.WriteLine(
                    $"Forward pulse: {forwardPulse.TotalMilliseconds:F0} ms " +
                    $"(estimated speed {_estimatedForwardSpeed:F2})");
                await _input.TapAsync(
                    GameKey.Forward,
                    forwardPulse,
                    cancellationToken);
                await Task.Delay(TimeSpan.FromMilliseconds(120), cancellationToken);

                PlayerKinematics afterPulse = CapturePlayer();
                float moved = beforePulse.Distance2DTo(afterPulse.Position);
                float afterRemaining = afterPulse.Position.Distance2DTo(target);
                float progress = beforeRemaining - afterRemaining;

                float closestPassDistance = DistanceToSegment2D(
                    target,
                    beforePulse,
                    afterPulse.Position);
                if (closestPassDistance <= _settings.ArrivalRadius)
                {
                    TransitionTo(MovementState.Arrived);
                    return Result(
                        NavigationOutcome.Arrived,
                        afterPulse.Position,
                        afterRemaining);
                }

                UpdateForwardSpeedEstimate(moved, progress, forwardPulse);

                if (moved < 0.08f || progress < 0.03f)
                    stalledPulses++;
                else
                    stalledPulses = 0;

                if (stalledPulses < 3)
                    continue;

                recoveries++;
                if (recoveries > _settings.MaximumRecoveries)
                {
                    TransitionTo(MovementState.Stuck);
                    return Result(NavigationOutcome.Stuck, afterPulse.Position, afterRemaining);
                }

                TransitionTo(MovementState.Recovering);
                Console.WriteLine(
                    $"No useful progress for three pulses. " +
                    $"Running recovery {recoveries}/{_settings.MaximumRecoveries}.");
                await RecoverAsync(positiveTurnKey, cancellationToken);
                stalledPulses = 0;
            }

            PlayerKinematics timedOut = CapturePlayer();
            TransitionTo(MovementState.TimedOut);
            return Result(
                NavigationOutcome.TimedOut,
                timedOut.Position,
                timedOut.Position.Distance2DTo(target));
        }
        finally
        {
            await _input.ReleaseAllAsync(CancellationToken.None);
        }

        NavigationResult Result(
            NavigationOutcome outcome,
            WorldPosition finalPosition,
            float remainingDistance) =>
            new(outcome, finalPosition, remainingDistance, stopwatch.Elapsed, recoveries);
    }

    private async Task<GameKey> CalibrateTurnDirectionAsync(
        CancellationToken cancellationToken)
    {
        TimeSpan[] calibrationPulses =
        [
            TimeSpan.FromMilliseconds(150),
            TimeSpan.FromMilliseconds(220),
            TimeSpan.FromMilliseconds(300)
        ];

        float delta = 0.0f;
        TimeSpan successfulPulse = calibrationPulses[0];
        for (int attempt = 0; attempt < calibrationPulses.Length; attempt++)
        {
            TimeSpan pulse = calibrationPulses[attempt];
            PlayerKinematics before = CapturePlayer();
            await _input.TapAsync(GameKey.TurnLeft, pulse, cancellationToken);
            await Task.Delay(TimeSpan.FromMilliseconds(180), cancellationToken);
            PlayerKinematics after = CapturePlayer();

            delta = AngleMath.NormalizeSigned(
                after.FacingRadians - before.FacingRadians);
            Console.WriteLine(
                $"Turn-left calibration {attempt + 1}/{calibrationPulses.Length}: " +
                $"delta={delta:F4} radians, pulse={pulse.TotalMilliseconds:F0} ms");
            if (MathF.Abs(delta) >= 0.01f)
            {
                successfulPulse = pulse;
                break;
            }

            if (attempt + 1 < calibrationPulses.Length)
                await Task.Delay(TimeSpan.FromMilliseconds(250), cancellationToken);
        }

        if (MathF.Abs(delta) < 0.01f)
            throw new InvalidOperationException(
                "Turn calibration produced no measurable facing change after three pulses. " +
                "Navigation was not started.");

        _measuredTurnRateRadiansPerSecond =
            MathF.Abs(delta) / (float)successfulPulse.TotalSeconds;
        float minimumPracticalRotation =
            _measuredTurnRateRadiansPerSecond.Value *
            (float)(MinimumTurnPulseMilliseconds / 1000.0);
        _effectiveTurnToleranceRadians = Math.Clamp(
            MathF.Max(
                _settings.TurnToleranceRadians,
                minimumPracticalRotation * 0.65f),
            _settings.TurnToleranceRadians,
            MaximumAdaptiveTurnToleranceRadians);

        Console.WriteLine(
            $"Measured turn rate: {_measuredTurnRateRadiansPerSecond:F3} rad/s; " +
            $"adaptive tolerance: {_effectiveTurnToleranceRadians:F3} radians");

        return delta > 0.0f ? GameKey.TurnLeft : GameKey.TurnRight;
    }

    private async Task RecoverAsync(
        GameKey positiveTurnKey,
        CancellationToken cancellationToken)
    {
        await _input.TapAsync(
            GameKey.Backward,
            TimeSpan.FromMilliseconds(300),
            cancellationToken);
        await _input.TapAsync(
            positiveTurnKey,
            TimeSpan.FromMilliseconds(300),
            cancellationToken);
        await _input.TapAsync(
            GameKey.Jump,
            TimeSpan.FromMilliseconds(120),
            cancellationToken);
        await Task.Delay(TimeSpan.FromMilliseconds(200), cancellationToken);
    }

    private PlayerKinematics CapturePlayer() =>
        _objectManager.Capture().LocalPlayerKinematics
        ?? throw new InvalidOperationException("Local player position is unavailable.");

    private async Task<bool> ExecuteVerifiedTurnAsync(
        GameKey turnKey,
        float absoluteError,
        float facingBefore,
        CancellationToken cancellationToken)
    {
        TimeSpan turnPulse = CalculateTurnPulse(absoluteError, _failedTurnPulses);
        await _input.TapAsync(turnKey, turnPulse, cancellationToken);
        await Task.Delay(TimeSpan.FromMilliseconds(100), cancellationToken);

        float facingAfter = CapturePlayer().FacingRadians;
        float measuredDelta = MathF.Abs(AngleMath.NormalizeSigned(
            facingAfter - facingBefore));
        if (measuredDelta >= 0.01f)
        {
            _failedTurnPulses = 0;
            return true;
        }

        _failedTurnPulses++;
        Console.WriteLine(
            $"Turn pulse produced no measurable rotation " +
            $"({_failedTurnPulses}/6, pulse={turnPulse.TotalMilliseconds:F0} ms).");
        return false;
    }

    private TimeSpan CalculateTurnPulse(
        float absoluteError,
        int failedTurnPulses)
    {
        double minimumMilliseconds = failedTurnPulses switch
        {
            >= 4 => 220.0,
            >= 2 => 150.0,
            >= 1 => 110.0,
            _ => MinimumTurnPulseMilliseconds
        };

        double estimatedMilliseconds = _measuredTurnRateRadiansPerSecond is > 0.01f
            ? absoluteError / _measuredTurnRateRadiansPerSecond.Value *
              1000.0 * TurnPulseGain
            : absoluteError / 3.0f * 1000.0f;
        double milliseconds = Math.Clamp(
            estimatedMilliseconds,
            minimumMilliseconds,
            250.0f);
        return TimeSpan.FromMilliseconds(milliseconds);
    }

    private TimeSpan CalculateForwardPulse(float remainingDistance)
    {
        float desiredTravel = MathF.Max(
            0.10f,
            remainingDistance - _settings.ArrivalRadius * 0.5f);
        double milliseconds = desiredTravel / _estimatedForwardSpeed * 1000.0;
        milliseconds = Math.Clamp(
            milliseconds,
            _settings.MinimumForwardPulse.TotalMilliseconds,
            _settings.ForwardPulse.TotalMilliseconds);
        return TimeSpan.FromMilliseconds(milliseconds);
    }

    private void UpdateForwardSpeedEstimate(
        float moved,
        float progress,
        TimeSpan pulseDuration)
    {
        if (progress <= 0.03f || pulseDuration.TotalSeconds <= 0.0)
            return;

        float observedSpeed = moved / (float)pulseDuration.TotalSeconds;
        if (observedSpeed is < 2.0f or > 15.0f)
            return;

        _estimatedForwardSpeed =
            _estimatedForwardSpeed * 0.75f + observedSpeed * 0.25f;
    }

    private static float DistanceToSegment2D(
        WorldPosition point,
        WorldPosition segmentStart,
        WorldPosition segmentEnd)
    {
        float segmentX = segmentEnd.X - segmentStart.X;
        float segmentY = segmentEnd.Y - segmentStart.Y;
        float lengthSquared = segmentX * segmentX + segmentY * segmentY;
        if (lengthSquared <= 0.000001f)
            return point.Distance2DTo(segmentStart);

        float projection =
            ((point.X - segmentStart.X) * segmentX +
             (point.Y - segmentStart.Y) * segmentY) / lengthSquared;
        projection = Math.Clamp(projection, 0.0f, 1.0f);

        var closest = new WorldPosition(
            segmentStart.X + segmentX * projection,
            segmentStart.Y + segmentY * projection,
            segmentStart.Z);
        return point.Distance2DTo(closest);
    }

    private void TransitionTo(MovementState nextState)
    {
        if (_state == nextState)
            return;

        Console.WriteLine($"State: {_state} -> {nextState}");
        _state = nextState;
    }
}
