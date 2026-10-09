#pragma once

namespace Bot
{
    // Read-only, attempt-scoped diagnostics. Never used to authorize selection
    // or acceptance; the existing dialog result remains the control input.
    struct QuestDiscoveryDialogDiagnostics
    {
        static constexpr const char* ResultVariable = "WOW_INTERNAL_DISCOVERY_DIAGNOSTIC";
        static constexpr const char* Script = R"lua(
WOW_INTERNAL_DISCOVERY_DIAGNOSTIC='probe_incomplete';
local function clean(v)
    local s=tostring(v or '');
    s=string.gsub(s,'[%c]',' ');
    return string.sub(s,1,96);
end;
local function visible(f)
    if not f then return 'missing'; end;
    local ok,v=pcall(function() return f:IsVisible(); end);
    if not ok then return 'error:'..clean(v); end;
    if v then return 'yes'; end;
    return 'no';
end;
local function scalar(f)
    if not f then return 'unavailable'; end;
    local ok,v=pcall(f);
    if not ok then return 'error:'..clean(v); end;
    return clean(v);
end;
local function gossip(f)
    if not f then return 'unavailable'; end;
    local ok,a=pcall(function() return {f()}; end);
    if not ok then return 'error:'..clean(a); end;
    local n=table.getn(a); local s='values='..n..' titles=[';
    for i=1,math.min(n,16),2 do s=s..'{'..clean(a[i])..'}'; end;
    return s..']';
end;
local function greeting()
    if not GetNumAvailableQuests or not GetAvailableTitle then return 'unavailable'; end;
    local n=GetNumAvailableQuests(); local s='count='..n..' titles=[';
    for i=1,math.min(n,8) do s=s..'{'..clean(GetAvailableTitle(i))..'}'; end;
    return s..']';
end;
WOW_INTERNAL_DISCOVERY_DIAGNOSTIC=
    'readFinished='..tostring(WOW_INTERNAL_DISCOVERY_READ_FINISHED)..
    ' gossipOpen='..visible(GossipFrame)..
    ' questFrameOpen='..visible(QuestFrame)..
    ' detailOpen='..visible(QuestFrameDetailPanel)..
    ' greetingOpen='..visible(QuestFrameGreetingPanel)..
    ' title={'..scalar(GetTitleText)..'}'..
    ' greetingAvailable={'..scalar(greeting)..'}'..
    ' greetingActive='..scalar(GetNumActiveQuests)..
    ' gossipAvailable={'..gossip(GetGossipAvailableQuests)..'}'..
    ' gossipActive={'..gossip(GetGossipActiveQuests)..'}';
)lua";
    };
}
