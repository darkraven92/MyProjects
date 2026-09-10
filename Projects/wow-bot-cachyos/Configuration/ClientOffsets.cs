namespace WowBot.CachyOS.Configuration;

public sealed class ClientOffsets
{
    // RVA: address relative to the WoW.exe image base.
    public uint ObjectManagerPointerRva { get; init; }
    public uint FirstObjectFromObjectManager { get; init; }
    public uint LocalGuidFromObjectManager { get; init; }
    public uint NextObjectFromObject { get; init; }
    public uint ObjectTypeFromObject { get; init; }
    public uint GuidFromObject { get; init; }
    public uint DescriptorPointerFromObject { get; init; }
    public uint EntryIdFromDescriptors { get; init; }
    public uint UnitTargetGuidFromDescriptors { get; init; }
    public uint UnitHealthFromDescriptors { get; init; }
    public uint UnitPower1FromDescriptors { get; init; }
    public uint UnitMaxHealthFromDescriptors { get; init; }
    public uint UnitMaxPower1FromDescriptors { get; init; }
    public uint UnitLevelFromDescriptors { get; init; }
    public uint UnitFactionTemplateFromDescriptors { get; init; }
    public uint UnitBytes0FromDescriptors { get; init; }
    public uint UnitFlagsFromDescriptors { get; init; }
    public uint UnitNpcFlagsFromDescriptors { get; init; }
    public uint UnitDynamicFlagsFromDescriptors { get; init; }
    public uint PlayerQuestLog1FromDescriptors { get; init; }
    public uint PlayerQuestLogSlotStride { get; init; }
    public int PlayerQuestLogSlotCount { get; init; }
    public uint PositionXFromObject { get; init; }
    public uint PositionYFromObject { get; init; }
    public uint PositionZFromObject { get; init; }
    public uint FacingFromObject { get; init; }
    public int MaximumObjectCount { get; init; } = 4096;

    public void Validate()
    {
        if (ObjectManagerPointerRva == 0 ||
            FirstObjectFromObjectManager == 0 ||
            NextObjectFromObject == 0 ||
            ObjectTypeFromObject == 0 ||
            GuidFromObject == 0 ||
            DescriptorPointerFromObject == 0 ||
            EntryIdFromDescriptors == 0 ||
            UnitTargetGuidFromDescriptors == 0 ||
            UnitHealthFromDescriptors == 0 ||
            UnitPower1FromDescriptors == 0 ||
            UnitMaxHealthFromDescriptors == 0 ||
            UnitMaxPower1FromDescriptors == 0 ||
            UnitLevelFromDescriptors == 0 ||
            UnitFactionTemplateFromDescriptors == 0 ||
            UnitBytes0FromDescriptors == 0 ||
            UnitFlagsFromDescriptors == 0 ||
            UnitNpcFlagsFromDescriptors == 0 ||
            UnitDynamicFlagsFromDescriptors == 0 ||
            PlayerQuestLog1FromDescriptors == 0 ||
            PlayerQuestLogSlotStride == 0 ||
            PositionXFromObject == 0 ||
            PositionYFromObject == 0 ||
            PositionZFromObject == 0 ||
            FacingFromObject == 0)
        {
            throw new InvalidOperationException(
                "offsets.json contains invalid zero offsets.");
        }

        if (MaximumObjectCount is < 1 or > 100_000)
            throw new InvalidOperationException("MaximumObjectCount is outside the allowed range.");
        if (PlayerQuestLogSlotCount is < 1 or > 50)
            throw new InvalidOperationException("PlayerQuestLogSlotCount is outside the allowed range.");
    }
}
