using WowBot.CachyOS.Input;
using WowBot.CachyOS.Objects;

namespace WowBot.CachyOS.Runtime;

public sealed class QuestTurnInTestRunner
{
    private const float VerificationRadius = 100.0f;
    private const int MaximumWorldClickAttempts = 18;
    private const int MaximumUiActions = 8;

    private readonly ObjectManager _objectManager;
    private readonly IScreenInputDriver _input;
    private readonly NpcTargetTestRunner _targetRunner;
    private readonly uint _questId;

    public QuestTurnInTestRunner(
        ObjectManager objectManager,
        IScreenInputDriver input,
        string mmapsDirectory,
        uint mapId,
        uint npcEntryId,
        uint questId)
    {
        _objectManager = objectManager;
        _input = input;
        _questId = questId;
        _targetRunner = new NpcTargetTestRunner(
            objectManager,
            input,
            mmapsDirectory,
            mapId,
            npcEntryId,
            questId);
    }

    public async Task RunAsync(CancellationToken cancellationToken)
    {
        await DesktopQuestUi.ValidateDependenciesAsync(cancellationToken);

        var ui = new DesktopQuestUi(_input);
        try
        {
            ulong npcGuid = await _targetRunner.ApproachAndTargetAsync(cancellationToken);
            VerifyTarget(npcGuid);

            Console.WriteLine("Opening the verified questgiver with bounded right-clicks...");
            DesktopFrame frame = await OpenQuestDialogAsync(ui, npcGuid, cancellationToken);

            for (int action = 1; action <= MaximumUiActions; action++)
            {
                if (!IsQuestActive())
                {
                    PrintSuccess(action - 1);
                    return;
                }

                OcrMatch? match =
                    frame.FindPhrase("Complete Quest") ??
                    frame.FindPhrase("Continue") ??
                    frame.FindPhrase("Your Place in the World");

                if (match is null)
                {
                    throw new InvalidOperationException(
                        "The quest window was detected, but no safe quest action was recognized. " +
                        "The bot stopped without clicking an unknown UI element.");
                }

                Console.WriteLine(
                    $"UI action {action}/{MaximumUiActions}: {match.Phrase} " +
                    $"at ({match.CenterX}, {match.CenterY})");
                await ui.ClickAsync(match, cancellationToken);
                await Task.Delay(TimeSpan.FromMilliseconds(900), cancellationToken);

                if (!IsQuestActive())
                {
                    PrintSuccess(action);
                    return;
                }

                VerifyTarget(npcGuid);
                frame = await ui.CaptureAsync(cancellationToken);
            }

            throw new InvalidOperationException(
                $"Quest {_questId} is still active after {MaximumUiActions} verified UI actions. " +
                "The bounded runner stopped.");
        }
        finally
        {
            ui.Cleanup();
            await _input.ReleaseAllAsync(CancellationToken.None);
        }
    }

    private async Task<DesktopFrame> OpenQuestDialogAsync(
        DesktopQuestUi ui,
        ulong npcGuid,
        CancellationToken cancellationToken)
    {
        DesktopFrame initial = await ui.CaptureAsync(cancellationToken);
        if (ContainsQuestUi(initial))
            return initial;

        // Prefer a visible world-space Gornek label. Matches inside the upper-left
        // target frame are excluded. The bounded fallback grid covers the model
        // below the centre line at close interaction range.
        var interactionPoints = new List<(int X, int Y, string Source)>();
        int worldLeft = initial.Window.X + (int)(initial.Window.Width * 0.22f);
        int worldRight = initial.Window.X + (int)(initial.Window.Width * 0.78f);
        int worldTop = initial.Window.Y + (int)(initial.Window.Height * 0.15f);
        int worldBottom = initial.Window.Y + (int)(initial.Window.Height * 0.78f);
        int expectedX = initial.Window.X + initial.Window.Width / 2;
        int expectedY = initial.Window.Y + (int)(initial.Window.Height * 0.48f);

        OcrMatch[] worldNameMatches = initial.FindPhrases("Gornek")
            .Where(match =>
                match.CenterX >= worldLeft && match.CenterX <= worldRight &&
                match.CenterY >= worldTop && match.CenterY <= worldBottom)
            .OrderBy(match =>
                Math.Abs(match.CenterX - expectedX) +
                Math.Abs(match.CenterY - expectedY))
            .ToArray();
        foreach (OcrMatch match in worldNameMatches)
            interactionPoints.Add((match.CenterX, match.CenterY, "OCR:Gornek"));

        (float X, float Y)[] relativePoints =
        [
            (0.50f, 0.55f),
            (0.50f, 0.62f),
            (0.50f, 0.48f),
            (0.46f, 0.55f),
            (0.54f, 0.55f),
            (0.46f, 0.62f),
            (0.54f, 0.62f),
            (0.50f, 0.70f),
            (0.42f, 0.55f),
            (0.58f, 0.55f),
            (0.50f, 0.42f),
            (0.42f, 0.48f),
            (0.58f, 0.48f),
            (0.42f, 0.65f),
            (0.58f, 0.65f),
            (0.46f, 0.72f),
            (0.54f, 0.72f),
            (0.50f, 0.78f)
        ];
        foreach ((float relativeX, float relativeY) in relativePoints)
        {
            (int x, int y) = initial.ToDesktopPoint(relativeX, relativeY);
            if (!interactionPoints.Any(point =>
                    Math.Abs(point.X - x) < 8 && Math.Abs(point.Y - y) < 8))
            {
                interactionPoints.Add((x, y, "bounded-grid"));
            }
        }

        Console.WriteLine(
            $"World-space Gornek OCR matches: {worldNameMatches.Length}; " +
            $"bounded interaction candidates: {interactionPoints.Count}");

        for (int attempt = 0;
             attempt < Math.Min(MaximumWorldClickAttempts, interactionPoints.Count);
             attempt++)
        {
            if (!HasExpectedTarget(npcGuid))
            {
                Console.WriteLine("Target was lost after a missed world click; reacquiring Gornek...");
                await _targetRunner.ReacquireTargetAsync(npcGuid, cancellationToken);
            }

            (int x, int y, string source) = interactionPoints[attempt];
            Console.WriteLine(
                $"Interaction attempt {attempt + 1}/{MaximumWorldClickAttempts}: " +
                $"right-click ({x}, {y}), source={source}");
            await ui.ClickAsync(x, y, MouseButton.Right, cancellationToken);
            await Task.Delay(TimeSpan.FromMilliseconds(800), cancellationToken);

            if (!IsQuestActive())
                return await ui.CaptureAsync(cancellationToken);

            DesktopFrame captured = await ui.CaptureAsync(cancellationToken);
            if (ContainsQuestUi(captured))
            {
                Console.WriteLine("Quest dialog recognized.");
                return captured;
            }
        }

        throw new InvalidOperationException(
            "Could not recognize Gornek's quest dialog after bounded centre-screen " +
            "right-click attempts. No arbitrary quest button was clicked.");
    }

    private static bool ContainsQuestUi(DesktopFrame frame) =>
        frame.FindPhrase("Your Place in the World") is not null ||
        frame.FindPhrase("Complete Quest") is not null ||
        frame.FindPhrase("Continue") is not null;

    private void VerifyTarget(ulong expectedGuid)
    {
        ulong observed = _objectManager
            .CaptureUnits(VerificationRadius)
            .PlayerTargetGuid;
        if (observed != expectedGuid)
        {
            throw new InvalidOperationException(
                $"Target changed during quest interaction: expected 0x{expectedGuid:X16}, " +
                $"observed 0x{observed:X16}. The bot stopped.");
        }
    }

    private bool HasExpectedTarget(ulong expectedGuid) =>
        _objectManager.CaptureUnits(VerificationRadius).PlayerTargetGuid == expectedGuid;

    private bool IsQuestActive() =>
        _objectManager.CaptureQuestLog().Entries
            .Any(entry => entry.QuestId == _questId);

    private void PrintSuccess(int uiActions)
    {
        Console.WriteLine($"Quest {_questId} is no longer present in the quest log.");
        Console.WriteLine($"Verified UI actions: {uiActions}");
        Console.WriteLine("Result: QUEST TURN-IN CONFIRMED");
        Console.WriteLine(
            "The runner stopped immediately after confirmation and did not accept another quest.");
    }
}
