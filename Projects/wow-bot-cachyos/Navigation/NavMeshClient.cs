using System.Runtime.InteropServices;
using System.Text;
using WowBot.CachyOS.Objects;

namespace WowBot.CachyOS.Navigation;

public sealed record NavMeshPath(
    IReadOnlyList<WorldPosition> Points,
    bool IsPartial,
    int LoadedTiles);

public sealed class NavMeshClient : IDisposable
{
    private const string LibraryName = "libwowbot_nav.so";
    private const int ErrorCapacity = 1024;
    private const int MaximumPoints = 256;
    private nint _handle;

    public NavMeshClient(string mmapsDirectory, uint mapId)
    {
        string libraryPath = Path.Combine(AppContext.BaseDirectory, LibraryName);
        if (!File.Exists(libraryPath))
            throw new FileNotFoundException(
                $"Native NavMesh library was not copied to {libraryPath}. " +
                "Build Native/build-native.sh, then rebuild the C# project.",
                libraryPath);

        string fullDirectory = Path.GetFullPath(mmapsDirectory);
        var error = new StringBuilder(ErrorCapacity);
        _handle = NativeMethods.Create(fullDirectory, mapId, error, error.Capacity);
        if (_handle == nint.Zero)
            throw new InvalidOperationException(
                $"Could not load NavMesh map {mapId}: {ReadError(error)}");

        LoadedTiles = NativeMethods.LoadedTiles(_handle);
    }

    public int LoadedTiles { get; }

    public NavMeshPath FindPath(WorldPosition start, WorldPosition target)
    {
        ObjectDisposedException.ThrowIf(_handle == nint.Zero, this);

        var points = new NativePoint[MaximumPoints];
        var error = new StringBuilder(ErrorCapacity);
        NativePoint nativeStart = NativePoint.FromWorld(start);
        NativePoint nativeTarget = NativePoint.FromWorld(target);
        int succeeded = NativeMethods.FindPath(
            _handle,
            in nativeStart,
            in nativeTarget,
            points,
            points.Length,
            out int pointCount,
            out int partial,
            error,
            error.Capacity);

        if (succeeded == 0)
            throw new InvalidOperationException(
                $"NavMesh path query failed: {ReadError(error)}");

        var result = new WorldPosition[pointCount];
        for (int index = 0; index < pointCount; ++index)
            result[index] = points[index].ToWorld();

        return new NavMeshPath(result, partial != 0, LoadedTiles);
    }

    public void Dispose()
    {
        if (_handle == nint.Zero)
            return;

        NativeMethods.Destroy(_handle);
        _handle = nint.Zero;
        GC.SuppressFinalize(this);
    }

    ~NavMeshClient()
    {
        if (_handle != nint.Zero)
            NativeMethods.Destroy(_handle);
    }

    private static string ReadError(StringBuilder error) =>
        error.Length == 0 ? "unknown native error" : error.ToString();

    [StructLayout(LayoutKind.Sequential)]
    private readonly struct NativePoint
    {
        public NativePoint(float x, float y, float z)
        {
            X = x;
            Y = y;
            Z = z;
        }

        public readonly float X;
        public readonly float Y;
        public readonly float Z;

        public static NativePoint FromWorld(WorldPosition value) =>
            new(value.X, value.Y, value.Z);

        public WorldPosition ToWorld() => new(X, Y, Z);
    }

    private static class NativeMethods
    {
        [DllImport(LibraryName, EntryPoint = "wow_nav_create",
            CallingConvention = CallingConvention.Cdecl)]
        public static extern nint Create(
            [MarshalAs(UnmanagedType.LPUTF8Str)] string mmapsDirectory,
            uint mapId,
            StringBuilder errorBuffer,
            int errorCapacity);

        [DllImport(LibraryName, EntryPoint = "wow_nav_destroy",
            CallingConvention = CallingConvention.Cdecl)]
        public static extern void Destroy(nint handle);

        [DllImport(LibraryName, EntryPoint = "wow_nav_loaded_tiles",
            CallingConvention = CallingConvention.Cdecl)]
        public static extern int LoadedTiles(nint handle);

        [DllImport(LibraryName, EntryPoint = "wow_nav_find_path",
            CallingConvention = CallingConvention.Cdecl)]
        public static extern int FindPath(
            nint handle,
            in NativePoint start,
            in NativePoint target,
            [Out, MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 4)]
            NativePoint[] outputPoints,
            int maximumPoints,
            out int outputPointCount,
            out int outputPartial,
            StringBuilder errorBuffer,
            int errorCapacity);
    }
}
