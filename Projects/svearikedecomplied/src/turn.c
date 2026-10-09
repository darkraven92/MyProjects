#include "turn.h"
#include "economy.h"
#include "family.h"
enum { YEAR_DELAY, EVENTS_A, EVENTS_B, PERIOD_EVENT, RELATIONS, WARS,
    ENEMY_TROOPS, RIOTS, LOSE_AREAS, TRADE, TRADE_NOTICES, PRICES, INCOME,
    SUPPLY, GROWTH, COMPLETE };
static const SrTurnRequest *request(SrTurn *t,int type,int area,int country,SrEvent event) {
    t->pending=(SrTurnRequest){type,area,country,0,event};
    return &t->pending;
}
int sr_turn_begin(SrGame *g,SrTurn *t) {
    if(!g || !t || t->active || g->family<1 || g->family>5 || g->turn<1 || g->turn>=60 || g->crushed) return 0;
    *t=(SrTurn){0}; t->active=1;
    t->event_b_slot=sr_random_next(&g->random,5);
    do { t->event_slot=sr_random_next(&g->random,5); } while(t->event_slot==t->event_b_slot);
    return 1;
}
static const SrTurnRequest *event_request(SrGame *g,SrTurn *t,SrEvent e) {
    if(!sr_event_apply(g,e)) return request(t,SR_TURN_FAULT,0,0,e);
    return request(t,SR_TURN_EVENT,0,0,e);
}
const SrTurnRequest *sr_turn_advance(SrGame *g,SrTurn *t) {
    static const SrTurnRequest none={0};
    const SrEvent no_event={0,0};
    if(!g || !t) return &none;
    if(t->pending.type || !t->active) return &t->pending;
    for(;;) switch(t->phase) {
    case YEAR_DELAY:
        if(t->year_index<5) return request(t,SR_TURN_YEAR,0,0,no_event);
        t->phase=RELATIONS; break;
    case EVENTS_A: {
        int count=sr_catalog_dated_count(SR_EVENTS_A);
        while(t->scan<=count) {
            int number=t->scan++;
            const SrDatedEvent *e=sr_catalog_dated(SR_EVENTS_A,number);
            if(e->base_year>g->year) break;
            if(e->base_year==g->year) return event_request(g,t,(SrEvent){SR_EVENT_A,number});
        }
        t->phase=EVENTS_B; break;
    }
    case EVENTS_B: {
        t->phase=PERIOD_EVENT;
        if(t->year_index==t->event_b_slot) {
            SrEvent e=sr_event_choose_b(g);
            if(e.record) return event_request(g,t,e);
        }
        break;
    }
    case PERIOD_EVENT: {
        t->phase=YEAR_DELAY;
        if(t->year_index==t->event_slot) {
            SrEvent e=sr_event_choose(g);
            if(e.record) return event_request(g,t,e);
        }
        break;
    }
    case RELATIONS:
        sr_economy_relations(g); ++g->turn;
        t->phase=WARS; t->scan=0; break;
    case WARS:
        while(t->scan<4) {
            int country=t->scan++;
            if(g->countries[country].relation>0 && sr_random_next(&g->random,10)<=g->countries[country].relation) {
                t->war_countries|=1u<<country;
                return request(t,SR_TURN_WAR,0,country+1,no_event);
            }
        }
        t->phase=ENEMY_TROOPS; break;
    case ENEMY_TROOPS:
        sr_economy_enemy_troops(g); t->scan=0; t->phase=RIOTS; break;
    case RIOTS:
        if(!g->no_riot) while(t->scan<g->player_areas.count) {
            int area=g->player_areas.records[t->scan++];
            int riot=sr_game_unrest(g,area,0),roll=sr_random_next(&g->random,100);
            if(g->areas[area].tax_level>1) {
                if(roll<=riot) return request(t,SR_TURN_RIOT,area,0,no_event);
                if(roll<=riot+10 && riot>0) {
                    t->riots[area]=1;
                    return request(t,SR_TURN_UNREST,area,0,no_event);
                }
            }
        }
        g->no_riot=0; t->phase=LOSE_AREAS; break;
    case LOSE_AREAS: {
        int last=0;
        /* errorMes does not suspend the script. All removals happen and the
           final province message wins; retain it for the presentation layer. */
        for(int i=0;i<g->player_areas.count;) {
            int area=g->player_areas.records[i];
            if(sr_game_unrest(g,area,0)>100) {
                sr_list_remove(&g->player_areas,area); g->areas[area].owned=0; last=area;
            } else ++i;
        }
        t->phase=TRADE;
        if(!g->player_areas.count || g->crushed) {
            g->crushed=1; t->active=0;
            return request(t,SR_TURN_GAME_OVER,last,0,no_event);
        }
        if(last) return request(t,SR_TURN_LOST_AREA,last,0,no_event);
        break;
    }
    case TRADE:
        t->trade_losses=sr_economy_trade(g,t->war_countries); t->phase=TRADE_NOTICES; t->scan=0; break;
    case TRADE_NOTICES:
        while(t->scan<4) {
            int country=t->scan++;
            if(t->trade_losses&(1u<<country)) return request(t,SR_TURN_TRADE_LOST,0,country+1,no_event);
        }
        t->phase=PRICES; break;
    case PRICES:sr_economy_prices(g); t->phase=INCOME; break;
    case INCOME:sr_economy_income(g,t->riots); t->phase=SUPPLY; break;
    case SUPPLY:
        if(!sr_economy_supply(g)) return request(t,SR_TURN_STARVING,0,0,no_event);
        t->phase=GROWTH; break;
    case GROWTH:
        sr_economy_growth(g);
        for(int i=0;i<4;++i) g->player_relation_mod[i]=0;
        g->points=sr_game_points(g);
        if(!sr_family_update(g)) return request(t,SR_TURN_FAULT,0,0,no_event);
        t->phase=COMPLETE; break;
    case COMPLETE:
        t->active=0;
        return request(t,g->turn==60?SR_TURN_GAME_OVER:SR_TURN_FINISHED,0,0,no_event);
    default:return request(t,SR_TURN_FAULT,0,0,no_event);
    }
}
int sr_turn_respond(SrGame *g,SrTurn *t,int choice) {
    if(!g || !t || !t->active) return 0;
    switch(t->pending.type) {
    case SR_TURN_YEAR:
        ++g->year; ++t->year_index; t->phase=EVENTS_A; t->scan=1; break;
    case SR_TURN_EVENT:
        if(*sr_event_minigame(t->pending.event)) {
            t->pending.type=SR_TURN_MINIGAME; return 1;
        }
        break;
    case SR_TURN_RIOT: {
        int number=t->pending.area; SrAreaState *a=&g->areas[number];
        if(choice==SR_RIOT_STRIKE) {
            int troops=a->infantry+a->cavalry+a->artillery;
            if(!troops) { t->error=SR_RIOT_NO_TROOPS; return 0; }
            int cost=troops*g->troop_level/1000;
            if(cost>g->silver) { t->error=SR_RIOT_NO_SILVER; return 0; }
            g->silver-=cost; a->riot+=10;
        } else if(choice==SR_RIOT_TALK) a->riot+=5;
        else if(choice==SR_RIOT_CONCEDE) { a->riot-=20; a->tax_level=1; }
        else return 0;
        t->riots[number]=choice; t->error=0; break;
    }
    case SR_TURN_STARVING:
        /* Leave phase SUPPLY to re-check after dismissal; never repeat INCOME. */
        if(sr_economy_troop_cost(g)>g->crops) return 0;
        break;
    case SR_TURN_UNREST:case SR_TURN_LOST_AREA:case SR_TURN_TRADE_LOST:case SR_TURN_MINIGAME_RESULT:break;
    default:return 0;
    }
    t->pending=(SrTurnRequest){0}; return 1;
}
int sr_turn_minigame_finished(SrGame *g,SrTurn *t,int score) {
    if(!g || !t || !t->active || t->pending.type!=SR_TURN_MINIGAME) return 0;
    sr_event_minigame_result(g,t->pending.event,score);
    t->pending.type=SR_TURN_MINIGAME_RESULT; t->pending.score=score; return 1;
}
int sr_turn_war_finished(SrGame *g,SrTurn *t) {
    if(!g || !t || !t->active || t->pending.type!=SR_TURN_WAR) return 0;
    if(g->crushed) {
        /* Original battle resolution can transfer straight to the end scene. */
        t->active=0; t->pending.type=SR_TURN_GAME_OVER;
    } else t->pending=(SrTurnRequest){0};
    return 1;
}
