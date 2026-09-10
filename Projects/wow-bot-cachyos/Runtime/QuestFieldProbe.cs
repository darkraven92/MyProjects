using WowBot.CachyOS.Objects;

namespace WowBot.CachyOS.Runtime;

public sealed class QuestFieldProbe
{
    private readonly ObjectManager _objectManager;
    private readonly uint _questId;

    public QuestFieldProbe(ObjectManager objectManager, uint questId)
    {
        _objectManager = objectManager;
        _questId = questId;
    }

    public void Run()
    {
        DescriptorProbeSnapshot probe = _objectManager.ProbePlayerDescriptors(_questId);

        Console.WriteLine("Player descriptor value probe completed");
        Console.WriteLine($"Descriptor address: 0x{probe.DescriptorAddress:X8}");
        Console.WriteLine($"Quest ID searched:  {probe.SearchedValue}");
        Console.WriteLine($"Bytes scanned:      0x{probe.ScannedBytes:X}");
        Console.WriteLine($"Matches:            {probe.Matches.Count}");

        foreach (DescriptorValueMatch match in probe.Matches)
        {
            Console.WriteLine(
                $"  byteOffset=0x{match.ByteOffset:X4}, fieldIndex=0x{match.FieldIndex:X4}, " +
                $"previous=0x{match.PreviousValue:X8}, value={match.MatchedValue}, " +
                $"next1=0x{match.NextValue1:X8}, next2=0x{match.NextValue2:X8}");
        }

        if (probe.Matches.Count == 0)
        {
            Console.WriteLine(
                "The quest ID was not present in the scanned descriptor range. " +
                "Do not change offsets.json without another diagnostic.");
        }

        Console.WriteLine("No input or process-memory writes were sent.");
    }
}
