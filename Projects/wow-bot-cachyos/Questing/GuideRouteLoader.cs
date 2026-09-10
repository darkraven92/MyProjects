using System.Text.Json;
using System.Text.Json.Serialization;

namespace WowBot.CachyOS.Questing;

public static class GuideRouteLoader
{
    public static GuideRouteDefinition Load(string path)
    {
        if (!File.Exists(path))
            throw new FileNotFoundException("Quest route file was not found.", path);

        string json = File.ReadAllText(path);
        var options = new JsonSerializerOptions { PropertyNameCaseInsensitive = true };
        options.Converters.Add(new JsonStringEnumConverter());

        GuideRouteDefinition route = JsonSerializer.Deserialize<GuideRouteDefinition>(json, options)
            ?? throw new InvalidDataException("Could not deserialize the quest route.");
        route.Validate();
        return route;
    }
}
