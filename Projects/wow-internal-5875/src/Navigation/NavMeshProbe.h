#pragma once

#include "DetourNavigationProvider.h"

#include "../Debug/Logger.h"
#include "../Objects/PlayerSnapshot.h"

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace Navigation
{
    class NavMeshProbe
    {
    private:
        static constexpr std::uint32_t KalimdorMapId =
            1;

        static constexpr NavPoint ZureethaFargaze =
        {
            -629.052f,
            -4228.060f,
            38.2334f
        };

        static std::string Float(
            float value)
        {
            std::ostringstream stream;

            stream
                << std::fixed
                << std::setprecision(3)
                << value;

            return stream.str();
        }

        static std::string Hex64(
            std::uint64_t value)
        {
            std::ostringstream stream;

            stream
                << "0x"
                << std::hex
                << std::uppercase
                << std::setw(16)
                << std::setfill('0')
                << value;

            return stream.str();
        }

        static float Distance3D(
            const NavPoint& a,
            const NavPoint& b)
        {
            const float dx =
                b.x - a.x;

            const float dy =
                b.y - a.y;

            const float dz =
                b.z - a.z;

            return
                std::sqrt(
                    dx * dx +
                    dy * dy +
                    dz * dz
                );
        }

    public:
        static bool Run(
            const Objects::PlayerState& player)
        {
            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "NAVMESH PHASE 10A: READ-ONLY QUERY"
            );

            Debug::Logger::Info(
                "No movement command will be sent."
            );

            const std::string directory =
                DetourNavigationProvider::
                    ResolveMmapsDirectory();

            Debug::Logger::Info(
                "MMAP directory: " +
                directory
            );

            Debug::Logger::Info(
                "Map ID: " +
                std::to_string(
                    KalimdorMapId
                )
            );

            const NavPoint start
            {
                player.x,
                player.y,
                player.z
            };

            Debug::Logger::Info(
                "Start: (" +
                Float(start.x) +
                "," +
                Float(start.y) +
                "," +
                Float(start.z) +
                ")"
            );

            Debug::Logger::Info(
                "Destination: Zureetha Fargaze "
                "(-629.052,-4228.060,38.233)"
            );

            DetourNavigationProvider provider;

            std::string error;

            if (!provider.Initialize(
                    directory,
                    KalimdorMapId,
                    error
                ))
            {
                Debug::Logger::Info(
                    "NAVMESH 10A: BLOCKED"
                );

                Debug::Logger::Info(
                    "Reason: " +
                    error
                );

                Debug::Logger::Info(
                    "Set WOW_NAV_MMAPS if the mmap "
                    "directory is stored elsewhere."
                );

                Debug::Logger::Info(
                    "================================"
                );

                return false;
            }

            Debug::Logger::Info(
                "Loaded tiles: " +
                std::to_string(
                    provider.LoadedTiles()
                )
            );

            NavPathResult result{};

            if (!provider.FindPath(
                    start,
                    ZureethaFargaze,
                    result
                ))
            {
                Debug::Logger::Info(
                    "NAVMESH 10A: QUERY FAILED"
                );

                Debug::Logger::Info(
                    "Reason: " +
                    result.error
                );

                Debug::Logger::Info(
                    "Loaded tiles: " +
                    std::to_string(
                        result.loadedTiles
                    )
                );

                if (
                    result.startPoly != 0)
                {
                    Debug::Logger::Info(
                        "Nearest start poly: " +
                        Hex64(
                            result.startPoly
                        )
                    );
                }

                Debug::Logger::Info(
                    "================================"
                );

                return false;
            }

            Debug::Logger::Info(
                "Nearest start poly: " +
                Hex64(
                    result.startPoly
                )
            );

            Debug::Logger::Info(
                "Nearest end poly: " +
                Hex64(
                    result.endPoly
                )
            );

            Debug::Logger::Info(
                "Polygon path count: " +
                std::to_string(
                    result.polygonCount
                )
            );

            Debug::Logger::Info(
                std::string(
                    "Partial path: "
                ) +
                (
                    result.partial
                        ? "yes"
                        : "no"
                )
            );

            Debug::Logger::Info(
                "Straight path points: " +
                std::to_string(
                    result.points.size()
                )
            );

            float totalLength =
                0.0f;

            for (
                std::size_t index = 0;
                index < result.points.size();
                ++index)
            {
                const auto& point =
                    result.points[
                        index
                    ];

                if (index > 0)
                {
                    totalLength +=
                        Distance3D(
                            result.points[
                                index - 1
                            ],
                            point
                        );
                }

                Debug::Logger::Info(
                    "NAV point[" +
                    std::to_string(
                        index
                    ) +
                    "]: (" +
                    Float(point.x) +
                    "," +
                    Float(point.y) +
                    "," +
                    Float(point.z) +
                    ")"
                );
            }

            Debug::Logger::Info(
                "Path length: " +
                Float(
                    totalLength
                )
            );

            if (result.partial)
            {
                Debug::Logger::Info(
                    "NAVMESH 10A: PARTIAL"
                );

                Debug::Logger::Info(
                    "More adjacent .mmtile files are "
                    "required before movement is enabled."
                );
            }
            else
            {
                Debug::Logger::Info(
                    "NAVMESH 10A: PASS"
                );
            }

            Debug::Logger::Info(
                "================================"
            );

            return
                !result.partial;
        }
    };
}
