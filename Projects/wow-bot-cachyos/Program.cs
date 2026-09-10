using System.Text.Json;
using WowBot.CachyOS.Configuration;
using WowBot.CachyOS.GameData;
using WowBot.CachyOS.Input;
using WowBot.CachyOS.Memory;
using WowBot.CachyOS.Navigation;
using WowBot.CachyOS.Objects;
using WowBot.CachyOS.Runtime;

namespace WowBot.CachyOS;

internal static class Program
{
    private static async Task<int> Main(string[] args)
    {
        using var cancellation = new CancellationTokenSource();
        Console.CancelKeyPress += (_, eventArgs) =>
        {
            eventArgs.Cancel = true;
            cancellation.Cancel();
        };

        try
        {
            BotOptions options = BotOptions.Parse(args);

            if (options.InspectQuestRoutePath is not null)
            {
                var inspector = new QuestRouteInspector(options.InspectQuestRoutePath);
                inspector.Run();
                return 0;
            }

            int pid = options.ProcessId ?? LinuxProcessLocator.FindWineProcess(options.ProcessSearchText);

            string offsetsJson = await File.ReadAllTextAsync(options.OffsetsPath, cancellation.Token);
            ClientOffsets offsets = JsonSerializer.Deserialize<ClientOffsets>(offsetsJson,
                new JsonSerializerOptions { PropertyNameCaseInsensitive = true })
                ?? throw new InvalidOperationException("Could not deserialize offsets.json.");
            offsets.Validate();

            using var memory = new LinuxProcessMemory(pid);
            ulong moduleBase = LinuxProcessLocator.FindModuleBase(pid, options.ModuleSearchText);

            Console.WriteLine($"Attached to PID {pid}");
            Console.WriteLine($"WoW module base: 0x{moduleBase:X}");
            string mode = options switch
            {
                { MoveTest: true } => "BoundedMoveTest",
                { NavigationTest: true } => "BoundedNavigationTest",
                { NavMeshTest: true } => "ReadOnlyNavMeshTest",
                { NavMeshNavigationTest: true } => "BoundedNavMeshNavigationTest",
                { ScanUnits: true } => "ReadOnlyUnitScan",
                { CombatScan: true } => "ReadOnlyCombatScan",
                { QuestScan: true } => "ReadOnlyQuestScan",
                { QuestFieldProbeValue: not null } => "ReadOnlyQuestFieldProbe",
                { NpcTargetTest: true } => "BoundedNpcTargetTest",
                { QuestTurnInTest: true } => "BoundedQuestTurnInTest",
                { RecordRoutePath: not null } => "RouteRecorder",
                { FollowRoutePath: not null } => "RouteFollower",
                _ => "ObserveOnly"
            };
            Console.WriteLine($"Mode: {mode}. Press Ctrl+C to stop.");

            var objectManager = new ObjectManager(memory, moduleBase, offsets);

            if (options.ProbeOnly)
            {
                WorldSnapshot probe = objectManager.Capture();
                PrintProbe(probe);
                return 0;
            }

            if (options.ScanUnits)
            {
                var scanner = new UnitScanner(objectManager, options.ScanRadius);
                scanner.Run();
                return 0;
            }

            if (options.CombatScan)
            {
                FactionTemplateStore factions = FactionTemplateStore.Load(options.DbcDirectory);
                var scanner = new CombatScanner(objectManager, options.ScanRadius, factions);
                scanner.Run();
                return 0;
            }

            if (options.QuestScan)
            {
                var scanner = new QuestScanner(objectManager, options.QuestRoutePath);
                scanner.Run();
                return 0;
            }

            if (options.QuestFieldProbeValue.HasValue)
            {
                var probe = new QuestFieldProbe(objectManager, options.QuestFieldProbeValue.Value);
                probe.Run();
                return 0;
            }

            if (options.NpcTargetTest)
            {
                IInputDriver npcInput = new YdotoolInputDriver();
                var npcTest = new NpcTargetTestRunner(
                    objectManager,
                    npcInput,
                    options.NavMeshDirectory,
                    options.NavMeshMapId,
                    options.NpcEntryId,
                    options.RequiredQuestId);
                await npcTest.RunAsync(cancellation.Token);
                return 0;
            }

            if (options.QuestTurnInTest)
            {
                IScreenInputDriver questInput = new YdotoolInputDriver();
                var turnInTest = new QuestTurnInTestRunner(
                    objectManager,
                    questInput,
                    options.NavMeshDirectory,
                    options.NavMeshMapId,
                    options.NpcEntryId,
                    options.RequiredQuestId);
                await turnInTest.RunAsync(cancellation.Token);
                return 0;
            }

            if (options.MoveTest)
            {
                IInputDriver movementInput = new YdotoolInputDriver();
                var movementTest = new MovementTestRunner(
                    objectManager,
                    movementInput,
                    TimeSpan.FromMilliseconds(options.MoveMilliseconds));
                await movementTest.RunAsync(cancellation.Token);
                return 0;
            }

            if (options.NavMeshTest)
            {
                var navMeshTest = new NavMeshTestRunner(
                    objectManager,
                    options.NavMeshDirectory,
                    options.NavMeshMapId,
                    new WorldPosition(
                        options.NavMeshTargetX!.Value,
                        options.NavMeshTargetY!.Value,
                        options.NavMeshTargetZ!.Value));
                navMeshTest.Run();
                return 0;
            }

            if (options.NavMeshNavigationTest)
            {
                IInputDriver navMeshInput = new YdotoolInputDriver();
                var navMeshNavigationTest = new NavMeshNavigationTestRunner(
                    objectManager,
                    navMeshInput,
                    options.NavMeshDirectory,
                    options.NavMeshMapId,
                    new WorldPosition(
                        options.NavMeshTargetX!.Value,
                        options.NavMeshTargetY!.Value,
                        options.NavMeshTargetZ!.Value));
                await navMeshNavigationTest.RunAsync(cancellation.Token);
                return 0;
            }

            if (options.NavigationTest)
            {
                IInputDriver navigationInput = new YdotoolInputDriver();
                var navigationTest = new NavigationTestRunner(
                    objectManager,
                    navigationInput,
                    options.NavigationDistance,
                    options.NavigationBearingDegrees);
                await navigationTest.RunAsync(cancellation.Token);
                return 0;
            }

            if (options.RecordRoutePath is not null)
            {
                var recorder = new RouteRecorder(
                    objectManager,
                    options.RecordRoutePath,
                    options.RouteSpacing);
                await recorder.RunAsync(cancellation.Token);
                return 0;
            }

            if (options.FollowRoutePath is not null)
            {
                IInputDriver routeInput = new YdotoolInputDriver();
                var follower = new RouteFollower(
                    objectManager,
                    routeInput,
                    options.FollowRoutePath,
                    options.StartAtNearestWaypoint,
                    options.ReverseRoute,
                    options.RouteLoopCount);
                await follower.RunAsync(cancellation.Token);
                return 0;
            }

            IInputDriver input = new NullInputDriver();
            IBotState initialState = new ObserveState();
            var bot = new BotController(objectManager, input, initialState, TimeSpan.FromMilliseconds(100));

            await bot.RunAsync(cancellation.Token);
            return 0;
        }
        catch (OperationCanceledException)
        {
            return 0;
        }
        catch (Exception exception)
        {
            Console.Error.WriteLine($"Fatal: {exception.Message}");
            return 1;
        }
    }

    private static void PrintProbe(WorldSnapshot world)
    {
        Console.WriteLine("Object Manager probe succeeded");
        Console.WriteLine($"ObjectManager: 0x{world.ObjectManagerAddress:X8}");
        Console.WriteLine($"Local GUID:    0x{world.LocalGuid:X16}");
        Console.WriteLine($"Objects:       {world.Objects.Count}");

        foreach (var group in world.Objects.GroupBy(item => item.Type).OrderBy(group => group.Key))
            Console.WriteLine($"  {group.Key,-13} {group.Count(),4}");

        if (world.LocalPlayer is not null)
            Console.WriteLine($"Local player:  0x{world.LocalPlayer.Address:X8}");
        else
            Console.WriteLine("Local player:  not present in the visible object list");

        if (world.LocalPlayerKinematics is not null)
        {
            PlayerKinematics player = world.LocalPlayerKinematics;
            Console.WriteLine(
                $"Position:      X={player.Position.X:F3}, Y={player.Position.Y:F3}, " +
                $"Z={player.Position.Z:F3}");
            Console.WriteLine($"Facing:        {player.FacingRadians:F4} radians");
        }
    }
}

internal sealed record BotOptions(
    int? ProcessId,
    string ProcessSearchText,
    string ModuleSearchText,
    string OffsetsPath,
    bool ProbeOnly,
    bool MoveTest,
    int MoveMilliseconds,
    bool NavigationTest,
    float NavigationDistance,
    float NavigationBearingDegrees,
    bool NavMeshTest,
    bool NavMeshNavigationTest,
    bool ScanUnits,
    bool CombatScan,
    bool QuestScan,
    uint? QuestFieldProbeValue,
    bool NpcTargetTest,
    bool QuestTurnInTest,
    uint NpcEntryId,
    uint RequiredQuestId,
    float ScanRadius,
    string DbcDirectory,
    string QuestRoutePath,
    string NavMeshDirectory,
    uint NavMeshMapId,
    float? NavMeshTargetX,
    float? NavMeshTargetY,
    float? NavMeshTargetZ,
    string? RecordRoutePath,
    string? FollowRoutePath,
    string? InspectQuestRoutePath,
    float RouteSpacing,
    bool StartAtNearestWaypoint,
    bool ReverseRoute,
    int RouteLoopCount)
{
    public static BotOptions Parse(string[] args)
    {
        int? pid = null;
        string process = "WoW.exe";
        string module = "WoW.exe";
        string offsets = Path.Combine(AppContext.BaseDirectory, "offsets.json");
        bool probeOnly = false;
        bool moveTest = false;
        int moveMilliseconds = 750;
        bool navigationTest = false;
        float navigationDistance = 8.0f;
        float navigationBearingDegrees = 45.0f;
        bool navMeshTest = false;
        bool navMeshNavigationTest = false;
        bool scanUnits = false;
        bool combatScan = false;
        bool questScan = false;
        uint? questFieldProbeValue = null;
        bool npcTargetTest = false;
        bool questTurnInTest = false;
        uint npcEntryId = 0;
        uint requiredQuestId = 0;
        float scanRadius = 50.0f;
        bool scanRadiusSpecified = false;
        string dbcDirectory = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
            "Games",
            "WoW-NavData",
            "dbc");
        bool dbcDirectorySpecified = false;
        string questRoutePath = Path.GetFullPath(
            Path.Combine("routes", "joana-horde-1-6.json"));
        bool questRouteSpecified = false;
        string navMeshDirectory = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
            "Games",
            "WoW-NavData",
            "mmaps");
        uint navMeshMapId = 0;
        float? navMeshTargetX = null;
        float? navMeshTargetY = null;
        float? navMeshTargetZ = null;
        string? recordRoutePath = null;
        string? followRoutePath = null;
        string? inspectQuestRoutePath = null;
        float routeSpacing = 2.0f;
        bool startAtNearestWaypoint = false;
        bool reverseRoute = false;
        int routeLoopCount = 1;

        for (int i = 0; i < args.Length; i++)
        {
            string value = args[i];
            switch (value)
            {
                case "--pid" when i + 1 < args.Length:
                    pid = int.Parse(args[++i]);
                    break;
                case "--process" when i + 1 < args.Length:
                    process = args[++i];
                    break;
                case "--module" when i + 1 < args.Length:
                    module = args[++i];
                    break;
                case "--offsets" when i + 1 < args.Length:
                    offsets = Path.GetFullPath(args[++i]);
                    break;
                case "--probe":
                    probeOnly = true;
                    break;
                case "--move-test":
                    moveTest = true;
                    break;
                case "--move-ms" when i + 1 < args.Length:
                    moveMilliseconds = int.Parse(args[++i]);
                    break;
                case "--navigate-test":
                    navigationTest = true;
                    break;
                case "--distance" when i + 1 < args.Length:
                    navigationDistance = ParseFloat(args[++i]);
                    break;
                case "--bearing-deg" when i + 1 < args.Length:
                    navigationBearingDegrees = ParseFloat(args[++i]);
                    break;
                case "--navmesh-test":
                    navMeshTest = true;
                    break;
                case "--navmesh-navigate-test":
                    navMeshNavigationTest = true;
                    break;
                case "--scan-units":
                    scanUnits = true;
                    break;
                case "--combat-scan":
                    combatScan = true;
                    break;
                case "--quest-scan":
                    questScan = true;
                    break;
                case "--quest-field-probe" when i + 1 < args.Length:
                    questFieldProbeValue = uint.Parse(args[++i]);
                    break;
                case "--npc-target-test":
                    npcTargetTest = true;
                    break;
                case "--quest-turnin-test":
                    questTurnInTest = true;
                    break;
                case "--complete-your-place":
                    questTurnInTest = true;
                    npcEntryId = 3143;
                    requiredQuestId = 4641;
                    navMeshMapId = 1;
                    break;
                case "--npc-entry" when i + 1 < args.Length:
                    npcEntryId = uint.Parse(args[++i]);
                    break;
                case "--required-quest" when i + 1 < args.Length:
                    requiredQuestId = uint.Parse(args[++i]);
                    break;
                case "--radius" when i + 1 < args.Length:
                    scanRadius = ParseFloat(args[++i]);
                    scanRadiusSpecified = true;
                    break;
                case "--dbc-dir" when i + 1 < args.Length:
                    dbcDirectory = Path.GetFullPath(args[++i]);
                    dbcDirectorySpecified = true;
                    break;
                case "--quest-route" when i + 1 < args.Length:
                    questRoutePath = Path.GetFullPath(args[++i]);
                    questRouteSpecified = true;
                    break;
                case "--navmesh-dir" when i + 1 < args.Length:
                    navMeshDirectory = Path.GetFullPath(args[++i]);
                    break;
                case "--map" when i + 1 < args.Length:
                    navMeshMapId = uint.Parse(args[++i]);
                    break;
                case "--target-x" when i + 1 < args.Length:
                    navMeshTargetX = ParseFloat(args[++i]);
                    break;
                case "--target-y" when i + 1 < args.Length:
                    navMeshTargetY = ParseFloat(args[++i]);
                    break;
                case "--target-z" when i + 1 < args.Length:
                    navMeshTargetZ = ParseFloat(args[++i]);
                    break;
                case "--record-route" when i + 1 < args.Length:
                    recordRoutePath = Path.GetFullPath(args[++i]);
                    break;
                case "--follow-route" when i + 1 < args.Length:
                    followRoutePath = Path.GetFullPath(args[++i]);
                    break;
                case "--inspect-quest-route" when i + 1 < args.Length:
                    inspectQuestRoutePath = Path.GetFullPath(args[++i]);
                    break;
                case "--spacing" when i + 1 < args.Length:
                    routeSpacing = ParseFloat(args[++i]);
                    break;
                case "--nearest":
                    startAtNearestWaypoint = true;
                    break;
                case "--reverse":
                    reverseRoute = true;
                    break;
                case "--loops" when i + 1 < args.Length:
                    routeLoopCount = int.Parse(args[++i]);
                    break;
                default:
                    throw new ArgumentException($"Unknown or incomplete argument: {value}");
            }
        }

        int selectedModes = (probeOnly ? 1 : 0) +
                            (moveTest ? 1 : 0) +
                            (navigationTest ? 1 : 0) +
                            (navMeshTest ? 1 : 0) +
                            (navMeshNavigationTest ? 1 : 0) +
                            (scanUnits ? 1 : 0) +
                            (combatScan ? 1 : 0) +
                            (questScan ? 1 : 0) +
                            (questFieldProbeValue.HasValue ? 1 : 0) +
                            (npcTargetTest ? 1 : 0) +
                            (questTurnInTest ? 1 : 0) +
                            (recordRoutePath is not null ? 1 : 0) +
                            (followRoutePath is not null ? 1 : 0) +
                            (inspectQuestRoutePath is not null ? 1 : 0);
        if (selectedModes > 1)
            throw new ArgumentException(
                "Only one run mode can be selected at a time.");
        if (moveMilliseconds is < 100 or > 2_000)
            throw new ArgumentOutOfRangeException(
                nameof(moveMilliseconds),
                "--move-ms must be between 100 and 2000.");
        if (navigationDistance is < 2.0f or > 20.0f)
            throw new ArgumentOutOfRangeException(
                nameof(navigationDistance),
                "--distance must be between 2 and 20 game units.");
        if (navigationBearingDegrees is < -180.0f or > 180.0f)
            throw new ArgumentOutOfRangeException(
                nameof(navigationBearingDegrees),
                "--bearing-deg must be between -180 and 180.");
        bool anyNavMeshTarget =
            navMeshTargetX.HasValue || navMeshTargetY.HasValue || navMeshTargetZ.HasValue;
        bool completeNavMeshTarget =
            navMeshTargetX.HasValue && navMeshTargetY.HasValue && navMeshTargetZ.HasValue;
        bool anyNavMeshMode = navMeshTest || navMeshNavigationTest;
        if (anyNavMeshMode && !completeNavMeshTarget)
            throw new ArgumentException(
                "The selected NavMesh mode requires --target-x, --target-y and --target-z.");
        if (!anyNavMeshMode && anyNavMeshTarget)
            throw new ArgumentException(
                "--target-x, --target-y and --target-z require a NavMesh mode.");
        if (navMeshMapId > 999)
            throw new ArgumentOutOfRangeException(
                nameof(navMeshMapId),
                "--map must be between 0 and 999.");
        if (scanRadius is < 5.0f or > 100.0f)
            throw new ArgumentOutOfRangeException(
                nameof(scanRadius),
                "--radius must be between 5 and 100 game units.");
        if (!scanUnits && !combatScan && scanRadiusSpecified)
            throw new ArgumentException("--radius requires --scan-units or --combat-scan.");
        if (!combatScan && dbcDirectorySpecified)
            throw new ArgumentException("--dbc-dir requires --combat-scan.");
        if (!questScan && questRouteSpecified)
            throw new ArgumentException("--quest-route requires --quest-scan.");
        if (questFieldProbeValue.HasValue &&
            (questFieldProbeValue.Value == 0 || questFieldProbeValue.Value > 100_000))
            throw new ArgumentOutOfRangeException(
                nameof(questFieldProbeValue),
                "--quest-field-probe must be a quest ID between 1 and 100000.");
        bool anyNpcArgument = npcEntryId != 0 || requiredQuestId != 0;
        bool anyNpcMode = npcTargetTest || questTurnInTest;
        if (anyNpcMode && (npcEntryId == 0 || requiredQuestId == 0))
        {
            throw new ArgumentException(
                "The selected NPC mode requires --npc-entry and --required-quest.");
        }
        if (!anyNpcMode && anyNpcArgument)
            throw new ArgumentException(
                "--npc-entry and --required-quest require an NPC mode.");
        if (npcEntryId > 1_000_000)
            throw new ArgumentOutOfRangeException(
                nameof(npcEntryId),
                "--npc-entry must be between 1 and 1000000.");
        if (requiredQuestId > 100_000)
            throw new ArgumentOutOfRangeException(
                nameof(requiredQuestId),
                "--required-quest must be between 1 and 100000.");
        if (routeSpacing is < 0.5f or > 10.0f)
            throw new ArgumentOutOfRangeException(
                nameof(routeSpacing),
                "--spacing must be between 0.5 and 10 game units.");
        if (routeLoopCount is < 1 or > 10)
            throw new ArgumentOutOfRangeException(
                nameof(routeLoopCount),
                "--loops must be between 1 and 10.");
        if (followRoutePath is null &&
            (startAtNearestWaypoint || reverseRoute || routeLoopCount != 1))
            throw new ArgumentException(
                "--nearest, --reverse and --loops require --follow-route.");

        return new BotOptions(
            pid,
            process,
            module,
            offsets,
            probeOnly,
            moveTest,
            moveMilliseconds,
            navigationTest,
            navigationDistance,
            navigationBearingDegrees,
            navMeshTest,
            navMeshNavigationTest,
            scanUnits,
            combatScan,
            questScan,
            questFieldProbeValue,
            npcTargetTest,
            questTurnInTest,
            npcEntryId,
            requiredQuestId,
            scanRadius,
            dbcDirectory,
            questRoutePath,
            navMeshDirectory,
            navMeshMapId,
            navMeshTargetX,
            navMeshTargetY,
            navMeshTargetZ,
            recordRoutePath,
            followRoutePath,
            inspectQuestRoutePath,
            routeSpacing,
            startAtNearestWaypoint,
            reverseRoute,
            routeLoopCount);
    }

    private static float ParseFloat(string value)
    {
        if (float.TryParse(
                value,
                System.Globalization.NumberStyles.Float,
                System.Globalization.CultureInfo.InvariantCulture,
                out float result) &&
            float.IsFinite(result))
        {
            return result;
        }

        throw new FormatException(
            $"'{value}' is not a valid finite number. Use a period as the decimal separator, " +
            "for example -612.215.");
    }
}
