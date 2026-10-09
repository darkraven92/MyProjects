#include "battle_scene.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"%d: %s\n",__LINE__,#x);return 1;}} while(0)
static uint8_t pixels[640*480*4];
int main(int argc,char **argv) {
    CHECK(argc==2);SrRandom rng;
    /* Exercise every streamed cast, including switches and cache reuse. These
       are renderer fixtures, not claims of reaching late campaigns normally. */
    for(int sw=1;sw<=3;++sw) for(int country=1;country<=4;++country) for(int en=1;en<=3;++en) {
        SrBattleArmy a={5000,5000,10000,sw,0},b={5000,5000,10000,en,0};
        sr_random_seed(&rng,0);CHECK(sr_battle_scene_begin(&a,&b,country,&rng,1));
        CHECK(rng.calls==(country==4?3u:2u));
        for(int bank=0;bank<3;++bank) {
            int id=sr_battle_asset_request(bank);if(!id) continue;
            char path[1024];int n=snprintf(path,sizeof path,"%s/%d.pack",argv[1],id);
            CHECK(n>0 && (size_t)n<sizeof path);
            FILE *f=fopen(path,"rb");CHECK(f);CHECK(!fseek(f,0,SEEK_END));
            long size=ftell(f);rewind(f);CHECK(size>0 && (unsigned long)size<=sr_battle_asset_capacity());
            uint8_t *data=sr_battle_asset_input(bank);
            CHECK(fread(data,1,(size_t)size,f)==(size_t)size);CHECK(!fclose(f));
            CHECK(!sr_battle_asset_load(bank,id+1,(uint32_t)size));
            data[0]='?';CHECK(!sr_battle_asset_load(bank,id,(uint32_t)size));data[0]='S';
            CHECK(!sr_battle_asset_load(bank,id,(uint32_t)size-1));
            CHECK(sr_battle_asset_load(bank,id,(uint32_t)size));
        }
        CHECK(sr_battle_scene_value(1) && sr_battle_scene_value(2)==10);
        int background=sr_battle_scene_value(6);
        CHECK(sr_battle_scene_sound(0)==(background==1?154:background==5?156:155));
        CHECK(!sr_battle_scene_sound(0) && !sr_battle_scene_sound(1));
        memset(pixels,0,sizeof pixels);sr_battle_scene_draw(pixels);
        for(unsigned n=3;n<sizeof pixels;n+=4) CHECK(pixels[n]==255);
        CHECK(sr_battle_scene_unit(6,4)==65 && sr_battle_scene_unit(6,5)==185);
        if(country==1 && en==1) {
            sr_battle_scene_tick(1000);
            for(int n=0;n<10;++n) {
                int x,y;CHECK(sr_battle_point(n+1,&x,&y));
                sr_battle_scene_down(sr_battle_scene_unit(n,4),sr_battle_scene_unit(n,5)-15);
                CHECK(sr_battle_scene_value(5)==n);sr_battle_scene_up(x+28,y+114);
            }
            CHECK(sr_battle_scene_value(0)==9); /* Swedish officer precedes troop actions. */
            uint32_t now=1000;const int positions[5][2]={{34,360},{69,383},{106,404},{143,423},{177,446}};
            for(int gun=0;gun<5;++gun) {
                sr_battle_scene_up(600,440);CHECK(!sr_battle_scene_value(17));
                sr_battle_scene_draw(pixels);
                sr_battle_scene_tick(now+103);CHECK(sr_battle_scene_value(0)==9);
                sr_battle_scene_tick(now+104);CHECK(sr_battle_scene_value(0)==5 && sr_battle_scene_value(29)==gun);
                CHECK(sr_battle_scene_value(25)==positions[gun][0] && sr_battle_scene_value(26)==positions[gun][1]);
                sr_battle_scene_up(0,0);sr_battle_scene_up(600,440);CHECK(rng.calls==2u+(unsigned)gun*2);
                int target=10;while(target<20 && !sr_battle_scene_unit(target,3)) ++target;CHECK(target<20);
                int x=sr_battle_scene_unit(target,4),y=sr_battle_scene_unit(target,5),hp=sr_battle_scene_unit(target,1);
                sr_battle_scene_move(x,y);CHECK(sr_battle_scene_value(28)==sr_battle_scene_unit(target,2));
                sr_battle_scene_move(0,0);CHECK(sr_battle_scene_value(28)==sr_battle_scene_unit(target,2));
                sr_battle_scene_up(0,0);CHECK(rng.calls==2u+(unsigned)gun*2);
                sr_battle_scene_up(x,y);CHECK(sr_battle_scene_value(0)==8 && sr_battle_scene_value(22)==0);
                CHECK(sr_battle_scene_value(21)==gun && sr_battle_scene_value(27)==1 && !sr_battle_scene_value(12));
                CHECK(sr_battle_scene_sound(1)==(sw==3?177:172) && !sr_battle_scene_sound(1));
                sr_battle_scene_draw(pixels); /* Includes SWE3's original copy-ink ArtShoot loop. */
                sr_battle_scene_tick(now+127);CHECK(sr_battle_scene_unit(target,1)==hp);
                sr_battle_scene_tick(now+128);CHECK(sr_battle_scene_unit(target,1)<hp);
                CHECK(!sr_battle_scene_sound(1));
                sr_battle_scene_tick(now+176);CHECK(sr_battle_scene_value(0)==9);
                now+=176;
            }
            CHECK(sr_battle_scene_value(24)==5 && rng.calls==12);
            sr_battle_scene_tick(now+103);CHECK(sr_battle_scene_value(0)==9);
            sr_battle_scene_tick(now+104);CHECK(sr_battle_scene_value(0)==2 && !sr_battle_scene_value(24));
            CHECK(sr_battle_scene_value(25)==-1000 && sr_battle_scene_value(14)==10 && sr_battle_scene_unit(0,6)==1);
            now+=104;sr_battle_scene_up(600,440);
            do {sr_battle_scene_tick(++now);} while(sr_battle_scene_value(0)!=2 && sr_battle_scene_value(0)!=7 && now<10000);
            CHECK(sr_battle_scene_value(0)==2 && sr_battle_scene_value(19)==2);
            CHECK(sr_battle_scene_value(27)==2 && sr_battle_scene_value(25)==-1000);
        }
    }
    /* Explicit fixture using the final cached SWE3/RUS3 banks. Five guns must
       independently fire, remove their targets, and retarget the live army. */
    SrBattleArmy small={600,0,0,3,0},guns={6000,0,5000,3,0};
    sr_random_seed(&rng,0);CHECK(sr_battle_scene_begin(&small,&guns,4,&rng,1));
    CHECK(sr_battle_scene_value(1));
    for(int n=0;n<6;++n) {
        int x,y;CHECK(sr_battle_point(n+1,&x,&y));
        sr_battle_scene_down(sr_battle_scene_unit(n,4),sr_battle_scene_unit(n,5)-15);
        CHECK(sr_battle_scene_value(5)==n);sr_battle_scene_up(x+28,y+114);
    }
    sr_battle_scene_tick(20000);CHECK(sr_battle_scene_value(0)==2);
    sr_battle_scene_up(600,440);sr_battle_scene_tick(20001);
    for(int n=0;n<5;++n) {
        CHECK(sr_battle_scene_value(0)==8 && sr_battle_scene_value(21)==n);
        CHECK(sr_battle_scene_value(16)==5-n && rng.calls==3u+(unsigned)(n+1)*2);
        sr_battle_scene_draw(pixels);
        sr_battle_scene_tick(20001+n*72+24);
        CHECK(sr_battle_scene_unit(5-n,1)<=0 && sr_battle_scene_unit(5-n,3));
        sr_battle_scene_tick(20001+(n+1)*72);
        CHECK(!sr_battle_scene_unit(5-n,3));
    }
    CHECK(sr_battle_scene_value(0)==3 && sr_battle_scene_value(14)==1);
    uint32_t tick=20361;
    while(sr_battle_scene_value(0)!=2 && sr_battle_scene_value(0)!=7 && tick<22000) sr_battle_scene_tick(++tick);
    CHECK(sr_battle_scene_value(0)==2 && sr_battle_scene_value(19)==2);
    CHECK(sr_battle_scene_value(14)==1 && sr_battle_scene_unit(0,6)==1 && sr_battle_scene_value(23)==1);
    /* Retreat is absent during deployment and blocked by officer/shot waits,
       but available on the original aiming frame before firing. */
    small=(SrBattleArmy){1000,0,1000,3,0};guns=(SrBattleArmy){6000,0,0,3,0};
    CHECK(sr_battle_scene_begin(&small,&guns,4,&rng,1));sr_battle_scene_tick(30000);
    double losses[4]={-1,-1,-1,-1};int real=-1,retreat=-1;
    CHECK(!sr_battle_scene_result(losses,&real,&retreat) && losses[0]==-1 && real==-1 && retreat==-1);
    sr_battle_scene_up(40,440);CHECK(sr_battle_scene_value(0)==1);
    for(int n=0;n<6;++n) {
        int x,y;CHECK(sr_battle_point(n+1,&x,&y));
        sr_battle_scene_down(sr_battle_scene_unit(n,4),sr_battle_scene_unit(n,5)-15);sr_battle_scene_up(x+28,y+114);
    }
    sr_battle_scene_up(40,440);CHECK(sr_battle_scene_value(0)==9);
    sr_battle_scene_tick(30104);CHECK(sr_battle_scene_value(0)==5);
    unsigned calls=rng.calls;sr_battle_scene_up(85,440);CHECK(sr_battle_scene_value(0)==5);
    sr_battle_scene_up(40,440);
    CHECK(sr_battle_scene_result(losses,&real,&retreat)==SR_BATTLE_ENEMY && retreat && !real);
    CHECK(losses[0]==4 && !losses[1] && !losses[2] && !losses[3] && rng.calls==calls);
    sr_battle_scene_tick(40000);CHECK(rng.calls==calls && sr_battle_scene_value(0)==7);
    /* Explicit artillery fixture: finish all six enemy groups across reload
       turns, then obtain a victory result from the actual scene controller. */
    small.artillery=50000;guns.infantry=600;
    sr_random_seed(&rng,0);CHECK(sr_battle_scene_begin(&small,&guns,4,&rng,1));
    sr_battle_scene_tick(40000);
    CHECK(!sr_battle_scene_result(losses,&real,&retreat)); /* New battle clears outcome. */
    for(int n=0;n<6;++n) {
        int x,y;CHECK(sr_battle_point(n+1,&x,&y));
        sr_battle_scene_down(sr_battle_scene_unit(n,4),sr_battle_scene_unit(n,5)-15);sr_battle_scene_up(x+28,y+114);
    }
    int shots=0;
    for(tick=40001;tick<50000 && sr_battle_scene_value(0)!=7;++tick) {
        sr_battle_scene_tick(tick);
        if(sr_battle_scene_value(0)==5) {
            int n=6;while(n<12 && !sr_battle_scene_unit(n,3)) ++n;CHECK(n<12);
            sr_battle_scene_up(sr_battle_scene_unit(n,4),sr_battle_scene_unit(n,5));
            CHECK(sr_battle_scene_value(0)==8);++shots;
            sr_battle_scene_up(40,440);CHECK(sr_battle_scene_value(0)==8); /* No mid-shot retreat. */
        } else if(sr_battle_scene_value(0)==2) sr_battle_scene_up(600,440);
    }
    CHECK(shots==6 && sr_battle_scene_result(losses,&real,&retreat)==SR_BATTLE_SWED && !retreat && !real);
    CHECK(losses[0]==4 && !losses[1] && losses[2]==600 && !losses[3]);
    CHECK(!sr_battle_asset_input(-1) && !sr_battle_asset_input(3));
    CHECK(!sr_battle_asset_load(3,1,16));
    puts("All sixteen battle banks, country/technology switches, malformed pack rejection and original stage coverage passed.");
    return 0;
}
