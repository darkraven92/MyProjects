#pragma once

namespace Bot
{
    enum class VendorState
    {
        Idle,
        PreparingHubSelection,
        SelectingHub,
        ReturningHomeForSearch,
        SearchingVendor,
        NavigatingVendor,
        NavigatingVendorAnchor,
        DirectVendorApproach,
        WaitingForMerchant,
        Selling,
        Maintaining,
        ReturningToGrind,
        Done,
        Failed
    };
}
