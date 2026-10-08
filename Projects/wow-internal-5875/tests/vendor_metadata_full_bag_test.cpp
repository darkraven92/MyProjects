#include "../src/Bot/AutoSellItemPolicy.h"
#include "../src/Bot/ManualVendorModePolicy.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

static std::string Read(const char* path)
{
    std::ifstream input(path);
    assert(input);
    return {std::istreambuf_iterator<char>(input), {}};
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--lua")
    {
        std::cout << "local tt=_G.tt; "
                  << Bot::AutoSellItemPolicy::LuaDefinition()
                  << "return saleReason;";
        return 0;
    }

    using Bot::ManualVendorModePolicy;
    assert(ManualVendorModePolicy::HoldAfterFailedTrip(true, 0));
    assert(ManualVendorModePolicy::HoldAfterFailedTrip(false, 0));
    assert(!ManualVendorModePolicy::HoldAfterFailedTrip(true, 1));

    const auto vendor = Read("src/Bot/VendorController.h");
    assert(vendor.find("metadata_pending|") != std::string::npos);
    assert(vendor.find("MetadataResolutionTimeoutTicks") != std::string::npos);
    assert(vendor.find("SellStepResult::MetadataPending") != std::string::npos);
    const auto grind = Read("src/Bot/GrindModeController.h");
    assert(grind.find("HoldAfterFailedTrip(") != std::string::npos);
    assert(grind.find("GRIND FULL BAG BLOCK state=entered") != std::string::npos);
    assert(grind.find("reason=vendor_trip_no_verified_space") !=
        std::string::npos);
    assert(grind.find("SetState(GrindModeState::WaitingForManualVendor)") !=
        std::string::npos);
}
