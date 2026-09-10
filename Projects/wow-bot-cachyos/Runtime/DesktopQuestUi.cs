using System.ComponentModel;
using System.Diagnostics;
using System.Text;
using WowBot.CachyOS.Input;

namespace WowBot.CachyOS.Runtime;

internal sealed class DesktopQuestUi
{
    private readonly IScreenInputDriver _input;
    private readonly string _temporaryDirectory;
    private int _captureNumber;
    private DesktopWindow? _window;

    public DesktopQuestUi(IScreenInputDriver input)
    {
        _input = input;
        _temporaryDirectory = Path.Combine(
            Path.GetTempPath(),
            $"wowbot-quest-ui-{Environment.ProcessId}");
        Directory.CreateDirectory(_temporaryDirectory);
    }

    public static async Task ValidateDependenciesAsync(
        CancellationToken cancellationToken)
    {
        var xdotool = CreateProcessStartInfo("xdotool");
        xdotool.RedirectStandardOutput = true;
        xdotool.ArgumentList.Add("--version");
        await RunAsync(xdotool, cancellationToken);

        var tesseract = CreateProcessStartInfo("tesseract");
        tesseract.RedirectStandardOutput = true;
        tesseract.ArgumentList.Add("--list-langs");

        Process process = StartProcess(tesseract);
        using (process)
        {
            string output = await process.StandardOutput.ReadToEndAsync(cancellationToken);
            string error = await process.StandardError.ReadToEndAsync(cancellationToken);
            await process.WaitForExitAsync(cancellationToken);
            if (process.ExitCode != 0)
            {
                throw new InvalidOperationException(
                    $"Tesseract language check failed: {error.Trim()}");
            }

            string[] languages = output.Split(
                ['\r', '\n'],
                StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
            if (!languages.Contains("eng", StringComparer.OrdinalIgnoreCase))
            {
                throw new InvalidOperationException(
                    "Tesseract English language data is missing. " +
                    "Install the CachyOS package tesseract-data-eng before running this mode.");
            }
        }

        Console.WriteLine("Quest UI dependencies: xdotool and Tesseract English data verified.");
    }

    public async Task<DesktopFrame> CaptureAsync(CancellationToken cancellationToken)
    {
        _window ??= await ReadActiveWindowAsync(cancellationToken);

        string imagePath = Path.Combine(
            _temporaryDirectory,
            $"wow-window-{++_captureNumber:D3}.png");

        var capture = CreateProcessStartInfo("spectacle");
        capture.ArgumentList.Add("--background");
        capture.ArgumentList.Add("--nonotify");
        capture.ArgumentList.Add("--activewindow");
        capture.ArgumentList.Add("--no-decoration");
        capture.ArgumentList.Add("--no-shadow");
        capture.ArgumentList.Add("--output");
        capture.ArgumentList.Add(imagePath);
        await RunAsync(capture, cancellationToken);

        if (!File.Exists(imagePath))
            throw new InvalidOperationException("Spectacle did not create a desktop screenshot.");

        (int width, int height) = ReadPngSize(imagePath);
        ValidateWindowScale(_window, width, height);
        IReadOnlyList<OcrWord> words = await ReadWordsAsync(imagePath, cancellationToken);
        return new DesktopFrame(imagePath, width, height, _window, words);
    }

    private static async Task<DesktopWindow> ReadActiveWindowAsync(
        CancellationToken cancellationToken)
    {
        var geometry = CreateProcessStartInfo("xdotool");
        geometry.RedirectStandardOutput = true;
        geometry.ArgumentList.Add("getactivewindow");
        geometry.ArgumentList.Add("getwindowgeometry");
        geometry.ArgumentList.Add("--shell");

        Process process = StartProcess(geometry);
        using (process)
        {
            string output = await process.StandardOutput.ReadToEndAsync(cancellationToken);
            string error = await process.StandardError.ReadToEndAsync(cancellationToken);
            await process.WaitForExitAsync(cancellationToken);
            if (process.ExitCode != 0)
            {
                throw new InvalidOperationException(
                    $"xdotool could not read the active XWayland window: {error.Trim()}");
            }

            Dictionary<string, int> values = output.Split('\n')
                .Select(line => line.Trim())
                .Where(line => line.Contains('='))
                .Select(line => line.Split('=', 2))
                .Where(parts => int.TryParse(parts[1], out _))
                .ToDictionary(
                    parts => parts[0],
                    parts => int.Parse(parts[1], System.Globalization.CultureInfo.InvariantCulture),
                    StringComparer.OrdinalIgnoreCase);

            if (!values.TryGetValue("X", out int x) ||
                !values.TryGetValue("Y", out int y) ||
                !values.TryGetValue("WIDTH", out int width) ||
                !values.TryGetValue("HEIGHT", out int height) ||
                width < 320 || height < 240)
            {
                throw new InvalidOperationException(
                    $"xdotool returned incomplete window geometry: {output.Trim()}");
            }

            Console.WriteLine(
                $"Active WoW window: X={x}, Y={y}, width={width}, height={height}");
            return new DesktopWindow(x, y, width, height);
        }
    }

    private static void ValidateWindowScale(
        DesktopWindow window,
        int imageWidth,
        int imageHeight)
    {
        float scaleX = window.Width / (float)imageWidth;
        float scaleY = window.Height / (float)imageHeight;
        if (!float.IsFinite(scaleX) || !float.IsFinite(scaleY) ||
            scaleX is < 0.5f or > 2.0f || scaleY is < 0.5f or > 2.0f)
        {
            throw new InvalidOperationException(
                $"Active-window screenshot {imageWidth}x{imageHeight} does not match " +
                $"the XWayland window {window.Width}x{window.Height}.");
        }
    }

    public async Task ClickAsync(OcrMatch match, CancellationToken cancellationToken)
    {
        await ClickAsync(match.CenterX, match.CenterY, MouseButton.Left, cancellationToken);
    }

    public async Task ClickAsync(
        int x,
        int y,
        MouseButton button,
        CancellationToken cancellationToken)
    {
        await _input.MoveMouseAbsoluteAsync(x, y, cancellationToken);
        await Task.Delay(TimeSpan.FromMilliseconds(100), cancellationToken);
        await _input.ClickMouseAsync(button, cancellationToken);
    }

    public void Cleanup()
    {
        try
        {
            if (Directory.Exists(_temporaryDirectory))
                Directory.Delete(_temporaryDirectory, recursive: true);
        }
        catch (IOException)
        {
        }
        catch (UnauthorizedAccessException)
        {
        }
    }

    private static async Task<IReadOnlyList<OcrWord>> ReadWordsAsync(
        string imagePath,
        CancellationToken cancellationToken)
    {
        var ocr = CreateProcessStartInfo("tesseract");
        ocr.RedirectStandardOutput = true;
        ocr.ArgumentList.Add(imagePath);
        ocr.ArgumentList.Add("stdout");
        ocr.ArgumentList.Add("-l");
        ocr.ArgumentList.Add("eng");
        ocr.ArgumentList.Add("--psm");
        ocr.ArgumentList.Add("11");
        ocr.ArgumentList.Add("tsv");

        Process process = StartProcess(ocr);
        using (process)
        {
            string output = await process.StandardOutput.ReadToEndAsync(cancellationToken);
            string error = await process.StandardError.ReadToEndAsync(cancellationToken);
            await process.WaitForExitAsync(cancellationToken);
            if (process.ExitCode != 0)
            {
                throw new InvalidOperationException(
                    $"Tesseract exited with code {process.ExitCode}: {error.Trim()}");
            }

            var words = new List<OcrWord>();
            foreach (string line in output.Split('\n').Skip(1))
            {
                string[] columns = line.TrimEnd('\r').Split('\t');
                if (columns.Length < 12 || string.IsNullOrWhiteSpace(columns[11]))
                    continue;

                if (!int.TryParse(columns[1], out int page) ||
                    !int.TryParse(columns[2], out int block) ||
                    !int.TryParse(columns[3], out int paragraph) ||
                    !int.TryParse(columns[4], out int lineNumber) ||
                    !int.TryParse(columns[6], out int left) ||
                    !int.TryParse(columns[7], out int top) ||
                    !int.TryParse(columns[8], out int width) ||
                    !int.TryParse(columns[9], out int height))
                {
                    continue;
                }

                words.Add(new OcrWord(
                    page,
                    block,
                    paragraph,
                    lineNumber,
                    left,
                    top,
                    width,
                    height,
                    columns[11]));
            }

            return words;
        }
    }

    private static (int Width, int Height) ReadPngSize(string path)
    {
        byte[] header = new byte[24];
        using FileStream stream = File.OpenRead(path);
        if (stream.Read(header, 0, header.Length) != header.Length ||
            header[0] != 0x89 || header[1] != 0x50 ||
            header[2] != 0x4E || header[3] != 0x47)
        {
            throw new InvalidDataException("Spectacle output is not a valid PNG file.");
        }

        int width = ReadBigEndianInt32(header, 16);
        int height = ReadBigEndianInt32(header, 20);
        if (width is < 320 or > 20_000 || height is < 240 or > 20_000)
            throw new InvalidDataException($"Implausible screenshot size {width}x{height}.");
        return (width, height);
    }

    private static int ReadBigEndianInt32(byte[] bytes, int offset) =>
        (bytes[offset] << 24) |
        (bytes[offset + 1] << 16) |
        (bytes[offset + 2] << 8) |
        bytes[offset + 3];

    private static ProcessStartInfo CreateProcessStartInfo(string fileName) => new()
    {
        FileName = fileName,
        UseShellExecute = false,
        RedirectStandardError = true,
        CreateNoWindow = true
    };

    private static Process StartProcess(ProcessStartInfo startInfo)
    {
        try
        {
            return Process.Start(startInfo)
                ?? throw new InvalidOperationException($"{startInfo.FileName} did not start.");
        }
        catch (Win32Exception exception)
        {
            throw new InvalidOperationException(
                $"{startInfo.FileName} was not found. The automatic quest UI requires " +
                "Spectacle, Tesseract with English data, xdotool, and ydotool.",
                exception);
        }
    }

    private static async Task RunAsync(
        ProcessStartInfo startInfo,
        CancellationToken cancellationToken)
    {
        Process process = StartProcess(startInfo);
        using (process)
        {
            string error = await process.StandardError.ReadToEndAsync(cancellationToken);
            await process.WaitForExitAsync(cancellationToken);
            if (process.ExitCode != 0)
            {
                throw new InvalidOperationException(
                    $"{startInfo.FileName} exited with code {process.ExitCode}: {error.Trim()}");
            }
        }
    }
}

internal sealed record DesktopFrame(
    string ImagePath,
    int Width,
    int Height,
    DesktopWindow Window,
    IReadOnlyList<OcrWord> Words)
{
    public (int X, int Y) ToDesktopPoint(float relativeX, float relativeY)
    {
        if (relativeX is < 0.0f or > 1.0f || relativeY is < 0.0f or > 1.0f)
            throw new ArgumentOutOfRangeException(nameof(relativeX));
        return (
            Window.X + (int)MathF.Round(Window.Width * relativeX),
            Window.Y + (int)MathF.Round(Window.Height * relativeY));
    }

    public OcrMatch? FindPhrase(string phrase) =>
        FindPhrases(phrase).FirstOrDefault();

    public IReadOnlyList<OcrMatch> FindPhrases(string phrase)
    {
        string[] wanted = phrase.Split(' ', StringSplitOptions.RemoveEmptyEntries)
            .Select(Normalize)
            .Where(value => value.Length > 0)
            .ToArray();
        if (wanted.Length == 0)
            return Array.Empty<OcrMatch>();

        var matches = new List<OcrMatch>();

        foreach (IGrouping<(int, int, int, int), OcrWord> group in Words.GroupBy(
                     word => (word.Page, word.Block, word.Paragraph, word.Line)))
        {
            OcrWord[] line = group.OrderBy(word => word.Left).ToArray();
            for (int start = 0; start + wanted.Length <= line.Length; start++)
            {
                bool isMatch = true;
                for (int index = 0; index < wanted.Length; index++)
                {
                    if (Normalize(line[start + index].Text) != wanted[index])
                    {
                        isMatch = false;
                        break;
                    }
                }

                if (!isMatch)
                    continue;

                OcrWord first = line[start];
                OcrWord last = line[start + wanted.Length - 1];
                int left = first.Left;
                int top = line.Skip(start).Take(wanted.Length).Min(word => word.Top);
                int right = last.Left + last.Width;
                int bottom = line.Skip(start).Take(wanted.Length)
                    .Max(word => word.Top + word.Height);
                float scaleX = Window.Width / (float)Width;
                float scaleY = Window.Height / (float)Height;
                matches.Add(new OcrMatch(
                    phrase,
                    Window.X + (int)MathF.Round(left * scaleX),
                    Window.Y + (int)MathF.Round(top * scaleY),
                    Math.Max(1, (int)MathF.Round((right - left) * scaleX)),
                    Math.Max(1, (int)MathF.Round((bottom - top) * scaleY))));
            }
        }

        return matches;
    }

    public string DebugText()
    {
        var builder = new StringBuilder();
        foreach (IGrouping<(int, int, int, int), OcrWord> line in Words.GroupBy(
                     word => (word.Page, word.Block, word.Paragraph, word.Line)))
        {
            string text = string.Join(' ', line.OrderBy(word => word.Left).Select(word => word.Text));
            if (!string.IsNullOrWhiteSpace(text))
                builder.AppendLine(text);
        }

        return builder.ToString().Trim();
    }

    private static string Normalize(string value)
    {
        var builder = new StringBuilder(value.Length);
        foreach (char character in value)
        {
            if (char.IsLetterOrDigit(character))
                builder.Append(char.ToLowerInvariant(character));
        }
        return builder.ToString();
    }
}

internal sealed record DesktopWindow(int X, int Y, int Width, int Height);

internal sealed record OcrWord(
    int Page,
    int Block,
    int Paragraph,
    int Line,
    int Left,
    int Top,
    int Width,
    int Height,
    string Text);

internal sealed record OcrMatch(
    string Phrase,
    int Left,
    int Top,
    int Width,
    int Height)
{
    public int CenterX => Left + Width / 2;
    public int CenterY => Top + Height / 2;
}
