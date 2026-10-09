-- Run with the production diagnostic script on stdin; no game client needed.
local probe = assert(loadstring(io.read('*a')));
local cases = 0;
local function frame(visible) return {IsVisible=function() return visible; end}; end;
local function check(needle)
    assert(string.find(WOW_INTERNAL_DISCOVERY_DIAGNOSTIC,needle,1,true),
        needle..' missing in '..WOW_INTERNAL_DISCOVERY_DIAGNOSTIC);
end;
local function run()
    WOW_INTERNAL_DISCOVERY_STATE='waiting';
    probe();
    assert(WOW_INTERNAL_DISCOVERY_STATE=='waiting'); -- no control result mutation
    cases=cases+1;
end;
-- Missing globals/APIs stay unknown, never become a fabricated offer.
run(); check('gossipOpen=missing'); check('gossipAvailable={unavailable}');
-- Hidden but present frames distinguish no recognized UI from absent globals.
QuestFrame=frame(false); QuestFrameDetailPanel=frame(false);
QuestFrameGreetingPanel=frame(false); GossipFrame=frame(false);
GetTitleText=function() return ''; end;
GetNumAvailableQuests=function() return 0; end;
GetAvailableTitle=function() error('no title expected'); end;
GetNumActiveQuests=function() return 0; end;
GetGossipAvailableQuests=function() end;
GetGossipActiveQuests=function() end;
WOW_INTERNAL_DISCOVERY_READ_FINISHED=true;
run(); check('readFinished=true'); check('gossipOpen=no'); check('count=0');
-- Direct quest detail, without GossipFrame, remains observable.
QuestFrame=frame(true); QuestFrameDetailPanel=frame(true);
GetTitleText=function() return 'Fixture Quest'; end;
run(); check('detailOpen=yes'); check('title={Fixture Quest}'); check('gossipOpen=no');
-- Greeting and single/multiple available gossip quests use the existing tuple layout.
QuestFrameDetailPanel=frame(false); QuestFrameGreetingPanel=frame(true);
GetNumAvailableQuests=function() return 2; end;
GetAvailableTitle=function(i) return ({'First','Second'})[i]; end;
run(); check('greetingOpen=yes'); check('count=2 titles=[{First}{Second}]');
GossipFrame=frame(true);
GetGossipAvailableQuests=function() return 'First',1; end;
run(); check('values=2 titles=[{First}]');
GetGossipAvailableQuests=function() return 'First',1,'Second',2; end;
GetGossipActiveQuests=function() return 'Active',1; end;
run(); check('values=4 titles=[{First}{Second}]'); check('gossipActive={values=2 titles=[{Active}]}');
-- An interrupted original read and failing diagnostic getters are explicit.
WOW_INTERNAL_DISCOVERY_READ_FINISHED=false;
GetTitleText=function() error('fixture title error'); end;
run(); check('readFinished=false'); check('title={error:');
GetGossipAvailableQuests=function() error('fixture gossip error'); end;
run(); check('gossipAvailable={error:');
-- Diagnostic read is bounded even if the greeting API reports a huge list.
local reads=0;
GetNumAvailableQuests=function() return 10000; end;
GetAvailableTitle=function() reads=reads+1; return 'Fixture'; end;
run(); assert(reads==8); check('count=10000');
print('Quest dialog Lua fixtures: '..cases..'/ '..cases..' PASS');
