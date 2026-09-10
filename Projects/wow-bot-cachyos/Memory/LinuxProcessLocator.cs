using System.Globalization;

namespace WowBot.CachyOS.Memory;

public static class LinuxProcessLocator
{
    public static int FindWineProcess(string searchText)
    {
        foreach (string directory in Directory.EnumerateDirectories("/proc"))
        {
            if (!int.TryParse(Path.GetFileName(directory), out int pid))
                continue;

            try
            {
                string commandLine = File.ReadAllText(Path.Combine(directory, "cmdline"))
                    .Replace('\0', ' ');
                string comm = File.ReadAllText(Path.Combine(directory, "comm")).Trim();

                if (commandLine.Contains(searchText, StringComparison.OrdinalIgnoreCase) ||
                    comm.Contains(searchText, StringComparison.OrdinalIgnoreCase))
                    return pid;
            }
            catch (IOException)
            {
                // Process exited while /proc was being scanned.
            }
            catch (UnauthorizedAccessException)
            {
                // Process belongs to another user.
            }
        }

        throw new InvalidOperationException(
            $"Could not find a process matching '{searchText}'. Supply it explicitly with --pid.");
    }

    public static ulong FindModuleBase(int pid, string moduleSearchText)
    {
        string mapsPath = $"/proc/{pid}/maps";
        var candidates = new List<(ulong Start, ulong FileOffset)>();

        foreach (string line in File.ReadLines(mapsPath))
        {
            if (!line.Contains(moduleSearchText, StringComparison.OrdinalIgnoreCase))
                continue;

            string[] columns = line.Split(' ', StringSplitOptions.RemoveEmptyEntries);
            if (columns.Length < 5)
                continue;

            string[] range = columns[0].Split('-', 2);
            if (range.Length != 2 ||
                !ulong.TryParse(range[0], NumberStyles.HexNumber, CultureInfo.InvariantCulture, out ulong start) ||
                !ulong.TryParse(columns[2], NumberStyles.HexNumber, CultureInfo.InvariantCulture, out ulong offset))
                continue;

            candidates.Add((start, offset));
        }

        if (candidates.Count == 0)
            throw new InvalidOperationException(
                $"No mapping containing '{moduleSearchText}' was found in {mapsPath}.");

        var zeroOffsetMapping = candidates.Where(item => item.FileOffset == 0)
            .OrderBy(item => item.Start)
            .FirstOrDefault();

        return zeroOffsetMapping != default
            ? zeroOffsetMapping.Start
            : candidates.Min(item => item.Start - item.FileOffset);
    }
}
