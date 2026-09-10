using System.ComponentModel;
using System.Runtime.InteropServices;

namespace WowBot.CachyOS.Memory;

public sealed class LinuxProcessMemory : IProcessMemory
{
    public int ProcessId { get; }

    public LinuxProcessMemory(int processId)
    {
        if (!Directory.Exists($"/proc/{processId}"))
            throw new ArgumentException($"PID {processId} does not exist.", nameof(processId));

        ProcessId = processId;
    }

    public unsafe void ReadBytes(ulong address, Span<byte> destination)
    {
        if (destination.IsEmpty)
            return;

        fixed (byte* destinationPointer = destination)
        {
            var local = new Iovec
            {
                Base = (nint)destinationPointer,
                Length = (nuint)destination.Length
            };
            var remote = new Iovec
            {
                Base = checked((nint)address),
                Length = (nuint)destination.Length
            };

            nint bytesRead = process_vm_readv(ProcessId, ref local, 1, ref remote, 1, 0);
            if (bytesRead == -1)
                throw new Win32Exception(Marshal.GetLastPInvokeError(),
                    $"process_vm_readv failed at 0x{address:X}");
            if ((nuint)bytesRead != (nuint)destination.Length)
                throw new IOException(
                    $"Short memory read at 0x{address:X}: expected {destination.Length}, received {bytesRead}.");
        }
    }

    public T Read<T>(ulong address) where T : unmanaged
    {
        Span<byte> buffer = stackalloc byte[Marshal.SizeOf<T>()];
        ReadBytes(address, buffer);
        return MemoryMarshal.Read<T>(buffer);
    }

    // The target is WoW x86, so an in-game pointer is always four bytes.
    public uint ReadPointer32(ulong address) => Read<uint>(address);

    public void Dispose()
    {
        // process_vm_readv does not require an open process handle.
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct Iovec
    {
        public nint Base;
        public nuint Length;
    }

    [DllImport("libc", SetLastError = true)]
    private static extern nint process_vm_readv(
        int pid,
        ref Iovec localIov,
        nuint localIovCount,
        ref Iovec remoteIov,
        nuint remoteIovCount,
        nuint flags);
}
