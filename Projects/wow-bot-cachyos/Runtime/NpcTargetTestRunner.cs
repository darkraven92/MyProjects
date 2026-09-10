using System.ComponentModel;
using System.Diagnostics;
using WowBot.CachyOS.Input;
using WowBot.CachyOS.Navigation;
using WowBot.CachyOS.Objects;

namespace WowBot.CachyOS.Runtime;

public sealed class NpcTargetTestRunner
{
    private const float ScanRadius = 100.0f;
    private const float MaximumNpcDistance = 85.0f;
    private const float MaximumPathDistance = 90.0f;
    private const float MaximumPathSegmentDistance = 85.0f;
    private const float ApproachDistance = 3.25f;
    private const float MaximumTargetDistance = 5.5f;
    private const int MaximumPathPoints = 24;
    private const int MaximumTargetAttempts = 12;
    private const int MaximumFocusAttempts = 3;
    private static readonly TimeSpan MaximumTotalDuration = TimeSpan.FromSeconds(90);

    private readonly ObjectManager _objectManager;
    private readonly IInputDriver _input;
    private readonly string _mmapsDirectory;
    private readonly uint _mapId;
    private readonly uint _npcEntryId;
    private readonly uint _requiredQuestId;

    public NpcTargetTestRunner(
        ObjectManager objectManager,
        IInputDriver input,
        string mmapsDirectory,
        uint mapId,
        uint npcEntryId,
        uint requiredQuestId)
    {
        _objectManager = objectManager;
        _input = input;
        _mmapsDirectory = mmapsDirectory;
        _mapId = mapId;
        _npcEntryId = npcEntryId;
        _requiredQuestId = requiredQuestId;
    }

    public async Task RunAsync(CancellationToken cancellationToken)
    {
        await ApproachAndTargetAsync(cancellationToken);

        QuestLogEntry after = await WaitForCompletedQuestAsync(cancellationToken);
        Console.WriteLine(
            $"Quest remains active and complete: {_requiredQuestId}, " +
            $"raw=0x{after.RawStateAndCounts:X8}");
        Console.WriteLine("Result: NPC TARGET VERIFIED");
        Console.WriteLine(
            "The NPC is targeted. No chat command, interaction key, " +
            "Complete Quest, or Accept action was sent.");
    }

    public async Task<ulong> ApproachAndTargetAsync(CancellationToken cancellationToken)
    {
        QuestLogEntry quest = await WaitForCompletedQuestAsync(cancellationToken);
        UnitScanSnapshot initialScan = _objectManager.CaptureUnits(ScanRadius);
        UnitSnapshot npc = FindNpc(initialScan, guid: null);

        if (npc.DistanceToPlayer > MaximumNpcDistance)
        {
            throw new InvalidOperationException(
                $"NPC {_npcEntryId} is {npc.DistanceToPlayer:F1} units away; " +
                $"the bounded test permits at most {MaximumNpcDistance:F0}.");
        }

        WorldPosition approach = CalculateApproachPoint(
            initialScan.Player.Position,
            npc.Position);

        Console.WriteLine($"NPC Entry:         {_npcEntryId}");
        Console.WriteLine($"NPC GUID:          0x{npc.Guid:X16}");
        Console.WriteLine($"NPC flags:         0x{npc.NpcFlags:X8} (questgiver={npc.IsQuestGiver})");
        Console.WriteLine(
            $"NPC position:      X={npc.Position.X:F3}, " +
            $"Y={npc.Position.Y:F3}, Z={npc.Position.Z:F3}");
        Console.WriteLine($"Initial distance:  {npc.DistanceToPlayer:F3}");
        Console.WriteLine(
            $"Approach point:    X={approach.X:F3}, " +
            $"Y={approach.Y:F3}, Z={approach.Z:F3}");
        Console.WriteLine(
            $"Quest gate:        {_requiredQuestId}, complete=yes, " +
            $"raw=0x{quest.RawStateAndCounts:X8}");

        using var navMesh = new NavMeshClient(_mmapsDirectory, _mapId);
        NavMeshPath? path = null;
        if (npc.DistanceToPlayer > MaximumTargetDistance)
        {
            path = navMesh.FindPath(initialScan.Player.Position, approach);
            ValidatePath(path);
            Console.WriteLine($"Loaded tiles:      {path.LoadedTiles}");
            Console.WriteLine($"Path points:       {path.Points.Count}");
        }
        else
        {
            Console.WriteLine("Already inside the bounded target distance.");
        }

        await ActivateAndVerifyGameWindowAsync(cancellationToken);

        Console.WriteLine("WoW input focus was verified from live facing data.");
        Console.WriteLine("No chat text will be typed.");

        using var timeout = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        timeout.CancelAfter(MaximumTotalDuration);

        try
        {
            if (path is not null)
                await FollowPathAsync(path, timeout.Token);

            UnitScanSnapshot nearScan = _objectManager.CaptureUnits(ScanRadius);
            UnitSnapshot nearNpc = FindNpc(nearScan, npc.Guid);
            Console.WriteLine($"Distance after movement: {nearNpc.DistanceToPlayer:F3}");
            if (nearNpc.DistanceToPlayer > MaximumTargetDistance)
            {
                throw new InvalidOperationException(
                    $"Stopped {nearNpc.DistanceToPlayer:F2} units from the NPC; " +
                    "no target input was sent.");
            }

            Console.WriteLine(
                "Cycling Target Nearest Friend with Ctrl+Tab and verifying every GUID...");
            bool matched = await TargetNpcAsync(npc.Guid, timeout.Token);
            if (!matched)
            {
                throw new InvalidOperationException(
                    $"Could not select NPC GUID 0x{npc.Guid:X16} after " +
                    $"{MaximumTargetAttempts} bounded Ctrl+Tab attempts. " +
                    "No interaction input was sent.");
            }

            return npc.Guid;
        }
        finally
        {
            await _input.ReleaseAllAsync(CancellationToken.None);
        }
    }

    public async Task ReacquireTargetAsync(
        ulong expectedGuid,
        CancellationToken cancellationToken)
    {
        UnitSnapshot npc = FindNpc(
            _objectManager.CaptureUnits(ScanRadius),
            expectedGuid);
        if (npc.DistanceToPlayer > MaximumTargetDistance)
        {
            throw new InvalidOperationException(
                $"Cannot reacquire NPC 0x{expectedGuid:X16}: distance " +
                $"is {npc.DistanceToPlayer:F2}, maximum is {MaximumTargetDistance:F1}.");
        }

        if (!await TargetNpcAsync(expectedGuid, cancellationToken))
        {
            throw new InvalidOperationException(
                $"Could not reacquire verified NPC GUID 0x{expectedGuid:X16}. " +
                "No further interaction click was sent.");
        }
    }

    private async Task ActivateAndVerifyGameWindowAsync(
        CancellationToken cancellationToken)
    {
        await _input.ReleaseAllAsync(CancellationToken.None);

        for (int attempt = 1; attempt <= MaximumFocusAttempts; attempt++)
        {
            bool activatedByTitle = attempt == 1 &&
                await TryActivateWowWindowByTitleAsync(cancellationToken);
            if (!activatedByTitle)
                await SendAltTabAsync(cancellationToken);

            await Task.Delay(TimeSpan.FromMilliseconds(1400), cancellationToken);

            PlayerKinematics before = CapturePlayer();
            await _input.TapAsync(
                GameKey.TurnLeft,
                TimeSpan.FromMilliseconds(180),
                cancellationToken);
            await Task.Delay(TimeSpan.FromMilliseconds(180), cancellationToken);
            PlayerKinematics after = CapturePlayer();
            float delta = MathF.Abs(AngleMath.NormalizeSigned(
                after.FacingRadians - before.FacingRadians));

            Console.WriteLine(
                $"Focus verification {attempt}/{MaximumFocusAttempts}: " +
                $"facing delta={delta:F4} radians " +
                $"(activation={(activatedByTitle ? "window-title" : "Alt+Tab")})");
            if (delta >= 0.01f)
                return;

            await Task.Delay(TimeSpan.FromMilliseconds(400), cancellationToken);
        }

        throw new InvalidOperationException(
            "Could not verify that WoW owns keyboard focus after three attempts. " +
            "No navigation or quest interaction was started.");
    }

    private async Task SendAltTabAsync(CancellationToken cancellationToken)
    {
        await Task.Delay(TimeSpan.FromMilliseconds(250), cancellationToken);
        await _input.KeyDownAsync(GameKey.Alt, cancellationToken);
        try
        {
            await Task.Delay(TimeSpan.FromMilliseconds(120), cancellationToken);
            await _input.TapAsync(
                GameKey.TargetNearest,
                TimeSpan.FromMilliseconds(140),
                cancellationToken);
            await Task.Delay(TimeSpan.FromMilliseconds(120), cancellationToken);
        }
        finally
        {
            await _input.KeyUpAsync(GameKey.Alt, CancellationToken.None);
        }
    }

    private static async Task<bool> TryActivateWowWindowByTitleAsync(
        CancellationToken cancellationToken)
    {
        var startInfo = new ProcessStartInfo
        {
            FileName = "xdotool",
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true
        };
        startInfo.ArgumentList.Add("search");
        startInfo.ArgumentList.Add("--onlyvisible");
        startInfo.ArgumentList.Add("--name");
        startInfo.ArgumentList.Add("World of Warcraft");
        startInfo.ArgumentList.Add("windowactivate");

        try
        {
            using Process process = Process.Start(startInfo)
                ?? throw new InvalidOperationException("xdotool did not start.");
            _ = await process.StandardOutput.ReadToEndAsync(cancellationToken);
            string error = await process.StandardError.ReadToEndAsync(cancellationToken);
            await process.WaitForExitAsync(cancellationToken);
            if (process.ExitCode == 0)
            {
                Console.WriteLine("Requested WoW window activation through XWayland.");
                return true;
            }

            Console.WriteLine(
                "WoW title activation was unavailable; falling back to Alt+Tab" +
                (string.IsNullOrWhiteSpace(error) ? "." : $": {error.Trim()}"));
            return false;
        }
        catch (Win32Exception)
        {
            Console.WriteLine("xdotool was unavailable; falling back to Alt+Tab.");
            return false;
        }
    }

    private async Task<bool> TargetNpcAsync(
        ulong expectedGuid,
        CancellationToken cancellationToken)
    {
        ulong initialGuid = _objectManager.CaptureUnits(ScanRadius).PlayerTargetGuid;
        if (initialGuid == expectedGuid)
        {
            Console.WriteLine($"Target already verified: 0x{initialGuid:X16}");
            return true;
        }

        for (int attempt = 1; attempt <= MaximumTargetAttempts; attempt++)
        {
            await _input.KeyDownAsync(GameKey.Control, cancellationToken);
            try
            {
                await _input.TapAsync(
                    GameKey.TargetNearest,
                    TimeSpan.FromMilliseconds(60),
                    cancellationToken);
            }
            finally
            {
                await _input.KeyUpAsync(GameKey.Control, CancellationToken.None);
            }

            await Task.Delay(TimeSpan.FromMilliseconds(250), cancellationToken);
            ulong observedGuid = _objectManager.CaptureUnits(ScanRadius).PlayerTargetGuid;
            Console.WriteLine(
                $"Target attempt {attempt,2}/{MaximumTargetAttempts}: " +
                $"observed GUID=0x{observedGuid:X16}");
            if (observedGuid == expectedGuid)
            {
                Console.WriteLine($"Target GUID verified: 0x{observedGuid:X16}");
                return true;
            }
        }

        return false;
    }

    private async Task FollowPathAsync(
        NavMeshPath path,
        CancellationToken cancellationToken)
    {
        var navigator = new WaypointNavigator(
            _objectManager,
            _input,
            NavigationSettings.BoundedTest);

        for (int index = 1; index < path.Points.Count; index++)
        {
            WorldPosition point = path.Points[index];
            Console.WriteLine(
                $"NavMesh point {index}/{path.Points.Count - 1}: " +
                $"X={point.X:F3}, Y={point.Y:F3}");
            NavigationResult result = await navigator.NavigateAsync(
                point,
                cancellationToken);
            Console.WriteLine(
                $"Point result: {result.Outcome}, " +
                $"remaining={result.RemainingDistance:F3}, " +
                $"recoveries={result.Recoveries}");
            if (result.Outcome != NavigationOutcome.Arrived)
            {
                throw new InvalidOperationException(
                    $"Navigation stopped with outcome {result.Outcome}; " +
                    "no target input was sent.");
            }
        }
    }

    private PlayerKinematics CapturePlayer() =>
        _objectManager.Capture().LocalPlayerKinematics
        ?? throw new InvalidOperationException(
            "Local player position is unavailable during focus verification.");

    private async Task<QuestLogEntry> WaitForCompletedQuestAsync(
        CancellationToken cancellationToken)
    {
        QuestLogEntry? lastSeen = null;
        for (int attempt = 1; attempt <= 8; attempt++)
        {
            lastSeen = _objectManager.CaptureQuestLog().Entries
                .FirstOrDefault(entry => entry.QuestId == _requiredQuestId);
            if (lastSeen?.IsComplete == true)
                return lastSeen;

            if (attempt < 8)
                await Task.Delay(TimeSpan.FromMilliseconds(250), cancellationToken);
        }

        if (lastSeen is null)
        {
            throw new InvalidOperationException(
                $"Required quest {_requiredQuestId} was not present in any of 8 quest-log snapshots.");
        }

        throw new InvalidOperationException(
            $"Required quest {_requiredQuestId} is active but not complete " +
            $"(raw=0x{lastSeen.RawStateAndCounts:X8}).");
    }

    private UnitSnapshot FindNpc(UnitScanSnapshot scan, ulong? guid)
    {
        UnitSnapshot? npc = scan.Units
            .Where(unit => unit.EntryId == _npcEntryId && (!guid.HasValue || unit.Guid == guid.Value))
            .OrderBy(unit => unit.DistanceToPlayer)
            .FirstOrDefault();
        UnitSnapshot result = npc ?? throw new InvalidOperationException(
            guid.HasValue
                ? $"The selected NPC GUID 0x{guid.Value:X16} is no longer visible."
                : $"No visible unit with Entry ID {_npcEntryId} was found within {ScanRadius:F0} units.");
        if (!result.IsQuestGiver)
        {
            throw new InvalidOperationException(
                $"Unit {_npcEntryId} has NPC flags 0x{result.NpcFlags:X8} and is not marked as a questgiver.");
        }

        return result;
    }

    private static WorldPosition CalculateApproachPoint(
        WorldPosition player,
        WorldPosition npc)
    {
        float deltaX = player.X - npc.X;
        float deltaY = player.Y - npc.Y;
        float length = MathF.Sqrt(deltaX * deltaX + deltaY * deltaY);
        if (length < 0.01f)
            return player;

        return new WorldPosition(
            npc.X + deltaX / length * ApproachDistance,
            npc.Y + deltaY / length * ApproachDistance,
            npc.Z);
    }

    private static void ValidatePath(NavMeshPath path)
    {
        if (path.IsPartial)
            throw new InvalidOperationException(
                "Detour returned a partial NPC approach path; no movement was sent.");
        if (path.Points.Count is < 2 or > MaximumPathPoints)
        {
            throw new InvalidOperationException(
                $"The NPC approach path contains {path.Points.Count} points; " +
                $"the bounded test permits 2-{MaximumPathPoints}.");
        }

        float totalDistance = 0.0f;
        for (int index = 1; index < path.Points.Count; index++)
        {
            float horizontal = path.Points[index - 1].Distance2DTo(path.Points[index]);
            float vertical = MathF.Abs(path.Points[index].Z - path.Points[index - 1].Z);
            totalDistance += horizontal;
            if (horizontal > MaximumPathSegmentDistance || vertical > 8.0f)
            {
                throw new InvalidOperationException(
                    $"Unsafe path segment {index}: horizontal={horizontal:F1}, " +
                    $"vertical={vertical:F1}; no movement was sent.");
            }
        }

        if (totalDistance > MaximumPathDistance)
        {
            throw new InvalidOperationException(
                $"NPC approach path is {totalDistance:F1} units long; " +
                $"the bounded test permits at most {MaximumPathDistance:F0}. " +
                "No movement was sent.");
        }
    }
}
