using WowBot.CachyOS.Navigation;
using WowBot.CachyOS.Objects;

namespace WowBot.CachyOS.Runtime;

public sealed class NavMeshTestRunner
{
    private readonly ObjectManager _objectManager;
    private readonly string _mmapsDirectory;
    private readonly uint _mapId;
    private readonly WorldPosition _target;

    public NavMeshTestRunner(
        ObjectManager objectManager,
        string mmapsDirectory,
        uint mapId,
        WorldPosition target)
    {
        _objectManager = objectManager;
        _mmapsDirectory = mmapsDirectory;
        _mapId = mapId;
        _target = target;
    }

    public void Run()
    {
        PlayerKinematics player = _objectManager.Capture().LocalPlayerKinematics
            ?? throw new InvalidOperationException("Local player position is unavailable.");

        Console.WriteLine($"NavMesh directory: {_mmapsDirectory}");
        Console.WriteLine($"Map:               {_mapId}");
        Console.WriteLine(
            $"Start:             X={player.Position.X:F3}, " +
            $"Y={player.Position.Y:F3}, Z={player.Position.Z:F3}");
        Console.WriteLine(
            $"Target:            X={_target.X:F3}, " +
            $"Y={_target.Y:F3}, Z={_target.Z:F3}");

        using var navMesh = new NavMeshClient(_mmapsDirectory, _mapId);
        NavMeshPath path = navMesh.FindPath(player.Position, _target);

        Console.WriteLine($"Loaded tiles:       {path.LoadedTiles}");
        Console.WriteLine($"Path points:        {path.Points.Count}");
        Console.WriteLine($"Partial path:       {(path.IsPartial ? "yes" : "no")}");

        float totalDistance = 0.0f;
        for (int index = 0; index < path.Points.Count; ++index)
        {
            WorldPosition point = path.Points[index];
            if (index > 0)
                totalDistance += path.Points[index - 1].Distance3DTo(point);

            Console.WriteLine(
                $"  [{index,3}] X={point.X,10:F3}, " +
                $"Y={point.Y,10:F3}, Z={point.Z,8:F3}");
        }

        Console.WriteLine($"Path distance:      {totalDistance:F3}");
        Console.WriteLine("Result: NAVMESH QUERY SUCCEEDED (no movement input sent)");
    }
}

