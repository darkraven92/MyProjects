namespace WowBot.CachyOS.Memory;

public interface IProcessMemory : IDisposable
{
    int ProcessId { get; }
    void ReadBytes(ulong address, Span<byte> destination);
    T Read<T>(ulong address) where T : unmanaged;
    uint ReadPointer32(ulong address);
}
