using WowBot.CachyOS.Objects;

namespace WowBot.CachyOS.Runtime;

public sealed class UnitScanner
{
    private readonly ObjectManager _objectManager;
    private readonly float _radius;

    public UnitScanner(ObjectManager objectManager, float radius)
    {
        _objectManager = objectManager;
        _radius = radius;
    }

    public void Run()
    {
        UnitScanSnapshot scan = _objectManager.CaptureUnits(_radius);

        Console.WriteLine($"Scan radius:          {_radius:F1}");
        Console.WriteLine($"Visible unit objects: {scan.VisibleUnitCount}");
        Console.WriteLine($"Nearby valid units:   {scan.Units.Count}");
        Console.WriteLine($"Skipped invalid:      {scan.SkippedInvalidUnits}");
        Console.WriteLine($"Player target GUID:   0x{scan.PlayerTargetGuid:X16}");

        if (scan.Units.Count == 0)
        {
            Console.WriteLine("No valid units were found inside the scan radius.");
            return;
        }

        Console.WriteLine();
        Console.WriteLine(
            " Dist  Lvl      Entry       Health    HP% Alive " +
            "GUID               Target GUID");

        foreach (UnitSnapshot unit in scan.Units)
        {
            Console.WriteLine(
                $"{unit.DistanceToPlayer,5:F1} " +
                $"{unit.Level,4} " +
                $"{unit.EntryId,10} " +
                $"{unit.Health,6}/{unit.MaximumHealth,-6} " +
                $"{unit.HealthPercent,5:F1} " +
                $"{(unit.IsAlive ? "yes" : "no"),5} " +
                $"0x{unit.Guid:X16} " +
                $"0x{unit.TargetGuid:X16}");
            Console.WriteLine(
                $"      pos=({unit.Position.X:F3}, " +
                $"{unit.Position.Y:F3}, {unit.Position.Z:F3}) " +
                $"object=0x{unit.ObjectAddress:X8} " +
                $"npcFlags=0x{unit.NpcFlags:X8}");
        }
    }
}
