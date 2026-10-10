#include "../src/Bot/ConnectionObservationPolicy.h"

#include <cassert>
#include <string>

static void Has(const std::string& text, const char* field)
{
    assert(text.find(field) != std::string::npos);
}

int main()
{
    using namespace Bot;
    assert(ParseConnectionMode(nullptr) == ConnectionMode::Normal);
    assert(ParseConnectionMode("observe") == ConnectionMode::Observe);
    for (const char* value : {"", "Observe", "observe ", "reconnect", "normal"})
        assert(ParseConnectionMode(value) == ConnectionMode::Blocked);

    ConnectionEvidence5875 connected{true, true, true, "charselect", "read_only_api_predicate"};
    ConnectionWorldObservation world{true, "complete", 123, 456, 789};
    ConnectionObservationTracker tracker;
    std::string fields;
    assert(tracker.Observe(connected, world, fields));
    Has(fields, "worldSnapshot=valid");
    Has(fields, "serverConnected=yes");
    Has(fields, "lastGlueScreen=charselect glueScreenSemantics=historical");
    Has(fields, "glueVisibility=unknown");
    Has(fields, "dialogState=unknown");
    Has(fields, "dialogVisible=unknown dialogType=unknown");
    Has(fields, "liveGlueReason=source_gap_identity_and_lifetime");
    Has(fields, "loading=unknown");
    Has(fields, "currentGlueScreen=unknown pendingGlueScreen=unknown glueGeneration=unknown");
    Has(fields, "disconnectConfirmed=unknown actionEligibility=unknown");
    Has(fields, "inputOwner=none commands=none");
    for (int i = 0; i < 10000; ++i)
        assert(!tracker.Observe(connected, world, fields));

    // Normal logout/loading: losing OM never changes the independent server
    // predicate or turns historical character select into visible UI proof.
    const auto priorWorld = world;
    world = {false, "manager_missing"};
    assert(tracker.Observe(connected, world, fields));
    Has(fields, "worldSnapshot=unavailable worldStage=manager_missing");
    Has(fields, "manager=0x0 playerGuid=unknown localPlayer=unknown");
    Has(fields, "serverConnected=yes");
    Has(fields, "lastGlueScreen=charselect");
    Has(fields, "disconnectConfirmed=unknown");
    assert(!tracker.Observe(connected, world, fields));

    auto disconnectedPredicate = connected;
    disconnectedPredicate.serverConnected = false;
    assert(tracker.Observe(disconnectedPredicate, world, fields));
    Has(fields, "serverConnected=no");
    Has(fields, "lastGlueScreen=charselect");
    Has(fields, "dialogState=unknown");
    Has(fields, "disconnectConfirmed=unknown");
    connected.lastGlueScreen = "login";
    assert(tracker.Observe(connected, world, fields));
    Has(fields, "serverConnected=yes lastGlueScreen=login");

    // Current failed reads replace successful evidence immediately. Neither
    // absent connection reads nor OM loss supply a false disconnected value.
    auto unreadable = connected;
    unreadable.serverConnectionKnown = false;
    unreadable.reason = "read_unavailable";
    assert(tracker.Observe(unreadable, world, fields));
    Has(fields, "serverConnected=unknown lastGlueScreen=login");
    Has(fields, "disconnectConfirmed=unknown");
    assert(!tracker.Observe(unreadable, world, fields));
    assert(tracker.Observe({}, world, fields));
    Has(fields, "sourceVerified=no serverConnected=unknown lastGlueScreen=unknown");
    assert(!tracker.Observe({}, world, fields));
    // Even inconsistent synthetic evidence cannot bypass signature gating.
    auto badSignature = connected;
    badSignature.signaturesKnown = false;
    Has(ConnectionObservationTracker::Fields(badSignature, world),
        "sourceVerified=no serverConnected=unknown lastGlueScreen=unknown");

    assert(tracker.Observe(connected, priorWorld, fields));
    assert(!tracker.Observe(connected, priorWorld, fields));
    world = priorWorld;
    ++world.manager; // same character, a fresh world manager
    assert(tracker.Observe(connected, world, fields));
    ++world.playerGuid; // identity change is observable, never auto-resumed
    assert(tracker.Observe(connected, world, fields));
    ++world.localPlayer;
    assert(tracker.Observe(connected, world, fields));

    // R0.1b.2 research did not qualify a live frame or lifetime reader.
    // Exhaust existing evidence combinations: none may manufacture Glue,
    // loading or disconnect-dialog evidence. In particular, active_guid_missing
    // was observed DURING loading, but is not itself a loading classifier.
    for (bool valid : {false, true})
    for (bool signatures : {false, true})
    for (bool serverKnown : {false, true})
    for (bool serverConnected : {false, true})
    for (const char* screen : {"unknown", "login", "charselect", "movie"})
    for (const char* stage : {"complete", "manager_missing", "active_guid_missing",
                             "manager_root_unreadable", "player_snapshot_unreadable"})
    {
        ConnectionEvidence5875 sample{signatures, serverKnown, serverConnected, screen,
            signatures ? "read_only_api_predicate" : "signature_mismatch"};
        const ConnectionWorldObservation snapshot{valid, stage, 123, 456, 789};
        ConnectionObservationTracker sparse;
        assert(sparse.Observe(sample, snapshot, fields));
        for (const char* field : {"glueVisibility", "dialogState", "dialogVisible", "dialogType",
                                 "loading", "currentGlueScreen", "pendingGlueScreen",
                                 "glueGeneration", "disconnectConfirmed", "actionEligibility"})
            Has(fields, (std::string(field) + "=unknown").c_str());
        Has(fields, "inputOwner=none commands=none");
        assert(!sparse.Observe(sample, snapshot, fields));
        // A fresh unknown sample always loses any prior qualified predicate.
        sparse.Observe({}, snapshot, fields);
        Has(fields, "sourceVerified=no serverConnected=unknown lastGlueScreen=unknown");
        Has(fields, "dialogVisible=unknown dialogType=unknown");
        assert(!sparse.Observe({}, snapshot, fields));
    }

    LocationCandidates5875 raw{true,123,1,14,363};
    ConnectionObservationTracker lifecycle;
    for (const auto snapshot : {priorWorld, ConnectionWorldObservation{false,"manager_missing"},
         ConnectionWorldObservation{false,"active_guid_missing",124,0,0},
         ConnectionWorldObservation{false,"local_player_missing",124,456,0},
         ConnectionWorldObservation{false,"player_snapshot_unreadable",124,456,790}, priorWorld})
    {
        assert(lifecycle.Observe(connected, snapshot, fields, raw));
        Has(fields, "mapCandidate=1 zoneCandidate=14 areaCandidate=363");
        Has(fields, "locationQualification=unqualified locationReason=source_lifetime_gap");
        Has(fields, "inputOwner=none commands=none");
        Has(fields, "disconnectConfirmed=unknown");
        Has(fields, "serverConnected=yes");
        Has(fields, "glueScreenSemantics=historical");
        for (const char* forbidden : {"currentMap=", "currentZone=", "currentArea=", "runtimeQualified=yes"})
            assert(fields.find(forbidden) == std::string::npos);
        for (int i = 0; i < 10000; ++i)
            assert(!lifecycle.Observe(connected, snapshot, fields, raw));
    }
    Has(ConnectionObservationTracker::Fields(connected,{false,"active_guid_missing",124,0,0},raw),
        "manager=0x7c playerGuid=0x0 localPlayer=unknown");
    Has(ConnectionObservationTracker::Fields(connected,{false,"local_player_missing",124,456,0},raw),
        "manager=0x7c playerGuid=0x1c8 localPlayer=unknown");
    Has(ConnectionObservationTracker::Fields(connected,{false,"player_snapshot_unreadable",124,456,790},raw),
        "manager=0x7c playerGuid=0x1c8 localPlayer=0x316");
    for (const char* stage : {"manager_root_unreadable","unknown","future_stage"})
        Has(ConnectionObservationTracker::Fields(connected,{false,stage,124,456,790},raw),
            "manager=unknown playerGuid=unknown localPlayer=unknown");

    for (int field = 0; field < 3; ++field)
    {
        auto changed = raw;
        if (field == 0) changed.mapCandidate = 0xffffffffu;
        if (field == 1) changed.zoneCandidate = 0;
        if (field == 2) changed.areaCandidate = 0;
        assert(lifecycle.Observe(connected, priorWorld, fields, changed));
        Has(fields, "locationQualification=unqualified");
        if (field == 0) Has(fields, "mapCandidate=4294967295");
        if (field == 1) Has(fields, "zoneCandidate=0");
        if (field == 2) Has(fields, "areaCandidate=0");
        assert(!lifecycle.Observe(connected, priorWorld, fields, changed));
        assert(lifecycle.Observe(connected, priorWorld, fields, raw));
    }
    auto badRaw = raw;
    badRaw.signaturesKnown = false;
    assert(lifecycle.Observe(connected, priorWorld, fields, badRaw));
    Has(fields, "mapCandidate=unknown zoneCandidate=unknown areaCandidate=unknown");
    Has(fields, "locationQualification=unqualified");
    assert(!lifecycle.Observe(connected, priorWorld, fields, {}));
}
