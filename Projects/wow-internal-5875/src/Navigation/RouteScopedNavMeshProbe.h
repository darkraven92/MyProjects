#pragma once

#include "DetourNavigationProvider.h"

#include "../Debug/Logger.h"
#include "../Objects/WorldState.h"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace Navigation
{
    class RouteScopedNavMeshProbe
    {
    private:
        static constexpr std::uint32_t KalimdorMapId = 1;

        static constexpr NavPoint YarrogBaneshadow =
        {
            -58.1846f,
            -4220.6400f,
            62.3418f
        };

        bool attempted_ = false;

        static std::string Float(float value)
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(3) << value;
            return stream.str();
        }

        static long long Milliseconds(
            const std::chrono::steady_clock::time_point& start,
            const std::chrono::steady_clock::time_point& end)
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                end - start
            ).count();
        }

        static void LogResult(
            const char* scope,
            int margin,
            long long initMs,
            long long queryMs,
            const DetourNavigationProvider& provider,
            const NavPathResult& result,
            bool queryReturned)
        {
            Debug::Logger::Info(
                std::string("NAVMESH 11B.8.3: ") + scope +
                " margin=" + std::to_string(margin) +
                " loadedTiles=" + std::to_string(provider.LoadedTiles()) +
                " initMs=" + std::to_string(initMs) +
                " queryMs=" + std::to_string(queryMs)
            );

            Debug::Logger::Info(
                "NAVMESH 11B.8.3: query returned=" +
                std::string(queryReturned ? "yes" : "no") +
                " success=" +
                std::string(result.success ? "yes" : "no") +
                " partial=" +
                std::string(result.partial ? "yes" : "no") +
                " polygons=" + std::to_string(result.polygonCount) +
                " straightPoints=" + std::to_string(result.points.size())
            );

            if (!result.error.empty())
            {
                Debug::Logger::Info(
                    "NAVMESH 11B.8.3: Reason: " + result.error
                );
            }
        }

        static bool TryRouteWindow(
            const std::string& directory,
            const NavPoint& start,
            int margin)
        {
            DetourNavigationProvider provider;
            std::string error;

            const auto initStart = std::chrono::steady_clock::now();
            const bool initialized = provider.InitializeForRoute(
                directory,
                KalimdorMapId,
                start,
                YarrogBaneshadow,
                margin,
                error
            );
            const auto initEnd = std::chrono::steady_clock::now();

            if (!initialized)
            {
                Debug::Logger::Info(
                    "NAVMESH 11B.8.3: route-window initialization FAILED margin=" +
                    std::to_string(margin) +
                    " initMs=" +
                    std::to_string(Milliseconds(initStart, initEnd))
                );
                Debug::Logger::Info(
                    "NAVMESH 11B.8.3: Reason: " + error
                );
                return false;
            }

            NavPathResult result{};
            const auto queryStart = std::chrono::steady_clock::now();
            const bool queryReturned = provider.FindPath(
                start,
                YarrogBaneshadow,
                result
            );
            const auto queryEnd = std::chrono::steady_clock::now();

            LogResult(
                "route-window",
                margin,
                Milliseconds(initStart, initEnd),
                Milliseconds(queryStart, queryEnd),
                provider,
                result,
                queryReturned
            );

            return
                queryReturned &&
                result.success &&
                !result.partial;
        }

        static bool TryFullMap(
            const std::string& directory,
            const NavPoint& start)
        {
            DetourNavigationProvider provider;
            std::string error;

            const auto initStart = std::chrono::steady_clock::now();
            const bool initialized = provider.Initialize(
                directory,
                KalimdorMapId,
                error
            );
            const auto initEnd = std::chrono::steady_clock::now();

            if (!initialized)
            {
                Debug::Logger::Info(
                    "NAVMESH 11B.8.3: full-map initialization FAILED initMs=" +
                    std::to_string(Milliseconds(initStart, initEnd))
                );
                Debug::Logger::Info(
                    "NAVMESH 11B.8.3: Reason: " + error
                );
                return false;
            }

            NavPathResult result{};
            const auto queryStart = std::chrono::steady_clock::now();
            const bool queryReturned = provider.FindPath(
                start,
                YarrogBaneshadow,
                result
            );
            const auto queryEnd = std::chrono::steady_clock::now();

            LogResult(
                "full-map-fallback",
                -1,
                Milliseconds(initStart, initEnd),
                Milliseconds(queryStart, queryEnd),
                provider,
                result,
                queryReturned
            );

            return
                queryReturned &&
                result.success &&
                !result.partial;
        }

    public:
        void RunOnce(const Objects::WorldState& world)
        {
            if (attempted_)
            {
                return;
            }

            attempted_ = true;

            const NavPoint start
            {
                world.player.x,
                world.player.y,
                world.player.z
            };

            Debug::Logger::Info("================================");
            Debug::Logger::Info("NAVMESH PHASE 11B.8.3: READ-ONLY ROUTE PROBE");
            Debug::Logger::Info("No CTM or movement command will be sent.");
            Debug::Logger::Info(
                "Start: (" + Float(start.x) + "," + Float(start.y) + "," + Float(start.z) + ")"
            );
            Debug::Logger::Info(
                "Destination: Yarrog Baneshadow (-58.185,-4220.640,62.342)"
            );

            const std::string directory =
                DetourNavigationProvider::ResolveMmapsDirectory();

            if (TryRouteWindow(directory, start, 2))
            {
                Debug::Logger::Info("NAVMESH 11B.8.3: PASS margin=2");
                Debug::Logger::Info("================================");
                return;
            }

            Debug::Logger::Info(
                "NAVMESH 11B.8.3: expanding route tile window margin 2 -> 4."
            );

            if (TryRouteWindow(directory, start, 4))
            {
                Debug::Logger::Info("NAVMESH 11B.8.3: PASS margin=4");
                Debug::Logger::Info("================================");
                return;
            }

            Debug::Logger::Info(
                "NAVMESH 11B.8.3: route windows failed; running full-map diagnostic fallback."
            );

            if (TryFullMap(directory, start))
            {
                Debug::Logger::Info(
                    "NAVMESH 11B.8.3: PASS only with full-map fallback"
                );
            }
            else
            {
                Debug::Logger::Info("NAVMESH 11B.8.3: FAILED");
            }

            Debug::Logger::Info("================================");
        }
    };
}
