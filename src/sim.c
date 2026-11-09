#include "game.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>

#define PI 3.14159265358979323846f
static float clampf(float v,float lo,float hi){return v<lo?lo:v>hi?hi:v;}
static uint32_t roll(SatGame *g){uint32_t x=g->rng;x^=x<<13;x^=x>>17;x^=x<<5;return g->rng=x?x:0x68a5f1bdU;}
static float randf(SatGame *g){return (roll(g)>>8)*(1.0f/16777216.0f);}
float sat_distance(float x,float y,float a,float b){return hypotf(x-a,y-b);}
float sat_heading(float vx,float vy){float a=atan2f(vx,-vy)*180.f/PI;return a<0?a+360:a;}
const char *sat_type_name(int t){static const char *types[]={"CIVILIAN","BOMBER","RECON","DECOY","ESCORT"};return t>=0&&t<5?types[t]:"UNKNOWN";}
void sat_event(SatGame *g,int level,const char *fmt,...){SatEvent *e=&g->events[g->next_event];va_list args;va_start(args,fmt);vsnprintf(e->text,sizeof e->text,fmt,args);va_end(args);e->at=g->time;e->level=level;g->next_event=(g->next_event+1)%SAT_EVENTS;if(g->event_count<SAT_EVENTS)g->event_count++;}
static void create_target(SatGame *g,int type,int near){
  SatTarget *t=NULL;for(int i=0;i<SAT_TARGETS;i++)if(!g->targets[i].used){t=&g->targets[i];break;}
  if(!t)return;
  memset(t,0,sizeof *t);t->used=t->alive=1;t->type=type;t->declared=SAT_UNKNOWN;
  t->serial=++g->track_counter;t->altitude=type==SAT_CIVIL?8900:type==SAT_RECON?18900:type==SAT_DECOY?7200:type==SAT_ESCORT?12500:11000;
  t->rcs=type==SAT_RECON?1.7f:type==SAT_DECOY?0.6f:type==SAT_CIVIL?12.f:type==SAT_ESCORT?3.5f:16.f;
  t->jammer=type==SAT_BOMBER?(g->wave>1?.32f:.12f):type==SAT_ESCORT?.38f:type==SAT_DECOY?.58f:0;
  t->transponder=type==SAT_CIVIL;
  t->x=g->faction==SAT_USA?(near?640:620):(near?360:380);
  t->y=90+randf(g)*540.f;
  float sp=type==SAT_RECON?0.31f:type==SAT_BOMBER?0.235f:type==SAT_ESCORT?0.28f:type==SAT_CIVIL?0.215f:0.26f;
  t->vx=(g->faction==SAT_USA?-sp:sp);t->vy=(randf(g)-.5f)*.060f;
  if(type==SAT_CIVIL){t->x=g->faction==SAT_USA?650:350;t->y=150+randf(g)*420;t->vy=(randf(g)-.5f)*.03f;}
  snprintf(t->callsign,sizeof t->callsign,"TRK-%03d",t->serial);
}
void sat_new(SatGame *g,int faction,unsigned seed){
 memset(g,0,sizeof *g);g->faction=faction==SAT_USSR?SAT_USSR:SAT_USA;g->rng=seed?seed:0x5a718a22U;
 g->time=0;g->paused=0;g->time_scale=4;g->integrity=100;g->score=0;g->selected_track=-1;g->selected_fighter=0;g->selected_radar=0;
 g->weather=.16f;g->noise=.07f;g->next_spawn=340.f;g->next_civilian=660.f;g->next_repair=140.f;
 const char *us_sites[]={"ECHO POINT","FORT ORION","BEACON NORTH","DOVER ARRAY","COAST WATCH"};
 const char *su_sites[]={"SEVER-1","MURMANSK-3","POLYARNY","RUBEZH-4","BELAYA"};
 float pts[5][2]={{300,150},{410,310},{340,565},{520,475},{220,385}};
 for(int i=0;i<SAT_RADARS;i++){SatRadar *r=&g->radars[i];snprintf(r->name,sizeof r->name,"%s",g->faction==SAT_USA?us_sites[i]:su_sites[i]);r->x=g->faction==SAT_USA?pts[i][0]:1000-pts[i][0];r->y=pts[i][1];r->range=(i==3?390:i==1?430:350);r->sweep=(float)((i*59)%360);r->scan=(i%2?15.f:12.f);r->gain=.78f;r->health=1.f;r->altitude=i==0?300.f:40.f;r->active=1;}
 const char *us_sam[]={"CAPE SENTINEL","HARBOR SHIELD","COASTAL DEFENSE"};
 const char *su_sam[]={"S-75 NORTH","S-125 COAST","S-200 ZONE"};
 float smp[3][2]={{330,220},{415,425},{235,545}};
 for(int i=0;i<SAT_SAMS;i++){SatSam *s=&g->sams[i];snprintf(s->name,sizeof s->name,"%s",g->faction==SAT_USA?us_sam[i]:su_sam[i]);s->x=g->faction==SAT_USA?smp[i][0]:1000-smp[i][0];s->y=smp[i][1];s->ammo=i==0?6:4;s->reload=0;}
 const char *us_air[]={"ALERT-01","ALERT-02","VIPER-03","VIPER-04","NOMAD-05","NOMAD-06","ARROW-07","ARROW-08"};
 const char *su_air[]={"SOKOL-01","SOKOL-02","BERKUT-03","BERKUT-04","VOLK-05","VOLK-06","GROM-07","GROM-08"};
 for(int i=0;i<SAT_FIGHTERS;i++){SatFighter *f=&g->fighters[i];snprintf(f->name,sizeof f->name,"%s",g->faction==SAT_USA?us_air[i]:su_air[i]);snprintf(f->model,sizeof f->model,"%s",g->faction==SAT_USA?(i<4?"F-106A":"F-4E"):(i<4?"Su-15TM":"MiG-21"));f->base=i%2;f->base_x=g->faction==SAT_USA?(i%2?185.f:380.f):(i%2?815.f:620.f);f->base_y=i%2?550.f:250.f;f->x=f->base_x;f->y=f->base_y;f->fuel=2200;f->speed=i<4?.45f:.40f;f->status=SAT_READY;f->target=-1;f->ammo=2;}
 sat_event(g,0,"%s / SECTOR COMMAND ON STATION",g->faction==SAT_USA?"NORAD":"PVO");
 sat_event(g,0,"Radar network synchronized. Five stations reporting.");
 sat_event(g,1,"AIR DEFENSE CONDITION 4. Maintain identification discipline.");
 create_target(g,SAT_RECON,1);create_target(g,SAT_CIVIL,1);
}
static int valid_track(const SatGame *g,int track){return track>=0&&track<SAT_TARGETS&&g->targets[track].used&&g->targets[track].alive&&g->targets[track].quality>.08f;}
void sat_cycle_track(SatGame *g,int direction){int start=g->selected_track;for(int i=1;i<=SAT_TARGETS;i++){int k=(start+direction*i+SAT_TARGETS*4)%SAT_TARGETS;if(valid_track(g,k)){g->selected_track=k;return;}}g->selected_track=-1;}
void sat_identify(SatGame *g,int track,int c){if(!valid_track(g,track)){sat_event(g,1,"Unable to classify: no valid radar track.");return;}SatTarget *t=&g->targets[track];if(t->declared==c)return;t->declared=c;g->identifications++;sat_event(g,c==SAT_HOSTILE?1:0,"%s MANUALLY CLASSIFIED %s",t->callsign,c==SAT_HOSTILE?"HOSTILE":"FRIENDLY");if(c==SAT_FRIENDLY)t->authorized=0;}
void sat_scramble(SatGame *g,int track){if(!valid_track(g,track)){sat_event(g,1,"SCRAMBLE DENIED: Select an active radar contact.");return;}SatTarget *t=&g->targets[track];int best=-1;float range=1e10f;for(int i=0;i<SAT_FIGHTERS;i++){SatFighter *f=&g->fighters[i];float d=sat_distance(f->x,f->y,t->estx,t->esty);if(f->status==SAT_READY&&d<range){range=d;best=i;}}
 if(best<0){sat_event(g,2,"NO READY INTERCEPTORS. All alert aircraft committed.");return;}
 SatFighter *f=&g->fighters[best];f->status=SAT_LAUNCHING;f->launch_timer=16.f+randf(g)*13.f;f->target=track;f->fuel=1800;g->selected_fighter=best;
 sat_event(g,1,"SCRAMBLE %s / %s assigned %s",f->name,f->model,t->callsign);
}
void sat_authorize(SatGame *g,int track){if(!valid_track(g,track))return;SatTarget *t=&g->targets[track];if(t->declared!=SAT_HOSTILE){sat_event(g,2,"ENGAGEMENT REFUSED: Declare target hostile first.");return;}t->authorized=!t->authorized;sat_event(g,1,"%s / WEAPONS %s",t->callsign,t->authorized?"FREE":"HOLD");}
static void kill_track(SatGame *g,SatTarget *t,const char *means){if(!t->alive)return;t->alive=0;t->quality=0;g->engagements++;if(t->type==SAT_CIVIL){g->score-=950;g->friendly_losses++;g->integrity-=12;sat_event(g,2,"CRITICAL: CIVILIAN AIRCRAFT DOWNED BY %s",means);}else{g->score+=t->type==SAT_BOMBER?260:180;g->intercepts++;sat_event(g,0,"TARGET DESTROYED: %s / %s",t->callsign,means);}}
void sat_sam_fire(SatGame *g,int track,int battery){if(!valid_track(g,track))return;SatTarget *t=&g->targets[track];if(t->declared!=SAT_HOSTILE||!t->authorized){sat_event(g,2,"LAUNCH DENIED: Hostile classification and weapons free required.");return;}int best=-1;float dmin=1e9;for(int i=0;i<SAT_SAMS;i++){if(battery>=0&&i!=battery)continue;SatSam *s=&g->sams[i];float d=sat_distance(s->x,s->y,t->x,t->y);if(d<220&&s->ammo>0&&d<dmin){best=i;dmin=d;}}if(best<0){sat_event(g,1,"SAM UNAVAILABLE: No armed battery within 220 km.");return;}
 SatSam *s=&g->sams[best];s->ammo--;float pk=clampf(.88f-dmin/700.f-t->jammer*.38f,.26f,.88f);sat_event(g,1,"%s / MISSILE LAUNCH ON %s",s->name,t->callsign);if(randf(g)<pk)kill_track(g,t,"SAM");else{g->score-=25;sat_event(g,1,"MISSILE MISSED. Target still airborne.");}}
void sat_radar_toggle(SatGame *g,int radar){if(radar<0||radar>=SAT_RADARS)return;SatRadar *r=&g->radars[radar];if(r->health<.2f){sat_event(g,2,"%s UNAVAILABLE: Equipment damaged.",r->name);return;}r->active=!r->active;sat_event(g,1,"%s %s",r->name,r->active?"TRANSMITTING":"STANDBY");}
static float angle_diff(float a,float b){float d=fabsf(a-b);return d>180?360-d:d;}
static void update_radars(SatGame *g,float dt){for(int j=0;j<SAT_RADARS;j++){SatRadar *r=&g->radars[j];if(!r->active||r->health<.2f)continue;float old=r->sweep;r->sweep=fmodf(r->sweep+r->scan*dt,360.f);if(r->sweep<old)r->scans++;for(int i=0;i<SAT_TARGETS;i++){SatTarget *t=&g->targets[i];if(!t->used||!t->alive)continue;float d=sat_distance(r->x,r->y,t->x,t->y);float horizon=4.12f*(sqrtf(r->altitude)+sqrtf(t->altitude));float range=r->range*powf(t->rcs/10.f,.095f)*r->health;if(d>range||d>horizon)continue;
 float az=sat_heading(t->x-r->x,t->y-r->y);if(angle_diff(r->sweep,az)>r->scan*dt+2.0f)continue;
 float p=clampf(.93f*expf(-powf(d/(range*.89f),4.f))*(1.f-t->jammer*.67f)*(1.f-g->noise*.30f)*r->gain,.04f,.98f);
 if(randf(g)<p){float err=(1.0f+r->range*.002f)*(1.f+t->jammer*3.3f);float nx=t->x+(randf(g)-.5f)*err;float ny=t->y+(randf(g)-.5f)*err;
 if(t->quality<.09f){t->estvx=t->vx;t->estvy=t->vy;t->estx=nx;t->esty=ny;}else{float age=clampf(g->time-t->seen_at,1.f,45.f);float mvx=(nx-t->estx)/age,mvy=(ny-t->esty)/age;t->estvx=clampf(.8f*t->estvx+.2f*mvx,-.9f,.9f);t->estvy=clampf(.8f*t->estvy+.2f*mvy,-.9f,.9f);t->estx=nx;t->esty=ny;}
 t->last_hit=g->time;t->seen_at=g->time;t->quality=clampf(t->quality+.27f,0,1);r->detections++;
 if(t->quality>=.28f&&t->last_update==0){sat_event(g,1,"NEW RADAR TRACK %s / BEARING %03.0f",t->callsign,az);t->last_update=1;}
 }}}
}
static void update_fighters(SatGame *g,float dt){for(int i=0;i<SAT_FIGHTERS;i++){SatFighter *f=&g->fighters[i];if(f->status==SAT_READY)continue;if(f->status==SAT_LAUNCHING){f->launch_timer-=dt;if(f->launch_timer<=0){f->status=SAT_INTERCEPT;sat_event(g,0,"%s AIRBORNE / VECTORING TO CONTACT",f->name);}continue;}
 if(f->status==SAT_TURNAROUND){f->launch_timer-=dt;if(f->launch_timer<=0){f->status=SAT_READY;f->fuel=2200;f->ammo=2;}continue;}
 f->fuel-=dt;if(f->fuel<280&&f->status!=SAT_RTB){f->status=SAT_RTB;sat_event(g,1,"%s BINGO FUEL / RETURNING TO BASE",f->name);}
 float destx=f->base_x,desty=f->base_y;
 if(f->status==SAT_INTERCEPT){if(f->target>=0&&f->target<SAT_TARGETS&&g->targets[f->target].alive){SatTarget *t=&g->targets[f->target];float predict=clampf(sat_distance(f->x,f->y,t->x,t->y)/(f->speed+.18f),0.f,300.f);destx=t->estx+t->estvx*predict;desty=t->esty+t->estvy*predict;
 float dist=sat_distance(f->x,f->y,t->x,t->y);if(dist<22.f){t->visually_identified=1;if(t->type==SAT_CIVIL||t->type==SAT_DECOY)g->score+=2;if(t->authorized&&t->declared==SAT_HOSTILE&&f->ammo>0){f->ammo--;float pk=clampf(.72f-t->jammer*.32f+.06f*randf(g),.28f,.85f);if(randf(g)<pk){kill_track(g,t,f->name);f->kills++;}else sat_event(g,1,"%s MISSILE MISSED / REATTACK",f->name);}}
 }else f->status=SAT_RTB;}
 float dx=destx-f->x,dy=desty-f->y,d=hypotf(dx,dy);if(d>.01f){float a=fminf(d,f->speed*dt);f->x+=dx/d*a;f->y+=dy/d*a;}
 if(f->status==SAT_RTB&&d<2.f){f->status=SAT_TURNAROUND;f->launch_timer=130.f+randf(g)*75.f;sat_event(g,0,"%s RECOVERED / GROUND CREW TURNAROUND",f->name);}
 if(f->fuel<=0){f->status=SAT_TURNAROUND;f->launch_timer=440;g->score-=60;g->integrity-=3;sat_event(g,2,"%s FORCED LANDING / FUEL EXHAUSTION",f->name);f->x=f->base_x;f->y=f->base_y;}
}}
static void update_targets(SatGame *g,float dt){for(int i=0;i<SAT_TARGETS;i++){SatTarget *t=&g->targets[i];if(!t->used||!t->alive)continue;
 t->x+=t->vx*dt;t->y+=t->vy*dt;t->estx+=t->estvx*dt;t->esty+=t->estvy*dt;t->quality=clampf(t->quality-dt*(g->time-t->last_hit>20.f?.006f:.00020f),0,1);
 float land=g->faction==SAT_USA?125.f:875.f;
 int reached=g->faction==SAT_USA?t->x<land:t->x>land;
 if(reached&&!t->attacked){t->attacked=1;t->alive=0;if(t->type==SAT_BOMBER){int dmg=7+(int)(randf(g)*8);g->integrity-=dmg;g->penetrations++;g->score-=180;sat_event(g,2,"BOMBER PENETRATION: Infrastructure damage %d%%",dmg);if(randf(g)<.38f){int r=(int)(randf(g)*SAT_RADARS);g->radars[r].health=clampf(g->radars[r].health-.55f,0,1);sat_event(g,2,"%s REPORTS EQUIPMENT DAMAGE",g->radars[r].name);}}
 else if(t->type==SAT_RECON){g->score-=50;sat_event(g,1,"RECONNAISSANCE AIRCRAFT LEFT DEFENDED SECTOR");}else if(t->type==SAT_CIVIL){g->score+=30;sat_event(g,0,"CIVIL FLIGHT TRANSITED SECTOR SAFELY");}else if(t->type==SAT_ESCORT){g->score-=40;}
 }
 if(t->x<-60||t->x>1060||t->y<-60||t->y>780){t->alive=0;t->escaped=1;}
}}
static void spawn_waves(SatGame *g){if(g->time>=g->next_spawn&&g->wave<5){g->wave++;int n=1+(g->wave%3);sat_event(g,2,"EARLY WARNING: FORMATION ACTIVITY / WAVE %d",g->wave);for(int i=0;i<n;i++)create_target(g,(g->wave%3==1&&i==0)?SAT_RECON:(i==n-1&&n>2)?SAT_ESCORT:SAT_BOMBER,0);g->next_spawn+=g->wave==1?460.f:520.f;}
 if(g->time>=g->next_civilian){create_target(g,SAT_CIVIL,0);g->next_civilian+=560.f;}
 if(g->time>=g->next_repair){for(int i=0;i<SAT_RADARS;i++){SatRadar *r=&g->radars[i];if(r->health<1){r->health=clampf(r->health+.045f,0,1);if(r->health>.2f&&!r->active){r->active=1;sat_event(g,0,"%s RETURNED TO SERVICE",r->name);}}}g->next_repair+=100.f;}
 if(g->time>=3050&&!g->complete){g->complete=1;g->victory=g->integrity>40&&g->friendly_losses==0;sat_event(g,g->victory?0:2,"SHIFT COMPLETE / FINAL EVALUATION AVAILABLE");}}
void sat_tick(SatGame *g,float dt){if(g->paused||g->complete)return;dt=clampf(dt,0,.5f);g->time+=dt;spawn_waves(g);update_targets(g,dt);update_radars(g,dt);update_fighters(g,dt);if(g->integrity<=0){g->integrity=0;g->complete=1;g->victory=0;sat_event(g,2,"SECTOR DEFENSE COLLAPSED / COMMAND RELIEVED");}if(g->selected_track>=0&&!valid_track(g,g->selected_track))sat_cycle_track(g,1);}
static uint32_t fnv(const unsigned char *s,size_t n){uint32_t h=2166136261u;for(size_t i=0;i<n;i++){h^=s[i];h*=16777619u;}return h;}
int sat_save(const SatGame *g,const char *path){if(!path)return 0;char tmp[1024];if(snprintf(tmp,sizeof tmp,"%s.tmp",path)>=(int)sizeof tmp)return 0;FILE *f=fopen(tmp,"wb");if(!f)return 0;uint32_t hdr[3]={0x32544153u,1,fnv((const unsigned char *)g,sizeof *g)};int ok=fwrite(hdr,sizeof hdr,1,f)==1&&fwrite(g,sizeof *g,1,f)==1&&fflush(f)==0;if(fclose(f))ok=0;if(!ok||rename(tmp,path)){remove(tmp);return 0;}return 1;}
int sat_load(SatGame *g,const char *path){if(!path)return 0;FILE *f=fopen(path,"rb");if(!f)return 0;uint32_t hdr[3];SatGame *tmp=malloc(sizeof *tmp);if(!tmp){fclose(f);return 0;}int ok=fread(hdr,sizeof hdr,1,f)==1&&hdr[0]==0x32544153u&&hdr[1]==1&&fread(tmp,sizeof *tmp,1,f)==1&&fgetc(f)==EOF;if(fclose(f))ok=0;if(ok&&fnv((const unsigned char *)tmp,sizeof *tmp)!=hdr[2])ok=0;if(ok&&(tmp->faction<0||tmp->faction>1||tmp->selected_radar<0||tmp->selected_radar>=SAT_RADARS||tmp->time<0||tmp->time>10000000.f||tmp->rng==0))ok=0;if(ok)*g=*tmp;free(tmp);return ok;}
