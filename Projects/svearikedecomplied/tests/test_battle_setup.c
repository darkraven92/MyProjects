#include "battle_setup.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"%d: %s\n",__LINE__,#x);return 1;}} while(0)
static int near(double a,double b) {return a-b<0.000001 && b-a<0.000001;}
int main(void) {
    SrBattleArmy s={2000,2000,2501,2,0},e={3000,1500,1000,1,0};
    SrBattleSetup b;double losses[4];
    CHECK(sr_battle_setup(&b,&s,&e));
    CHECK(b.swedes==6 && b.enemies==6 && !b.ready && !b.placed);
    CHECK(b.guns[0].count==2 && b.guns[0].per_icon==1250 && b.guns[0].reload==3);
    CHECK(b.guns[1].count==1 && b.guns[1].per_icon==1000 && b.guns[1].reload==4);
    for(int n=0;n<6;++n) {
        CHECK(b.units[n].troop.type==(n<3?SR_BATTLE_CAV:SR_BATTLE_INF));
        CHECK(b.units[n].troop.troops==666 && b.units[n].troop.level==2);
        CHECK(b.units[n].troop.attacked==1 && b.units[n].actions==1 && !b.units[n].placed);
        CHECK(b.units[n].x==65+n*35 && b.units[n].y==119);
    }
    CHECK(!sr_battle_setup_enemy(&b));
    CHECK(!sr_battle_deploy(&b,0,22) && b.units[0].square==22 && b.units[0].x==65 && !b.contents[22]);
    CHECK(sr_battle_deploy(&b,0,3) && b.placed==1 && b.contents[3]==SR_BATTLE_SWED);
    CHECK(!sr_battle_deploy(&b,1,3) && b.placed==1 && b.units[1].x==100 && !b.units[1].placed);
    CHECK(!sr_battle_deploy(&b,0,4) && b.units[0].square==3);
    CHECK(sr_battle_deploy(&b,1,4) && sr_battle_deploy(&b,2,5));
    CHECK(sr_battle_deploy(&b,3,1) && sr_battle_deploy(&b,4,2) && sr_battle_deploy(&b,5,6));
    CHECK(sr_battle_setup_enemy(&b) && b.ready && b.formation.profile==4 && b.formation.row==6);
    const int enemy_squares[]={46,47,45,39,42,36};
    for(int n=0;n<6;++n) {
        CHECK(b.units[n+6].square==enemy_squares[n] && b.units[n+6].placed);
        CHECK(b.contents[enemy_squares[n]]==SR_BATTLE_ENEMY);
        CHECK(b.units[n+6].troop.type==(n<2?SR_BATTLE_CAV:SR_BATTLE_INF));
        CHECK(b.units[n+6].troop.troops==750);
    }
    SrBattleSetup saved=b;
    CHECK(!sr_battle_setup_enemy(&b) && !memcmp(&b,&saved,sizeof b));
    sr_battle_losses(&b,losses);CHECK(losses[0]==2 && losses[1]==2 && !losses[2] && !losses[3]);
    b.units[0].troop.troops=-250;b.units[3].troop.troops=500;b.units[6].troop.troops=0;
    sr_battle_losses(&b,losses);CHECK(losses[0]==168 && losses[1]==668 && !losses[2] && losses[3]==750);

    /* Boundary totals and source integer division; no redistribution of remainders. */
    const int infantry[]={4000,4001,4999,5000,7000,7001};
    const int cavalry[]={2000,2000,2000,2000,3000,3000};
    const int groups[]={6,6,6,7,10,10},loss[]={0,1,3,0,0,1};
    for(int n=0;n<6;++n) {
        s=(SrBattleArmy){infantry[n],cavalry[n],0,1,0};
        CHECK(sr_battle_setup(&b,&s,&e) && b.swedes==groups[n]);
        sr_battle_losses(&b,losses);CHECK(losses[0]==loss[n] && !losses[1]);
        if(b.swedes>=7) CHECK(b.units[6].x==65 && b.units[6].y==185);
        if(b.swedes==10) CHECK(b.units[9].x==170 && b.units[9].y==185);
    }
    s=(SrBattleArmy){1000,0,999,3,0};e=(SrBattleArmy){1000,0,10001,3,SR_BATTLE_REAL_INF};
    CHECK(sr_battle_setup(&b,&s,&e) && b.swedes==6 && b.enemies==6);
    CHECK(!b.guns[0].count && !b.guns[0].per_icon && b.guns[1].count==5 && b.guns[1].per_icon==2000);
    CHECK(b.guns[1].reload==2);
    sr_battle_losses(&b,losses);CHECK(losses[0]==4 && near(losses[2],0));
    for(int n=0;n<6;++n) CHECK(b.units[n].troop.type==SR_BATTLE_INF && near(b.units[n+6].troop.troops,1000.0/6));
    e=(SrBattleArmy){4000.1,3000,0,1,3};
    CHECK(sr_battle_setup(&b,&s,&e) && b.enemies==7);
    CHECK(b.units[6].troop.type==SR_BATTLE_CAV && near(b.units[6].troop.troops,3000/3.0001));
    CHECK(b.units[9].troop.type==SR_BATTLE_INF && near(b.units[9].troop.troops,1000.025));
    sr_battle_losses(&b,losses);CHECK(near(losses[2],0) && near(losses[3],3000-9000/3.0001));
    e=(SrBattleArmy){7800,0,0,1,SR_BATTLE_REAL_INF};
    CHECK(sr_battle_setup(&b,&s,&e) && b.enemies==8 && b.units[6].troop.troops==975);
    CHECK(sr_battle_loss_types(&b)==1 && b.units[6].real);
    /* FLOAT icon count makes cavalry FLOAT even with INTEGER cavalry input.
       Exactly-zero survivors preserve that promotion; negative HP does not. */
    e=(SrBattleArmy){4000,3000,0,1,SR_BATTLE_REAL_INF};
    CHECK(sr_battle_setup(&b,&s,&e) && sr_battle_loss_types(&b)==3);
    for(int n=b.swedes;n<b.swedes+b.enemies;++n)
        if(b.units[n].troop.type==SR_BATTLE_CAV) b.units[n].troop.troops=0;
    CHECK(sr_battle_loss_types(&b)==3);
    for(int n=b.swedes;n<b.swedes+b.enemies;++n)
        if(b.units[n].troop.type==SR_BATTLE_CAV) b.units[n].troop.troops=-1;
    CHECK(sr_battle_loss_types(&b)==1);
    e=(SrBattleArmy){6502,0,0,1,SR_BATTLE_REAL_INF};
    CHECK(sr_battle_setup(&b,&s,&e) && b.enemies==7);
    sr_battle_losses(&b,losses);CHECK(losses[2]<0 && losses[2]>-0.000001 && sr_battle_loss_types(&b)==1);
    saved=b;e.real=0;e.infantry=e.cavalry=0;
    CHECK(!sr_battle_setup(&b,&s,&e) && !memcmp(&b,&saved,sizeof b));
    e.infantry=1000.5;CHECK(!sr_battle_setup(&b,&s,&e));
    puts("Army division, numeric types, original staging, deployment, enemy formation and survivor accounting passed.");
    return 0;
}
