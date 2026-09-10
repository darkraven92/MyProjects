using WowBot.CachyOS.Objects;

namespace WowBot.CachyOS.Navigation;

public static class AngleMath
{
    public const float Tau = MathF.PI * 2.0f;

    public static float NormalizePositive(float radians)
    {
        float normalized = radians % Tau;
        return normalized < 0.0f ? normalized + Tau : normalized;
    }

    public static float NormalizeSigned(float radians)
    {
        float normalized = NormalizePositive(radians);
        return normalized > MathF.PI ? normalized - Tau : normalized;
    }

    public static float HeadingTo(WorldPosition from, WorldPosition to) =>
        NormalizePositive(MathF.Atan2(to.Y - from.Y, to.X - from.X));
}
