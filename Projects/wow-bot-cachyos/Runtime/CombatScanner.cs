using WowBot.CachyOS.GameData;
using WowBot.CachyOS.Objects;

namespace WowBot.CachyOS.Runtime;

public sealed class CombatScanner
{
    private readonly ObjectManager _objectManager;
    private readonly float _radius;
    private readonly FactionTemplateStore _factions;

    public CombatScanner(
        ObjectManager objectManager,
        float radius,
        FactionTemplateStore factions)
    {
        _objectManager = objectManager;
        _radius = radius;
        _factions = factions;
    }

    public void Run()
    {
        CombatScanSnapshot scan = _objectManager.CaptureCombat(_radius);
        PlayerCombatSnapshot player = scan.Player;
        string powerLabel = $"{player.PowerType}:";

        Console.WriteLine("Combat descriptor scan succeeded");
        Console.WriteLine($"Player:               level {player.Level} {ClassName(player.ClassId)} " +
                          $"(race={player.RaceId}, class={player.ClassId}, gender={player.GenderId})");
        Console.WriteLine($"Health:               {player.Health}/{player.MaximumHealth} " +
                          $"({player.HealthPercent:F1}%)");
        Console.WriteLine($"{powerLabel,-21}" +
                          $"{player.Power}/{player.MaximumPower} ({player.PowerPercent:F1}%)");
        Console.WriteLine($"In combat:            {(player.IsInCombat ? "yes" : "no")}");
        Console.WriteLine($"Player faction:       {player.FactionTemplateId}");
        Console.WriteLine($"Faction templates:    {_factions.Count}");
        Console.WriteLine($"Player flags:         unit=0x{player.UnitFlags:X8}, " +
                          $"dynamic=0x{player.DynamicFlags:X8}");
        Console.WriteLine($"Player GUID:          0x{player.Guid:X16}");
        Console.WriteLine($"Player target GUID:   0x{player.TargetGuid:X16}");
        Console.WriteLine($"Scan radius:          {_radius:F1}");
        Console.WriteLine($"Visible unit objects: {scan.VisibleUnitCount}");
        Console.WriteLine($"Nearby valid units:   {scan.Units.Count}");
        Console.WriteLine($"Skipped invalid:      {scan.SkippedInvalidUnits}");

        if (scan.Units.Count == 0)
        {
            Console.WriteLine("No valid units were found inside the scan radius.");
            return;
        }

        Console.WriteLine();
        Console.WriteLine(
            " Dist Lvl  Entry Faction Reaction       Health Combat Targeted Safety gate");

        foreach (UnitSnapshot unit in scan.Units)
        {
            FactionReaction reaction = _factions.Resolve(
                player.FactionTemplateId,
                unit.FactionTemplateId);
            string safetyGate = SafetyGate(player, unit, reaction);
            Console.WriteLine(
                $"{unit.DistanceToPlayer,5:F1} " +
                $"{unit.Level,3} " +
                $"{unit.EntryId,6} " +
                $"{unit.FactionTemplateId,7} " +
                $"{reaction,-8} " +
                $"{unit.Health,5}/{unit.MaximumHealth,-5} " +
                $"{(unit.IsInCombat ? "yes" : "no"),6} " +
                $"{(unit.Guid == player.TargetGuid ? "yes" : "no"),8} " +
                safetyGate);
            Console.WriteLine(
                $"      guid=0x{unit.Guid:X16} target=0x{unit.TargetGuid:X16} " +
                $"unitFlags=0x{unit.UnitFlags:X8} npcFlags=0x{unit.NpcFlags:X8} " +
                $"dynamic=0x{unit.DynamicFlags:X8}");
        }

        Console.WriteLine();
        Console.WriteLine("No movement, targeting, attacks, or process-memory writes were sent.");
    }

    private static string SafetyGate(
        PlayerCombatSnapshot player,
        UnitSnapshot unit,
        FactionReaction reaction)
    {
        if (!unit.IsAlive)
            return "blocked:dead";
        if (unit.BlocksPlayerAttack)
            return "blocked:unit-flags";
        if (unit.Level > player.Level + 2)
            return "blocked:high-level";
        if (unit.TargetGuid != 0 && unit.TargetGuid != player.Guid)
            return "blocked:engaged-other";
        return reaction switch
        {
            FactionReaction.Friendly => "blocked:friendly",
            FactionReaction.Hostile => "candidate:hostile",
            FactionReaction.Neutral => "route-only:neutral",
            _ => "blocked:faction-unknown"
        };
    }

    private static string ClassName(byte classId) => classId switch
    {
        1 => "Warrior",
        2 => "Paladin",
        3 => "Hunter",
        4 => "Rogue",
        5 => "Priest",
        7 => "Shaman",
        8 => "Mage",
        9 => "Warlock",
        11 => "Druid",
        _ => "Unknown"
    };
}
