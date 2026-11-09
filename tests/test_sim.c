#include "../src/game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){SatGame *g=malloc(sizeof *g),*b=malloc(sizeof *b);CHECK(g&&b);
 for(int f=0;f<2;f++){
  sat_new(g,f,212);CHECK(g->faction==f);CHECK(g->integrity==100);CHECK(g->event_count>=2);CHECK(g->radars[0].active);
  for(int i=0;i<500;i++)sat_tick(g,.25f);
  CHECK(g->time>100.f);int found=0;for(int i=0;i<SAT_TARGETS;i++)if(g->targets[i].quality>.1f)found++;CHECK(found>0);
  sat_cycle_track(g,1);CHECK(g->selected_track>=0);int t=g->selected_track;int ready=0;for(int i=0;i<SAT_FIGHTERS;i++)ready+=g->fighters[i].status==SAT_READY;
  sat_scramble(g,t);int ready2=0;for(int i=0;i<SAT_FIGHTERS;i++)ready2+=g->fighters[i].status==SAT_READY;CHECK(ready2==ready-1);
  sat_identify(g,t,SAT_HOSTILE);CHECK(g->targets[t].declared==SAT_HOSTILE);sat_authorize(g,t);CHECK(g->targets[t].authorized);
  int on=g->radars[0].active;sat_radar_toggle(g,0);CHECK(g->radars[0].active!=on);
  CHECK(sat_save(g,"/tmp/search-track-sim-test.sat2"));CHECK(sat_load(b,"/tmp/search-track-sim-test.sat2"));CHECK(!memcmp(g,b,sizeof *g));
  for(int i=0;i<2000;i++)sat_tick(g,.25f);
  CHECK(g->wave>=1);CHECK(g->integrity>=0);
 }
 CHECK(!sat_load(g,"/tmp/does-not-exist-sat2"));remove("/tmp/search-track-sim-test.sat2");free(g);free(b);puts("All simulation tests passed.");return 0;}
