using System.Text;

namespace WowBot.CachyOS.GameData;

public enum FactionReaction
{
    Unknown,
    Friendly,
    Neutral,
    Hostile
}

public sealed record FactionTemplateEntry(
    uint Id,
    uint FactionId,
    uint Flags,
    uint FactionGroupMask,
    uint FriendGroupMask,
    uint EnemyGroupMask,
    IReadOnlyList<uint> EnemyFactions,
    IReadOnlyList<uint> FriendFactions);

public sealed class FactionTemplateStore
{
    private const int HeaderSize = 20;
    private const int RequiredFieldCount = 14;
    private const int RequiredRecordSize = RequiredFieldCount * sizeof(uint);

    private readonly IReadOnlyDictionary<uint, FactionTemplateEntry> _entries;

    private FactionTemplateStore(IReadOnlyDictionary<uint, FactionTemplateEntry> entries)
    {
        _entries = entries;
    }

    public int Count => _entries.Count;

    public static FactionTemplateStore Load(string dbcDirectory)
    {
        string path = Path.Combine(dbcDirectory, "FactionTemplate.dbc");
        if (!File.Exists(path))
            throw new FileNotFoundException("FactionTemplate.dbc was not found.", path);

        using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
        using var reader = new BinaryReader(stream, Encoding.UTF8, leaveOpen: false);

        if (stream.Length < HeaderSize)
            throw new InvalidDataException("FactionTemplate.dbc is smaller than a WDBC header.");

        string magic = Encoding.ASCII.GetString(reader.ReadBytes(4));
        if (magic != "WDBC")
            throw new InvalidDataException(
                $"FactionTemplate.dbc has unsupported magic '{magic}'. Expected WDBC.");

        uint recordCount = reader.ReadUInt32();
        uint fieldCount = reader.ReadUInt32();
        uint recordSize = reader.ReadUInt32();
        uint stringBlockSize = reader.ReadUInt32();

        if (recordCount == 0 || recordCount > 100_000)
            throw new InvalidDataException("FactionTemplate.dbc has an invalid record count.");
        if (fieldCount < RequiredFieldCount || recordSize < RequiredRecordSize)
            throw new InvalidDataException(
                $"FactionTemplate.dbc layout is too small: fields={fieldCount}, recordSize={recordSize}.");

        ulong requiredLength = HeaderSize +
                               (ulong)recordCount * recordSize +
                               stringBlockSize;
        if ((ulong)stream.Length < requiredLength)
            throw new InvalidDataException("FactionTemplate.dbc is truncated.");

        var entries = new Dictionary<uint, FactionTemplateEntry>((int)recordCount);
        for (uint record = 0; record < recordCount; record++)
        {
            long recordStart = stream.Position;
            uint id = reader.ReadUInt32();
            uint factionId = reader.ReadUInt32();
            uint flags = reader.ReadUInt32();
            uint factionGroupMask = reader.ReadUInt32();
            uint friendGroupMask = reader.ReadUInt32();
            uint enemyGroupMask = reader.ReadUInt32();
            uint[] enemyFactions = ReadFour(reader);
            uint[] friendFactions = ReadFour(reader);

            if (id != 0)
            {
                entries[id] = new FactionTemplateEntry(
                    id,
                    factionId,
                    flags,
                    factionGroupMask,
                    friendGroupMask,
                    enemyGroupMask,
                    enemyFactions,
                    friendFactions);
            }

            stream.Position = recordStart + recordSize;
        }

        return new FactionTemplateStore(entries);
    }

    public FactionReaction Resolve(uint playerTemplateId, uint unitTemplateId)
    {
        if (!_entries.TryGetValue(playerTemplateId, out FactionTemplateEntry? player) ||
            !_entries.TryGetValue(unitTemplateId, out FactionTemplateEntry? unit))
        {
            return FactionReaction.Unknown;
        }

        if (IsFriendlyTo(unit, player))
            return FactionReaction.Friendly;
        if (IsHostileTo(unit, player))
            return FactionReaction.Hostile;
        return FactionReaction.Neutral;
    }

    private static bool IsFriendlyTo(
        FactionTemplateEntry source,
        FactionTemplateEntry other)
    {
        if (source.FactionId == other.FactionId)
            return true;
        if ((other.FactionGroupMask & source.FriendGroupMask) != 0)
            return true;
        if ((source.FactionGroupMask & other.FriendGroupMask) != 0)
            return true;
        if (source.EnemyFactions.Contains(other.FactionId))
            return false;
        return source.FriendFactions.Contains(other.FactionId);
    }

    private static bool IsHostileTo(
        FactionTemplateEntry source,
        FactionTemplateEntry other)
    {
        if (source.FactionId == other.FactionId)
            return false;
        if ((other.FactionGroupMask & source.EnemyGroupMask) != 0)
            return true;
        if ((source.FactionGroupMask & other.EnemyGroupMask) != 0)
            return true;
        if (source.EnemyFactions.Contains(other.FactionId))
            return true;
        if (source.FriendFactions.Contains(other.FactionId))
            return false;
        return false;
    }

    private static uint[] ReadFour(BinaryReader reader) =>
    [
        reader.ReadUInt32(),
        reader.ReadUInt32(),
        reader.ReadUInt32(),
        reader.ReadUInt32()
    ];
}
