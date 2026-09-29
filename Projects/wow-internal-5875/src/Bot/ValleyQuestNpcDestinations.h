#pragma once

#include <cstdint>

namespace Bot
{
    struct QuestNpcDestination
    {
        std::uint32_t entry = 0;
        std::uint32_t mapId = 1;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        const char* label = "";
    };

    class ValleyQuestNpcDestinations
    {
    public:
        static const QuestNpcDestination* Find(std::uint32_t entry)
        {
            /*
             * These are search seeds, not interaction waypoints.
             * Once an NPC enters WorldState, GenericQuestTurnInExecutor
             * immediately abandons the seed and uses the live unit XYZ/GUID.
             *
             * 3143/3145/3153/9796 are runtime-observed in this project.
             * 11378 is derived from the Vanilla Durotar WorldMapArea bounds
             * and Foreman Thazz'ril's classic ~45,69 map location; it only
             * needs to bring the player into ObjectManager visibility.
             */
            static constexpr QuestNpcDestination locations[] =
            {
                {3143, 1, -600.13f, -4186.19f, 41.09f, "Gornek search seed"},
                {3145, 1, -629.05f, -4228.06f, 38.15f, "Zureetha Fargaze search seed"},
                {3153, 1, -639.34f, -4230.19f, 38.13f, "Frang search seed"},
                {9796, 1, -561.63f, -4221.80f, 41.59f, "Galgar search seed"},
                {11378, 1, -623.92f, -4341.88f, 41.10f, "Foreman Thazz'ril search seed"}
            };

            for (const auto& location : locations)
            {
                if (location.entry == entry)
                    return &location;
            }

            return nullptr;
        }
    };
}
