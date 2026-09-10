namespace WowBot.CachyOS.Objects;

public enum WowObjectType : uint
{
    None = 0,
    Item = 1,
    Container = 2,
    Unit = 3,
    Player = 4,
    GameObject = 5,
    DynamicObject = 6,
    Corpse = 7
}

public sealed record WowObject(
    uint Address,
    ulong Guid,
    WowObjectType Type,
    uint NextAddress);

public readonly record struct WorldPosition(float X, float Y, float Z)
{
    public float Distance2DTo(WorldPosition other)
    {
        float deltaX = other.X - X;
        float deltaY = other.Y - Y;
        return MathF.Sqrt(deltaX * deltaX + deltaY * deltaY);
    }

    public float Distance3DTo(WorldPosition other)
    {
        float deltaX = other.X - X;
        float deltaY = other.Y - Y;
        float deltaZ = other.Z - Z;
        return MathF.Sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
    }
}

public sealed record PlayerKinematics(
    uint ObjectAddress,
    WorldPosition Position,
    float FacingRadians);

public sealed record UnitSnapshot(
    uint ObjectAddress,
    ulong Guid,
    uint EntryId,
    uint Level,
    uint Health,
    uint MaximumHealth,
    ulong TargetGuid,
    uint FactionTemplateId,
    uint UnitFlags,
    uint NpcFlags,
    uint DynamicFlags,
    WorldPosition Position,
    float DistanceToPlayer)
{
    public bool IsAlive => Health > 0;
    public bool IsInCombat => (UnitFlags & 0x0008_0000u) != 0;
    public bool IsQuestGiver => (NpcFlags & 0x0000_0002u) != 0;
    public bool BlocksPlayerAttack =>
        (UnitFlags & (0x0000_0002u | 0x0000_0080u | 0x0000_0100u |
                      0x0001_0000u | 0x0200_0000u)) != 0;
    public float HealthPercent => MaximumHealth == 0
        ? 0.0f
        : Health * 100.0f / MaximumHealth;
}

public enum UnitPowerType : byte
{
    Mana = 0,
    Rage = 1,
    Focus = 2,
    Energy = 3,
    Happiness = 4,
    Unknown = byte.MaxValue
}

public sealed record PlayerCombatSnapshot(
    uint ObjectAddress,
    ulong Guid,
    uint Level,
    uint Health,
    uint MaximumHealth,
    uint Power,
    uint MaximumPower,
    UnitPowerType PowerType,
    byte RaceId,
    byte ClassId,
    byte GenderId,
    ulong TargetGuid,
    uint FactionTemplateId,
    uint UnitFlags,
    uint DynamicFlags,
    WorldPosition Position,
    float FacingRadians)
{
    public bool IsAlive => Health > 0;
    public bool IsInCombat => (UnitFlags & 0x0008_0000u) != 0;
    public float HealthPercent => MaximumHealth == 0
        ? 0.0f
        : Health * 100.0f / MaximumHealth;
    public float PowerPercent => MaximumPower == 0
        ? 0.0f
        : Power * 100.0f / MaximumPower;
}

public sealed record CombatScanSnapshot(
    DateTimeOffset CapturedAt,
    PlayerCombatSnapshot Player,
    IReadOnlyList<UnitSnapshot> Units,
    int VisibleUnitCount,
    int SkippedInvalidUnits);

public sealed record QuestLogEntry(
    int Slot,
    uint QuestId,
    byte StateFlags,
    IReadOnlyList<byte> ObjectiveCounts,
    uint TimerValue,
    uint RawStateAndCounts)
{
    public bool IsComplete => (StateFlags & 0x01) != 0;
    public bool IsFailed => (StateFlags & 0x02) != 0;
}

public sealed record QuestLogSnapshot(
    DateTimeOffset CapturedAt,
    uint PlayerLevel,
    IReadOnlyList<QuestLogEntry> Entries);

public sealed record DescriptorValueMatch(
    uint ByteOffset,
    uint FieldIndex,
    uint PreviousValue,
    uint MatchedValue,
    uint NextValue1,
    uint NextValue2);

public sealed record DescriptorProbeSnapshot(
    uint DescriptorAddress,
    uint SearchedValue,
    int ScannedBytes,
    IReadOnlyList<DescriptorValueMatch> Matches);

public sealed record UnitScanSnapshot(
    DateTimeOffset CapturedAt,
    PlayerKinematics Player,
    ulong PlayerTargetGuid,
    IReadOnlyList<UnitSnapshot> Units,
    int VisibleUnitCount,
    int SkippedInvalidUnits);

public sealed record WorldSnapshot(
    DateTimeOffset CapturedAt,
    uint ObjectManagerAddress,
    ulong LocalGuid,
    IReadOnlyList<WowObject> Objects,
    PlayerKinematics? LocalPlayerKinematics)
{
    public WowObject? LocalPlayer =>
        Objects.FirstOrDefault(item => item.Guid == LocalGuid && item.Type == WowObjectType.Player);
}
