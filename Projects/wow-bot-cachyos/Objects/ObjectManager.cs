using WowBot.CachyOS.Configuration;
using WowBot.CachyOS.Memory;

namespace WowBot.CachyOS.Objects;

public sealed class ObjectManager
{
    private const uint MinimumUserAddress = 0x0001_0000;
    private const float MaximumWorldCoordinate = 100_000.0f;
    private const uint MaximumPlausibleHealth = 1_000_000_000;

    private readonly IProcessMemory _memory;
    private readonly ulong _moduleBase;
    private readonly ClientOffsets _offsets;

    public ObjectManager(IProcessMemory memory, ulong moduleBase, ClientOffsets offsets)
    {
        _memory = memory;
        _moduleBase = moduleBase;
        _offsets = offsets;
    }

    public WorldSnapshot Capture()
    {
        // Build 1.12.1.5875 stores a direct global ObjectManager pointer.
        uint objectManager = _memory.ReadPointer32(_moduleBase + _offsets.ObjectManagerPointerRva);
        EnsurePointer(objectManager, "ObjectManager");

        ulong localGuid = _memory.Read<ulong>(
            (ulong)objectManager + _offsets.LocalGuidFromObjectManager);
        uint current = _memory.ReadPointer32(
            (ulong)objectManager + _offsets.FirstObjectFromObjectManager);

        var objects = new List<WowObject>();
        var visited = new HashSet<uint>();

        while (IsPlausiblePointer(current) &&
               objects.Count < _offsets.MaximumObjectCount &&
               visited.Add(current))
        {
            ulong guid = _memory.Read<ulong>((ulong)current + _offsets.GuidFromObject);
            WowObjectType type = (WowObjectType)_memory.Read<uint>(
                (ulong)current + _offsets.ObjectTypeFromObject);
            if ((uint)type > (uint)WowObjectType.Corpse)
                throw new InvalidDataException(
                    $"Object at 0x{current:X8} has invalid type value {(uint)type}.");

            uint next = _memory.ReadPointer32((ulong)current + _offsets.NextObjectFromObject);

            objects.Add(new WowObject(current, guid, type, next));

            if (next == current)
                break;

            current = next;
        }

        WowObject? localPlayer = objects.FirstOrDefault(
            item => item.Guid == localGuid && item.Type == WowObjectType.Player);
        PlayerKinematics? kinematics = localPlayer is null
            ? null
            : ReadKinematics(localPlayer.Address);

        return new WorldSnapshot(
            DateTimeOffset.UtcNow,
            objectManager,
            localGuid,
            objects,
            kinematics);
    }

    public UnitScanSnapshot CaptureUnits(float maximumDistance)
    {
        if (!float.IsFinite(maximumDistance) || maximumDistance is < 1.0f or > 200.0f)
            throw new ArgumentOutOfRangeException(
                nameof(maximumDistance),
                "Unit scan distance must be between 1 and 200 game units.");

        WorldSnapshot world = Capture();
        PlayerKinematics player = world.LocalPlayerKinematics
            ?? throw new InvalidOperationException("Local player position is unavailable.");
        WowObject localPlayer = world.LocalPlayer
            ?? throw new InvalidOperationException("Local player object is unavailable.");

        ulong playerTargetGuid = ReadUnitTargetGuid(localPlayer.Address);
        List<UnitSnapshot> units = ScanUnits(
            world.Objects,
            player.Position,
            maximumDistance,
            out int visibleUnitCount,
            out int skippedInvalidUnits);
        return new UnitScanSnapshot(
            world.CapturedAt,
            player,
            playerTargetGuid,
            units,
            visibleUnitCount,
            skippedInvalidUnits);
    }

    public CombatScanSnapshot CaptureCombat(float maximumDistance)
    {
        if (!float.IsFinite(maximumDistance) || maximumDistance is < 1.0f or > 200.0f)
            throw new ArgumentOutOfRangeException(
                nameof(maximumDistance),
                "Combat scan distance must be between 1 and 200 game units.");

        WorldSnapshot world = Capture();
        WowObject localPlayer = world.LocalPlayer
            ?? throw new InvalidOperationException("Local player object is unavailable.");
        PlayerKinematics kinematics = world.LocalPlayerKinematics
            ?? throw new InvalidOperationException("Local player position is unavailable.");
        PlayerCombatSnapshot player = ReadPlayerCombat(localPlayer, kinematics);
        List<UnitSnapshot> units = ScanUnits(
            world.Objects,
            player.Position,
            maximumDistance,
            out int visibleUnitCount,
            out int skippedInvalidUnits);

        return new CombatScanSnapshot(
            world.CapturedAt,
            player,
            units,
            visibleUnitCount,
            skippedInvalidUnits);
    }

    public QuestLogSnapshot CaptureQuestLog()
    {
        WorldSnapshot world = Capture();
        WowObject localPlayer = world.LocalPlayer
            ?? throw new InvalidOperationException("Local player object is unavailable.");
        uint descriptors = _memory.ReadPointer32(
            (ulong)localPlayer.Address + _offsets.DescriptorPointerFromObject);
        EnsurePointer(descriptors, "Local player descriptors");

        uint playerLevel = _memory.Read<uint>(
            (ulong)descriptors + _offsets.UnitLevelFromDescriptors);
        if (playerLevel is < 1 or > 60)
            throw new InvalidDataException($"Player level is implausible: {playerLevel}.");

        var entries = new List<QuestLogEntry>();
        for (int slot = 0; slot < _offsets.PlayerQuestLogSlotCount; slot++)
        {
            ulong slotAddress = (ulong)descriptors +
                                _offsets.PlayerQuestLog1FromDescriptors +
                                (ulong)slot * _offsets.PlayerQuestLogSlotStride;
            uint questId = _memory.Read<uint>(slotAddress);
            uint stateAndCounts = _memory.Read<uint>(slotAddress + 4);
            uint timerValue = _memory.Read<uint>(slotAddress + 8);

            if (questId == 0)
                continue;
            if (questId > 100_000)
                throw new InvalidDataException(
                    $"Quest slot {slot + 1} contains implausible quest id {questId}.");

            // Build 1.12.1 packs four six-bit objective counters into bits 0-23
            // and quest state flags into bits 24-31.
            byte stateFlags = (byte)(stateAndCounts >> 24);
            byte[] objectiveCounts =
            [
                (byte)(stateAndCounts & 0x3F),
                (byte)((stateAndCounts >> 6) & 0x3F),
                (byte)((stateAndCounts >> 12) & 0x3F),
                (byte)((stateAndCounts >> 18) & 0x3F)
            ];
            entries.Add(new QuestLogEntry(
                slot + 1,
                questId,
                stateFlags,
                objectiveCounts,
                timerValue,
                stateAndCounts));
        }

        return new QuestLogSnapshot(world.CapturedAt, playerLevel, entries);
    }

    public DescriptorProbeSnapshot ProbePlayerDescriptors(
        uint searchedValue,
        int scanBytes = 0x2000)
    {
        if (searchedValue == 0)
            throw new ArgumentOutOfRangeException(nameof(searchedValue));
        if (scanBytes is < 256 or > 0x10000 || (scanBytes & 3) != 0)
            throw new ArgumentOutOfRangeException(nameof(scanBytes));

        WorldSnapshot world = Capture();
        WowObject localPlayer = world.LocalPlayer
            ?? throw new InvalidOperationException("Local player object is unavailable.");
        uint descriptors = _memory.ReadPointer32(
            (ulong)localPlayer.Address + _offsets.DescriptorPointerFromObject);
        EnsurePointer(descriptors, "Local player descriptors");

        var matches = new List<DescriptorValueMatch>();
        for (int offset = 0; offset < scanBytes; offset += 4)
        {
            uint value = _memory.Read<uint>((ulong)descriptors + (ulong)offset);
            if (value != searchedValue)
                continue;

            uint previous = offset >= 4
                ? _memory.Read<uint>((ulong)descriptors + (ulong)(offset - 4))
                : 0;
            uint next1 = offset + 4 < scanBytes
                ? _memory.Read<uint>((ulong)descriptors + (ulong)(offset + 4))
                : 0;
            uint next2 = offset + 8 < scanBytes
                ? _memory.Read<uint>((ulong)descriptors + (ulong)(offset + 8))
                : 0;
            matches.Add(new DescriptorValueMatch(
                (uint)offset,
                (uint)(offset / 4),
                previous,
                value,
                next1,
                next2));
        }

        return new DescriptorProbeSnapshot(
            descriptors,
            searchedValue,
            scanBytes,
            matches);
    }

    private List<UnitSnapshot> ScanUnits(
        IReadOnlyList<WowObject> objects,
        WorldPosition playerPosition,
        float maximumDistance,
        out int visibleUnitCount,
        out int skippedInvalidUnits)
    {
        visibleUnitCount = 0;
        skippedInvalidUnits = 0;
        var units = new List<UnitSnapshot>();

        foreach (WowObject item in objects)
        {
            if (item.Type != WowObjectType.Unit)
                continue;

            visibleUnitCount++;
            if (!TryReadUnit(item, playerPosition, maximumDistance, out UnitSnapshot? unit))
            {
                skippedInvalidUnits++;
                continue;
            }

            if (unit is not null)
                units.Add(unit);
        }

        units.Sort((left, right) => left.DistanceToPlayer.CompareTo(right.DistanceToPlayer));
        return units;
    }

    private PlayerKinematics ReadKinematics(uint playerAddress)
    {
        float x = _memory.Read<float>((ulong)playerAddress + _offsets.PositionXFromObject);
        float y = _memory.Read<float>((ulong)playerAddress + _offsets.PositionYFromObject);
        float z = _memory.Read<float>((ulong)playerAddress + _offsets.PositionZFromObject);
        float facing = _memory.Read<float>((ulong)playerAddress + _offsets.FacingFromObject);

        if (!float.IsFinite(x) || !float.IsFinite(y) ||
            !float.IsFinite(z) || !float.IsFinite(facing))
            throw new InvalidDataException(
                $"Player at 0x{playerAddress:X8} contains non-finite movement values.");

        return new PlayerKinematics(
            playerAddress,
            new WorldPosition(x, y, z),
            facing);
    }

    private bool TryReadUnit(
        WowObject item,
        WorldPosition playerPosition,
        float maximumDistance,
        out UnitSnapshot? snapshot)
    {
        snapshot = null;

        try
        {
            uint descriptors = _memory.ReadPointer32(
                (ulong)item.Address + _offsets.DescriptorPointerFromObject);
            if (!IsPlausiblePointer(descriptors))
                return false;

            uint entryId = _memory.Read<uint>(
                (ulong)descriptors + _offsets.EntryIdFromDescriptors);
            uint health = _memory.Read<uint>(
                (ulong)descriptors + _offsets.UnitHealthFromDescriptors);
            uint maximumHealth = _memory.Read<uint>(
                (ulong)descriptors + _offsets.UnitMaxHealthFromDescriptors);
            uint level = _memory.Read<uint>(
                (ulong)descriptors + _offsets.UnitLevelFromDescriptors);
            ulong targetGuid = _memory.Read<ulong>(
                (ulong)descriptors + _offsets.UnitTargetGuidFromDescriptors);
            uint factionTemplateId = _memory.Read<uint>(
                (ulong)descriptors + _offsets.UnitFactionTemplateFromDescriptors);
            uint unitFlags = _memory.Read<uint>(
                (ulong)descriptors + _offsets.UnitFlagsFromDescriptors);
            uint npcFlags = _memory.Read<uint>(
                (ulong)descriptors + _offsets.UnitNpcFlagsFromDescriptors);
            uint dynamicFlags = _memory.Read<uint>(
                (ulong)descriptors + _offsets.UnitDynamicFlagsFromDescriptors);

            var position = new WorldPosition(
                _memory.Read<float>((ulong)item.Address + _offsets.PositionXFromObject),
                _memory.Read<float>((ulong)item.Address + _offsets.PositionYFromObject),
                _memory.Read<float>((ulong)item.Address + _offsets.PositionZFromObject));

            if (entryId == 0 || level is < 1 or > 255 ||
                maximumHealth == 0 || maximumHealth > MaximumPlausibleHealth ||
                health > maximumHealth || !IsPlausiblePosition(position))
            {
                return false;
            }

            float distance = playerPosition.Distance3DTo(position);
            if (!float.IsFinite(distance) || distance > maximumDistance)
                return true;

            snapshot = new UnitSnapshot(
                item.Address,
                item.Guid,
                entryId,
                level,
                health,
                maximumHealth,
                targetGuid,
                factionTemplateId,
                unitFlags,
                npcFlags,
                dynamicFlags,
                position,
                distance);
            return true;
        }
        catch (InvalidOperationException)
        {
            return false;
        }
        catch (InvalidDataException)
        {
            return false;
        }
        catch (IOException)
        {
            return false;
        }
        catch (System.ComponentModel.Win32Exception)
        {
            return false;
        }
    }

    private PlayerCombatSnapshot ReadPlayerCombat(
        WowObject localPlayer,
        PlayerKinematics kinematics)
    {
        uint descriptors = _memory.ReadPointer32(
            (ulong)localPlayer.Address + _offsets.DescriptorPointerFromObject);
        EnsurePointer(descriptors, "Local player descriptors");

        uint health = _memory.Read<uint>(
            (ulong)descriptors + _offsets.UnitHealthFromDescriptors);
        uint maximumHealth = _memory.Read<uint>(
            (ulong)descriptors + _offsets.UnitMaxHealthFromDescriptors);
        uint level = _memory.Read<uint>(
            (ulong)descriptors + _offsets.UnitLevelFromDescriptors);
        uint bytes0 = _memory.Read<uint>(
            (ulong)descriptors + _offsets.UnitBytes0FromDescriptors);
        byte raceId = (byte)bytes0;
        byte classId = (byte)(bytes0 >> 8);
        byte genderId = (byte)(bytes0 >> 16);
        byte rawPowerType = (byte)(bytes0 >> 24);
        UnitPowerType powerType = rawPowerType <= (byte)UnitPowerType.Happiness
            ? (UnitPowerType)rawPowerType
            : UnitPowerType.Unknown;
        int powerIndex = powerType == UnitPowerType.Unknown ? 0 : rawPowerType;
        uint power = _memory.Read<uint>(
            (ulong)descriptors + _offsets.UnitPower1FromDescriptors + (uint)(powerIndex * 4));
        uint maximumPower = _memory.Read<uint>(
            (ulong)descriptors + _offsets.UnitMaxPower1FromDescriptors + (uint)(powerIndex * 4));
        ulong targetGuid = _memory.Read<ulong>(
            (ulong)descriptors + _offsets.UnitTargetGuidFromDescriptors);
        uint factionTemplateId = _memory.Read<uint>(
            (ulong)descriptors + _offsets.UnitFactionTemplateFromDescriptors);
        uint unitFlags = _memory.Read<uint>(
            (ulong)descriptors + _offsets.UnitFlagsFromDescriptors);
        uint dynamicFlags = _memory.Read<uint>(
            (ulong)descriptors + _offsets.UnitDynamicFlagsFromDescriptors);

        if (level is < 1 or > 255 || maximumHealth == 0 ||
            maximumHealth > MaximumPlausibleHealth || health > maximumHealth ||
            maximumPower > MaximumPlausibleHealth || power > maximumPower ||
            raceId == 0 || classId == 0)
        {
            throw new InvalidDataException(
                "The local player contains implausible combat descriptor values.");
        }

        return new PlayerCombatSnapshot(
            localPlayer.Address,
            localPlayer.Guid,
            level,
            health,
            maximumHealth,
            power,
            maximumPower,
            powerType,
            raceId,
            classId,
            genderId,
            targetGuid,
            factionTemplateId,
            unitFlags,
            dynamicFlags,
            kinematics.Position,
            kinematics.FacingRadians);
    }

    private ulong ReadUnitTargetGuid(uint objectAddress)
    {
        uint descriptors = _memory.ReadPointer32(
            (ulong)objectAddress + _offsets.DescriptorPointerFromObject);
        EnsurePointer(descriptors, "Local player descriptors");
        return _memory.Read<ulong>(
            (ulong)descriptors + _offsets.UnitTargetGuidFromDescriptors);
    }

    private static bool IsPlausiblePosition(WorldPosition position) =>
        float.IsFinite(position.X) &&
        float.IsFinite(position.Y) &&
        float.IsFinite(position.Z) &&
        MathF.Abs(position.X) <= MaximumWorldCoordinate &&
        MathF.Abs(position.Y) <= MaximumWorldCoordinate &&
        MathF.Abs(position.Z) <= MaximumWorldCoordinate;

    // TSExplicitList uses bit 0 to mark its end sentinel.
    private static bool IsPlausiblePointer(uint address) =>
        address >= MinimumUserAddress && (address & 1u) == 0;

    private static void EnsurePointer(uint address, string name)
    {
        if (!IsPlausiblePointer(address))
            throw new InvalidDataException($"{name} pointer is invalid: 0x{address:X8}");
    }
}
