#define _POSIX_C_SOURCE 200809L
#include "game.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#define PI 3.14159265358979323846
#define MIN_W 1120
#define MIN_H 740
#define CL_BG 0
#define CL_SURFACE 1
#define CL_PANEL 2
#define CL_LINE 3
#define CL_GRID 4
#define CL_TEXT 5
#define CL_DIM 6
#define CL_GREEN 7
#define CL_GREEN2 8
#define CL_AMBER 9
#define CL_RED 10
#define CL_BLUE 11
#define CL_SEA 12
#define CL_LAND 13
#define CL_WHITE 14
#define CL_CYAN 15
#define CL_MUTED 16
#define CL_DARKRED 17
#define CL_MAX 18

typedef struct {
 Display *d;Window w;Pixmap back;GC gc;XFontStruct *fonts[3];unsigned long color[CL_MAX];
 int width,height,page,menu,menu_faction,zoom,show_help,blink,confirm_new,flash;
 float render_t;SatGame g;char savepath[1024],notice[160];float notice_until;
} App;
static App A;
static int min_i(int a,int b){return a<b?a:b;}
static int clamp_i(int a,int lo,int hi){return a<lo?lo:a>hi?hi:a;}
static int inside(int x,int y,int bx,int by,int bw,int bh){return x>=bx&&x<bx+bw&&y>=by&&y<by+bh;}
static void fg(int c){XSetForeground(A.d,A.gc,A.color[c]);}
static void box(int x,int y,int w,int h,int c){if(w<=0||h<=0)return;fg(c);XFillRectangle(A.d,A.back,A.gc,x,y,(unsigned)w,(unsigned)h);}
static void stroke(int x,int y,int w,int h,int c){if(w<=0||h<=0)return;fg(c);XDrawRectangle(A.d,A.back,A.gc,x,y,(unsigned)(w-1),(unsigned)(h-1));}
static void line(int x,int y,int x2,int y2,int c){fg(c);XDrawLine(A.d,A.back,A.gc,x,y,x2,y2);}
static void oval(int cx,int cy,int r,int c){if(r<1)return;fg(c);XDrawArc(A.d,A.back,A.gc,cx-r,cy-r,r*2,r*2,0,360*64);}
static void dot(int cx,int cy,int r,int c){if(r<1)return;fg(c);XFillArc(A.d,A.back,A.gc,cx-r,cy-r,r*2,r*2,0,360*64);}
static void poly(XPoint *pts,int n,int c){fg(c);XFillPolygon(A.d,A.back,A.gc,pts,n,Complex,CoordModeOrigin);}
static void txt(int x,int y,int font,int c,const char *str){fg(c);font=clamp_i(font,0,2);if(A.fonts[font])XSetFont(A.d,A.gc,A.fonts[font]->fid);XDrawString(A.d,A.back,A.gc,x,y,str,(int)strlen(str));}
static void label(int x,int y,int font,int c,const char *fmt,...){char str[400];va_list args;va_start(args,fmt);vsnprintf(str,sizeof str,fmt,args);va_end(args);txt(x,y,font,c,str);}
static void button(int x,int y,int w,int h,const char *title,int accent,int active){box(x,y,w,h,active?CL_PANEL:CL_SURFACE);stroke(x,y,w,h,active?accent:CL_LINE);box(x,y,3,h,accent);txt(x+10,y+h/2+5,0,active?CL_WHITE:CL_DIM,title);}
static void bar(int x,int y,int w,int h,float v,int col){box(x,y,w,h,CL_GRID);if(v>0){int q=(int)(w*fmaxf(0,fminf(1,v)));box(x,y,q,h,col);}}
static void rule(int x,int y,int w){line(x,y,x+w,y,CL_LINE);}
static void panel(int x,int y,int w,int h,const char *heading){box(x,y,w,h,CL_SURFACE);stroke(x,y,w,h,CL_LINE);if(heading){box(x,y,w,28,CL_PANEL);txt(x+12,y+19,0,CL_TEXT,heading);rule(x,y+28,w);}}
static int mx(void){return 209;}
static int my(void){return 83;}
static int mw(void){return A.width-209-319;}
static int mh(void){return A.height-83-207;}
static int rx(void){return A.width-304;}
static int rxw(void){return 292;}
static int foot_y(void){return A.height-197;}
static int world_x(float x){return mx()+(int)(x/SAT_WORLD_W*mw());}
static int world_y(float y){return my()+(int)(y/SAT_WORLD_H*mh());}
static int game_valid(int t){return t>=0&&t<SAT_TARGETS&&A.g.targets[t].used&&A.g.targets[t].alive&&A.g.targets[t].quality>.08f;}
static int target_col(const SatTarget *t){if(t->declared==SAT_HOSTILE)return CL_RED;if(t->declared==SAT_FRIENDLY)return CL_CYAN;if(t->transponder)return CL_BLUE;return t->quality>.45f?CL_AMBER:CL_GREEN;}
static void headline(const char *title,const char *sub){box(mx(),my(),mw(),31,CL_PANEL);txt(mx()+13,my()+21,0,CL_TEXT,title);txt(mx()+340,my()+21,0,CL_DIM,sub);}
static const char *navlabels[]={"TACTICAL PLOT","RADAR PPI","OPERATIONS","SENSOR NET","INTELLIGENCE","EVENT LOG","FIELD MANUAL"};
static void draw_frame(void){
 box(0,0,A.width,A.height,CL_BG);
 box(0,0,A.width,70,CL_SURFACE);box(0,0,6,70,CL_GREEN);rule(0,69,A.width);
 txt(19,32,2,CL_WHITE,"SEARCH / TRACK 2");txt(20,54,0,CL_DIM,"TACTICAL AIR DEFENSE SIMULATOR");
 int faction=A.g.faction;label(350,27,1,CL_GREEN,"%s  //  %s",faction==SAT_USA?"UNITED STATES / NORAD":"SOVIET UNION / PVO",faction==SAT_USA?"NORTH ATLANTIC SECTOR":"BARENTS DEFENSE DISTRICT");
 label(351,52,0,CL_DIM,"REGIONAL AIR DEFENSE OPERATIONS  |  EXERCISE SIMULATION");
 box(A.width-317,12,300,44,CL_PANEL);int clocksec=(8*3600+30*60+(int)A.g.time)%86400;label(A.width-303,31,1,CL_TEXT,"%02d:%02d:%02d  Z",clocksec/3600,(clocksec%3600)/60,clocksec%60);label(A.width-300,49,0,CL_GREEN,"SECTOR HEALTH %d%%    DEFCON  %d",A.g.integrity,A.g.integrity<45?2:4);
 box(0,70,194,A.height-70,CL_SURFACE);line(194,70,194,A.height,CL_LINE);
 txt(18,102,0,CL_DIM,"COMMAND STATIONS");
 for(int i=0;i<7;i++){int y=118+i*53;if(A.page==i){box(0,y,194,43,CL_PANEL);box(0,y,4,43,CL_GREEN);}label(18,y+18,0,A.page==i?CL_WHITE:CL_DIM,"0%d",i+1);txt(48,y+18,0,A.page==i?CL_GREEN:CL_TEXT,navlabels[i]);}
 rule(18,507,158);txt(18,534,0,CL_DIM,"SIMULATION");label(18,559,1,CL_WHITE,"%s",A.g.paused?"PAUSED":"LIVE OPERATIONS");
 label(18,583,0,CL_GREEN,"TIME COMPRESSION  %dx",A.g.time_scale);
 txt(18,606,0,CL_DIM,"SPACE  PAUSE");txt(18,625,0,CL_DIM,"[ / ]  SPEED");txt(18,644,0,CL_DIM,"TAB    NEXT TRACK");txt(18,663,0,CL_DIM,"S / L  SAVE / LOAD");
 rule(18,A.height-104,158);txt(18,A.height-80,0,CL_TEXT,"N    NEW OPERATION");txt(18,A.height-60,0,CL_DIM,"?    KEYBOARD HELP");txt(18,A.height-29,0,CL_GREEN,"LOCAL / OFFLINE / C17");
}
static void draw_map_grid(void){int x=mx(),y=my()+31,w=mw(),h=mh()-31;box(x,y,w,h,CL_SEA);
 for(int xx=0;xx<=1000;xx+=100){int sx=world_x((float)xx);line(sx,y,sx,y+h,CL_GRID);label(sx+4,y+13,0,CL_MUTED,"%03d",xx);}
 for(int yy=0;yy<=720;yy+=90){int sy=world_y((float)yy);if(sy<y)continue;line(x,sy,x+w,sy,CL_GRID);label(x+6,sy-4,0,CL_MUTED,"%03d",yy);}
 XPoint p[32];int n=0;float coast[16][2]={{0,0},{340,0},{365,65},{345,125},{390,176},{359,246},{386,322},{362,370},{422,456},{381,510},{405,590},{380,660},{425,720},{0,720}};
 if(A.g.faction==SAT_USA){for(int i=0;i<14;i++)p[n++]=(XPoint){(short)world_x(coast[i][0]),(short)world_y(coast[i][1])};}
 else{for(int i=0;i<14;i++)p[n++]=(XPoint){(short)world_x(1000-coast[i][0]),(short)world_y(coast[i][1])};}
 poly(p,n,CL_LAND);fg(CL_GRID);XDrawLines(A.d,A.back,A.gc,p,n,CoordModeOrigin);
 for(int i=0;i<10;i++){float land=A.g.faction==SAT_USA?75+i*26:925-i*26;label(world_x(land),world_y(65+i*59),0,CL_MUTED,"%c-%02d",A.g.faction==SAT_USA?'N':'P',i+1);}
 txt(x+14,y+h-18,0,CL_DIM,"SCHEMATIC THEATER GRID  /  NOT GEOGRAPHIC CARTOGRAPHY");
}
static void draw_radar_sites(int select){for(int i=0;i<SAT_RADARS;i++){SatRadar *r=&A.g.radars[i];int x=world_x(r->x),y=world_y(r->y);int range=(int)(r->range/SAT_WORLD_W*mw());if(r->active){oval(x,y,range,CL_GRID);int a=(int)(r->sweep*PI/180.0);(void)a;}
 line(x-7,y,x+7,y,r->active?CL_GREEN:CL_RED);line(x,y-7,x,y+7,r->active?CL_GREEN:CL_RED);if(select==i)oval(x,y,12,CL_AMBER);label(x+10,y-8,0,r->active?CL_DIM:CL_RED,"R%d",i+1);}
 for(int i=0;i<SAT_SAMS;i++){SatSam *s=&A.g.sams[i];int x=world_x(s->x),y=world_y(s->y);XPoint pp[4]={{x,y-7},{x+7,y},{x,y+7},{x-7,y}};poly(pp,4,CL_BLUE);label(x+10,y-5,0,CL_DIM,"SAM-%d",i+1);}
 float cities[2][2]={{220,320},{245,595}};for(int i=0;i<2;i++){int x=world_x(A.g.faction==SAT_USA?cities[i][0]:1000-cities[i][0]),y=world_y(cities[i][1]);box(x-3,y-3,7,7,CL_WHITE);label(x+10,y+5,0,CL_DIM,i==0?"CIVIL GRID":"COMMAND HQ");}
}
static void draw_fighters(void){for(int i=0;i<SAT_FIGHTERS;i++){SatFighter *f=&A.g.fighters[i];if(f->status==SAT_READY||f->status==SAT_TURNAROUND)continue;int x=world_x(f->x),y=world_y(f->y);XPoint p[3]={{(short)x,(short)(y-9)},{(short)(x-6),(short)(y+7)},{(short)(x+6),(short)(y+7)}};poly(p,3,CL_CYAN);label(x+10,y-4,0,CL_CYAN,"%s",f->name);if(f->target>=0&&game_valid(f->target)){SatTarget *t=&A.g.targets[f->target];line(x,y,world_x(t->estx),world_y(t->esty),CL_MUTED);}}
}
static void draw_targets_map(void){for(int i=0;i<SAT_TARGETS;i++){SatTarget *t=&A.g.targets[i];if(!game_valid(i))continue;int x=world_x(t->estx),y=world_y(t->esty),c=target_col(t);if(!inside(x,y,mx()+5,my()+34,mw()-10,mh()-39))continue;
 if(i==A.g.selected_track){oval(x,y,17,CL_WHITE);line(x-27,y,x-19,y,CL_WHITE);line(x+19,y,x+27,y,CL_WHITE);line(x,y-27,x,y-19,CL_WHITE);line(x,y+19,x,y+27,CL_WHITE);}
 int uncertainty=(int)((1.f-t->quality)*22);if(uncertainty>5)oval(x,y,uncertainty,CL_GRID);
 XPoint p[4]={{(short)x,(short)(y-7)},{(short)(x+7),(short)y},{(short)x,(short)(y+7)},{(short)(x-7),(short)y}};poly(p,4,c);label(x+10,y-10,0,c,"%s",t->callsign);line(x,y,x+(int)(t->estvx*85),y+(int)(t->estvy*85),c);
}}
static void draw_tactical(void){
 XRectangle clip={(short)mx(),(short)(my()+31),(unsigned short)mw(),(unsigned short)(mh()-31)};
 XSetClipRectangles(A.d,A.gc,0,0,&clip,1,Unsorted);
 draw_map_grid();draw_radar_sites(-1);draw_fighters();draw_targets_map();
 XSetClipMask(A.d,A.gc,None);
 headline("TACTICAL AIR PICTURE","MULTI-SENSOR / PLOT CONFIDENCE / INTERCEPT TRACKING");
}
static float radar_range(void){return A.zoom==0?160:A.zoom==1?300:520;}
static int scope_center_x(void){return mx()+mw()/2;}
static int scope_center_y(void){return my()+31+(mh()-31)/2;}
static int scope_radius(void){return min_i(mw()-62,mh()-100)/2;}
static void draw_ppi(void){int x=mx(),y=my()+31,w=mw(),h=mh()-31;box(x,y,w,h,CL_SEA);int cx=scope_center_x(),cy=scope_center_y(),rad=scope_radius();dot(cx,cy,rad,CL_LAND);
 for(int j=1;j<=4;j++){int r=rad*j/4;oval(cx,cy,r,CL_GRID);label(cx+6,cy-r+14,0,CL_MUTED,"%d",(int)(radar_range()*j/4));}
 line(cx-rad,cy,cx+rad,cy,CL_GRID);line(cx,cy-rad,cx,cy+rad,CL_GRID);for(int a=0;a<360;a+=30){float angle=a*(float)PI/180;int sx=cx+(int)(sinf(angle)*rad),sy=cy-(int)(cosf(angle)*rad);label(sx-9,sy+4,0,CL_DIM,"%03d",a);}
 SatRadar *r=&A.g.radars[A.g.selected_radar];if(r->active){for(int k=0;k<9;k++){float d=(r->sweep-(float)k*2.2f)*PI/180.f;int len=rad-(int)(k*1.5f);line(cx,cy,cx+(int)(cosf(d)*len),cy+(int)(sinf(d)*len),k==0?CL_GREEN:CL_GRID);}}
 dot(cx,cy,4,CL_GREEN);
 float scale=(float)rad/radar_range();for(int i=0;i<SAT_TARGETS;i++){SatTarget *t=&A.g.targets[i];if(!game_valid(i))continue;float dx=t->estx-r->x,dy=t->esty-r->y;float d=hypotf(dx,dy);if(d>radar_range())continue;int px=cx+(int)(dx*scale),py=cy+(int)(dy*scale),c=target_col(t);
 dot(px,py,i==A.g.selected_track?7:4,c);if(i==A.g.selected_track)oval(px,py,13,CL_WHITE);label(px+11,py-7,0,c,"%s",t->callsign);}
 for(int i=0;i<32;i++){float a=(float)(i*67+17)*.11f;int rr=rad*(i*37%99)/100;int nx=cx+(int)(cosf(a)*rr),ny=cy+(int)(sinf(a)*rr);dot(nx,ny,1,CL_MUTED);}
 headline("PLAN POSITION INDICATOR","PRIMARY SURVEILLANCE RADAR / SIMULATED DETECTIONS");
 label(x+17,y+h-22,0,CL_DIM,"STATION %d / %s / RANGE %d KM / GAIN %02.0f%%  |  Z TO CYCLE ZOOM",A.g.selected_radar+1,r->name,(int)radar_range(),r->gain*100);
}
static void draw_operations(void){int x=mx(),y=my()+31,w=mw(),h=mh()-31;box(x,y,w,h,CL_BG);headline("AIR OPERATIONS DESK","INTERCEPTOR FORCE / SCRAMBLE READINESS / FUEL STATUS");
 int cardw=(w-46)/3;for(int i=0;i<3;i++){int bx=x+12+i*(cardw+11);panel(bx,y+14,cardw,91,NULL);int ready=0,airborne=0;for(int j=0;j<SAT_FIGHTERS;j++)if(A.g.fighters[j].status==SAT_READY)ready++;else if(A.g.fighters[j].status==SAT_INTERCEPT)airborne++;label(bx+13,y+38,0,CL_DIM,"%s",i==0?"READY AIRFRAMES":i==1?"AIRBORNE INTERCEPTORS":"WEAPONS COMMITTED");label(bx+13,y+81,2,i==0?CL_GREEN:i==1?CL_CYAN:CL_AMBER,"%02d",i==0?ready:i==1?airborne:A.g.engagements);}
 int yy=y+122;box(x+13,yy,w-26,28,CL_PANEL);txt(x+24,yy+19,0,CL_TEXT,"CALLSIGN             AIRCRAFT          STATUS              FUEL       WEAPONS       ASSIGNMENT");
 for(int i=0;i<SAT_FIGHTERS;i++){SatFighter *f=&A.g.fighters[i];int ry=yy+32+i*39;box(x+13,ry,w-26,35,i==A.g.selected_fighter?CL_PANEL:(i%2?CL_SURFACE:CL_BG));if(i==A.g.selected_fighter)box(x+13,ry,3,35,CL_GREEN);const char *status[]={"READY","SCRAMBLING","INTERCEPT","RETURNING","SERVICING"};int sc=f->status==SAT_READY?CL_GREEN:f->status==SAT_INTERCEPT?CL_CYAN:f->status==SAT_LAUNCHING?CL_AMBER:CL_DIM;
 label(x+24,ry+22,0,CL_WHITE,"%-18s %-17s",f->name,f->model);label(x+298,ry+22,0,sc,"%-14s",status[f->status]);label(x+448,ry+22,0,CL_TEXT,"%3.0f%%",f->fuel/2200.f*100.f);bar(x+500,ry+12,62,8,f->fuel/2200.f,f->fuel<400?CL_RED:CL_GREEN);label(x+584,ry+22,0,CL_TEXT,"%d",f->ammo);label(x+661,ry+22,0,CL_AMBER,"%s",f->target>=0&&f->target<SAT_TARGETS?A.g.targets[f->target].callsign:"--");}
 label(x+18,y+h-23,0,CL_DIM,"SELECT A RADAR TRACK, PRESS F TO ASSIGN NEAREST READY INTERCEPTOR. AIRCRAFT AUTO-RETURN AT BINGO FUEL.");}
static void draw_network(void){int x=mx(),y=my()+31,w=mw(),h=mh()-31;box(x,y,w,h,CL_BG);headline("SENSOR & MISSILE NETWORK","RADAR AVAILABILITY / POWER CONTROL / STATION HEALTH");
 int roww=w-34;for(int i=0;i<SAT_RADARS;i++){SatRadar *r=&A.g.radars[i];int py=y+14+i*80;panel(x+15,py,roww,72,NULL);if(i==A.g.selected_radar){box(x+15,py,4,72,CL_GREEN);}
 label(x+28,py+26,1,CL_WHITE,"R%d / %s",i+1,r->name);label(x+28,py+49,0,r->active?CL_GREEN:CL_RED,"%s  |  RANGE %03.0f KM  |  SWEEP %02.0f DEG/S",r->active?"ACTIVE":"STANDBY",r->range,r->scan);
 label(x+roww-220,py+27,0,CL_DIM,"INTEGRITY");bar(x+roww-217,py+37,158,11,r->health,r->health<.4?CL_RED:CL_GREEN);label(x+roww-53,py+46,0,CL_TEXT,"%3.0f%%",r->health*100);}
 int yy=y+430;label(x+19,yy+12,1,CL_WHITE,"SURFACE TO AIR MISSILE BATTERIES");
 for(int i=0;i<SAT_SAMS;i++){SatSam *s=&A.g.sams[i];int bx=x+18+i*(w-36)/3;int bw=(w-55)/3;panel(bx,yy+27,bw,85,NULL);label(bx+11,yy+50,0,CL_CYAN,"SAM-%d / %s",i+1,s->name);label(bx+11,yy+79,1,CL_WHITE,"%d MISSILES READY",s->ammo);}
 label(x+19,y+h-14,0,CL_DIM,"CLICK A RADAR STATION TO SELECT IT. P TO TOGGLE TRANSMISSION. ENEMY TARGETS CAN MASK THEMSELVES.");}
static void draw_intel(void){int x=mx(),y=my()+31,w=mw(),h=mh()-31;box(x,y,w,h,CL_BG);headline("AIR INTELLIGENCE BOARD","INCOMPLETE INFORMATION / THREAT ESTIMATION / FLIGHT CORRELATION");
 panel(x+15,y+15,w-30,130,"THEATER THREAT ASSESSMENT");int threats=0,unknown=0,civ=0;for(int i=0;i<SAT_TARGETS;i++)if(game_valid(i)){SatTarget *t=&A.g.targets[i];if(t->declared==SAT_HOSTILE)threats++;else if(t->declared==SAT_UNKNOWN)unknown++;if(t->transponder)civ++;}
 label(x+27,y+70,2,CL_RED,"%02d",threats);label(x+28,y+94,0,CL_DIM,"CONFIRMED HOSTILE");label(x+260,y+70,2,CL_AMBER,"%02d",unknown);label(x+261,y+94,0,CL_DIM,"UNCLASSIFIED");label(x+506,y+70,2,CL_CYAN,"%02d",civ);label(x+506,y+94,0,CL_DIM,"TRANSPONDER SIGNALS");
 int iy=y+158;panel(x+15,iy,w-30,h-174,"ACTIVE TRACK DOSSIERS");int shown=0;for(int i=0;i<SAT_TARGETS&&shown<9;i++){SatTarget *t=&A.g.targets[i];if(!game_valid(i))continue;int yy=iy+51+shown*41;box(x+27,yy-18,w-55,34,(shown%2)?CL_BG:CL_PANEL);label(x+38,yy+5,0,target_col(t),"%-9s",t->callsign);label(x+159,yy+5,0,CL_TEXT,"ALT %5.0f m",t->altitude);label(x+311,yy+5,0,CL_TEXT,"SPD %4.0f km/h",hypotf(t->estvx,t->estvy)*3600.f);label(x+504,yy+5,0,CL_DIM,"QUALITY %02.0f%%",t->quality*100);label(x+w-195,yy+5,0,t->transponder?CL_CYAN:CL_AMBER,"%s",t->transponder?"IFF / CIVIL":"NO IFF");shown++;}if(!shown)txt(x+33,iy+74,0,CL_DIM,"No active confirmed radar tracks. Continue scanning.");
 label(x+27,y+h-15,0,CL_DIM,"TRANSPO​NDER INFORMATION IS AN INDICATOR, NOT DEFINITIVE PROOF OF IDENTITY.");}
static void draw_log(void){int x=mx(),y=my()+31,w=mw(),h=mh()-31;box(x,y,w,h,CL_BG);headline("OPERATIONAL EVENT LOG","IMMUTABLE IN-MEMORY TIMELINE / NEWEST FIRST");
 int count=min_i(A.g.event_count,(h-54)/27);for(int i=0;i<count;i++){int idx=(A.g.next_event-1-i+SAT_EVENTS)%SAT_EVENTS;SatEvent *e=&A.g.events[idx];int yy=y+16+i*27;box(x+13,yy,w-26,24,i%2?CL_BG:CL_SURFACE);label(x+26,yy+17,0,CL_DIM,"+%02d:%02d",(int)e->at/60,(int)e->at%60);box(x+104,yy+5,3,14,e->level==2?CL_RED:e->level==1?CL_AMBER:CL_GREEN);txt(x+121,yy+17,0,e->level==2?CL_RED:e->level==1?CL_TEXT:CL_GREEN,e->text);}
}
static void draw_manual(void){int x=mx(),y=my()+31,w=mw(),h=mh()-31;box(x,y,w,h,CL_BG);headline("COMMAND REFERENCE","SHORT OPERATING INSTRUCTIONS / TRAINING MODE");
 const char *lines[]={"MISSION: DEFEND THE SECTOR WITHOUT ENGAGING CIVIL AIRCRAFT.","","1-7       Open tactical, radar, operations, network, intelligence, log, help","TAB       Select next radar track. Click contacts on map or bottom list.","F          Scramble nearest ready interceptor toward selected radar track.","H          Classify selected contact HOSTILE (human command decision).","I           Classify selected contact FRIENDLY. Watch for civilian IFF.","E          Authorize / revoke weapons free for declared hostile track.","K          Launch SAM missile if an armed battery is in range.","P          Toggle selected radar transmitter on/off.","Z          Cycle PPI zoom. Choose station by clicking on Sensor Net.","[ / ]       Decrease / increase simulation time compression.","SPACE  Pause or resume. S save; L load; N choose a new operation.","","RADAR MODEL: Each site has a scan, max range, line-of-sight horizon,","gain, reliability, target cross-section and electronic interference.","Tracking estimates position/velocity from intermittent measurements.","Track quality decays without fresh radar hits. Contacts can vanish.","","INTERCEPTORS: Aircraft launch after readiness delay, fly lead pursuit,","and autonomously engage only after hostile ID and weapons release.","Missiles have uncertain outcomes. Aircraft return when low on fuel.","","SCORING: Defeat incursions, preserve infrastructure and civilian lives.","Misidentification carries severe consequences. Operational choices matter.","","GEOGRAPHY: Theater plot is a schematic game map, not actual geography."};
 for(int i=0;i<(int)(sizeof lines/sizeof lines[0]);i++){int yy=y+32+i*23;if(yy>y+h-8)break;txt(x+19,yy,0,i==0?CL_GREEN:(i<14?CL_TEXT:CL_DIM),lines[i]);}
}
static void right_status(void){int x=rx(),y=my(),w=rxw(),h=mh();panel(x,y,w,h,"TRACK CONTROL / COMMAND");int t=A.g.selected_track;
 if(game_valid(t)){SatTarget *v=&A.g.targets[t];int c=target_col(v);
 label(x+13,y+67,2,c,"%s",v->callsign);label(x+15,y+91,0,CL_DIM,"ACTIVE SENSOR-FUSED CONTACT");
 rule(x+13,y+105,w-26);label(x+15,y+129,0,CL_DIM,"CLASSIFICATION");label(x+15,y+152,1,c,"%s",v->declared==SAT_HOSTILE?"HOSTILE":v->declared==SAT_FRIENDLY?"FRIENDLY":"UNIDENTIFIED");
 label(x+15,y+184,0,CL_DIM,"TRACK CONFIDENCE");bar(x+15,y+195,w-31,10,v->quality,c);
 label(x+15,y+234,0,CL_TEXT,"ALTITUDE     %6.0f m",v->altitude);label(x+15,y+260,0,CL_TEXT,"SPEED        %6.0f km/h",hypotf(v->estvx,v->estvy)*3600.f);label(x+15,y+286,0,CL_TEXT,"HEADING      %6.0f deg",sat_heading(v->estvx,v->estvy));
 label(x+15,y+312,0,CL_TEXT,"IFF / XPDR   %s",v->transponder?"REPORTED":"NO REPLY");label(x+15,y+338,0,CL_TEXT,"VISUAL ID    %s",v->visually_identified?"OBSERVED":"NOT AVAILABLE");
 label(x+15,y+371,0,v->authorized?CL_RED:CL_DIM,"WEAPONS %s",v->authorized?"FREE / AUTHORIZED":"HOLD / SAFE");
 }else{txt(x+15,y+65,1,CL_DIM,"NO TARGET SELECTED");txt(x+15,y+95,0,CL_DIM,"Awaiting sensor reports.");txt(x+15,y+126,0,CL_DIM,"Choose a radar contact to");txt(x+15,y+146,0,CL_DIM,"scramble and identify it.");}
 int by=y+h-171,bw=(w-39)/2;
 button(x+13,by,bw,37,"F  SCRAMBLE",CL_CYAN,game_valid(t));button(x+26+bw,by,bw,37,"H  HOSTILE",CL_RED,game_valid(t));
 button(x+13,by+44,bw,37,"I  FRIENDLY",CL_BLUE,game_valid(t));button(x+26+bw,by+44,bw,37,"E  WEAPONS",CL_AMBER,game_valid(t));
 button(x+13,by+88,w-26,37,"K  SAM ENGAGEMENT",CL_RED,game_valid(t));
 label(x+16,y+h-15,0,CL_DIM,"SELECT / IDENTIFY / AUTHORIZE / ENGAGE");}
static void footer(void){int x=mx(),y=foot_y(),w=A.width-mx()-12,h=181;panel(x,y,w,h,"ACTIVE CONTACT REGISTER   /   CLICK TO SELECT   /   TAB TO CYCLE");
 int col[6]={x+14,x+110,x+248,x+394,x+539,x+676};
 label(col[0],y+51,0,CL_DIM,"CONTACT");label(col[1],y+51,0,CL_DIM,"STATUS");label(col[2],y+51,0,CL_DIM,"TRACK QUALITY");label(col[3],y+51,0,CL_DIM,"ALTITUDE");label(col[4],y+51,0,CL_DIM,"SPEED");label(col[5],y+51,0,CL_DIM,"IFF / AUTHORITY");
 int count=0;for(int i=0;i<SAT_TARGETS&&count<4;i++){SatTarget *t=&A.g.targets[i];if(!game_valid(i))continue;int ry=y+58+count*28;if(i==A.g.selected_track)box(x+8,ry-1,w-16,27,CL_PANEL);label(col[0],ry+19,0,target_col(t),"%s",t->callsign);label(col[1],ry+19,0,CL_TEXT,"%s",t->declared==SAT_HOSTILE?"HOSTILE":t->declared==SAT_FRIENDLY?"FRIEND":"UNKNOWN");bar(col[2],ry+8,87,9,t->quality,CL_GREEN);label(col[3],ry+19,0,CL_TEXT,"%5.0f m",t->altitude);label(col[4],ry+19,0,CL_TEXT,"%4.0f km/h",hypotf(t->estvx,t->estvy)*3600);label(col[5],ry+19,0,t->authorized?CL_RED:CL_DIM,"%s / %s",t->transponder?"XPDR":"--",t->authorized?"FREE":"HOLD");count++;}
 if(!count)txt(x+15,y+90,0,CL_DIM,"No reliable tracks yet. Radar stations are scanning the sector.");
 box(0,A.height-12,A.width,12,CL_PANEL);
 if(A.notice_until>A.render_t)label(205,A.height-2,0,CL_AMBER,"%s",A.notice);else txt(205,A.height-2,0,CL_DIM,"SEARCH AND TRACK II   //   TACTICAL TRAINING SIMULATION   //   ALL TIMES SIMULATED");
}
static void render_game(void){draw_frame();switch(A.page){case 0:draw_tactical();break;case 1:draw_ppi();break;case 2:draw_operations();break;case 3:draw_network();break;case 4:draw_intel();break;case 5:draw_log();break;default:draw_manual();break;}right_status();footer();if(A.g.paused){box(mx()+mw()/2-86,my()+mh()-66,172,36,CL_PANEL);stroke(mx()+mw()/2-86,my()+mh()-66,172,36,CL_AMBER);txt(mx()+mw()/2-66,my()+mh()-42,1,CL_AMBER,"SIMULATION PAUSED");}
 if(A.g.complete){int x=mx()+mw()/2-230,y=my()+mh()/2-115;box(x,y,460,234,CL_PANEL);stroke(x,y,460,234,A.g.victory?CL_GREEN:CL_RED);txt(x+34,y+48,2,A.g.victory?CL_GREEN:CL_RED,A.g.victory?"WATCH COMPLETED":"DEFENSE INCOMPLETE");label(x+35,y+85,1,CL_WHITE,"SECTOR INTEGRITY: %d%%",A.g.integrity);label(x+35,y+118,1,CL_TEXT,"SCORE: %d  /  INTERCEPTS: %d",A.g.score,A.g.intercepts);label(x+35,y+150,1,CL_TEXT,"PENETRATIONS: %d  /  CIVIL LOSSES: %d",A.g.penetrations,A.g.friendly_losses);txt(x+35,y+195,0,CL_AMBER,"PRESS N FOR A NEW OPERATION. S TO SAVE.");}}
static void draw_menu(void){box(0,0,A.width,A.height,CL_BG);int cx=A.width/2,cy=A.height/2;for(int r=65;r<370;r+=55)oval(cx,cy,r,CL_GRID);line(cx-380,cy,cx+380,cy,CL_GRID);line(cx,cy-380,cx,cy+380,CL_GRID);float ph=A.render_t*.25f;for(int i=0;i<12;i++){float an=ph+i*2*PI/12;dot(cx+(int)(cosf(an)*(80+i*20)),cy+(int)(sinf(an)*(80+i*20)),i%3==0?4:2,CL_GREEN);}
 box(0,0,A.width,80,CL_SURFACE);box(0,0,6,80,CL_GREEN);txt(34,37,2,CL_WHITE,"SEARCH / TRACK 2");txt(35,62,0,CL_DIM,"COLD WAR AIR DEFENSE COMMAND  |  NATIVE C17 / LINUX");
 panel(cx-355,cy-225,710,445,"SELECT OPERATIONAL COMMAND");txt(cx-324,cy-154,2,CL_WHITE,"YOUR WATCH BEGINS");txt(cx-323,cy-120,0,CL_DIM,"Monitor the sector. Identify unknown contacts. Scramble interceptors.");txt(cx-323,cy-99,0,CL_DIM,"Protect civilian lives. Preserve the integrity of the defense network.");
 int yy=cy-58;button(cx-323,yy,311,135,"",CL_CYAN,A.menu_faction==SAT_USA);button(cx+11,yy,311,135,"",CL_RED,A.menu_faction==SAT_USSR);
 txt(cx-305,yy+28,1,CL_CYAN,"UNITED STATES / NORAD");txt(cx+30,yy+28,1,CL_RED,"SOVIET UNION / PVO");
 txt(cx-305,yy+52,0,CL_DIM,"NORTH ATLANTIC DEFENSE");txt(cx+30,yy+52,0,CL_DIM,"BARENTS DEFENSE DISTRICT");txt(cx-305,yy+78,0,CL_TEXT,"F-106A  /  F-4E");txt(cx+30,yy+78,0,CL_TEXT,"Su-15TM  /  MiG-21");txt(cx-305,yy+104,0,CL_DIM,"5 RADARS  /  3 SAM BATTERIES");txt(cx+30,yy+104,0,CL_DIM,"5 RADARS  /  3 SAM BATTERIES");
 button(cx-323,cy+111,645,56,"ENTER  /  ASSUME COMMAND",CL_GREEN,1);
 txt(cx-321,cy+194,0,CL_DIM,"KEYS: A = USA / B = USSR / ENTER = DEPLOY / L = RESTORE SAVE");
 label(33,A.height-32,0,CL_GREEN,"INDEPENDENT 2D SIMULATION  /  NOT GEOGRAPHIC CARTOGRAPHY");}
static void draw(void){if(!A.back)return;if(A.menu)draw_menu();else render_game();XCopyArea(A.d,A.back,A.w,A.gc,0,0,(unsigned)A.width,(unsigned)A.height,0,0);XFlush(A.d);}
static int pick_track_map(int px,int py){int best=-1;float nearest=26;for(int i=0;i<SAT_TARGETS;i++){SatTarget *t=&A.g.targets[i];if(!game_valid(i))continue;int x,y;if(A.page==1){SatRadar *r=&A.g.radars[A.g.selected_radar];float scale=scope_radius()/radar_range();x=scope_center_x()+(int)((t->estx-r->x)*scale);y=scope_center_y()+(int)((t->esty-r->y)*scale);}else{x=world_x(t->estx);y=world_y(t->esty);}float d=hypotf(px-x,py-y);if(d<nearest){nearest=d;best=i;}}return best;}
static void do_control(char k){int t=A.g.selected_track;switch(k){case 'f':sat_scramble(&A.g,t);break;case 'h':sat_identify(&A.g,t,SAT_HOSTILE);break;case 'i':sat_identify(&A.g,t,SAT_FRIENDLY);break;case 'e':sat_authorize(&A.g,t);break;case 'k':sat_sam_fire(&A.g,t,-1);break;case 'p':sat_radar_toggle(&A.g,A.g.selected_radar);break;case 'z':A.zoom=(A.zoom+1)%3;break;default:break;}}
static void update_notice(const char *s){snprintf(A.notice,sizeof A.notice,"%s",s);A.notice_until=A.render_t+8.0f;}
static void click(int px,int py){if(A.menu){int cx=A.width/2,cy=A.height/2;int y=cy-58;if(inside(px,py,cx-323,y,311,135))A.menu_faction=SAT_USA;else if(inside(px,py,cx+11,y,311,135))A.menu_faction=SAT_USSR;else if(inside(px,py,cx-323,cy+111,645,56)){sat_new(&A.g,A.menu_faction,(unsigned)time(NULL));A.menu=0;A.page=0;}return;}
 for(int i=0;i<7;i++)if(inside(px,py,0,118+i*53,194,43)){A.page=i;return;}
 int x=rx(),y=my(),w=rxw(),h=mh(),by=y+h-171,bw=(w-39)/2;
 if(inside(px,py,x+13,by,bw,37))do_control('f');else if(inside(px,py,x+26+bw,by,bw,37))do_control('h');else if(inside(px,py,x+13,by+44,bw,37))do_control('i');else if(inside(px,py,x+26+bw,by+44,bw,37))do_control('e');else if(inside(px,py,x+13,by+88,w-26,37))do_control('k');
 if(inside(px,py,mx(),my()+31,mw(),mh()-31)){if(A.page==3){int idx=(py-(my()+31)-14)/80;if(idx>=0&&idx<SAT_RADARS){A.g.selected_radar=idx;update_notice("Radar selected. Press P to toggle transmitter; 2 for PPI.");}}
 else if(A.page==2){int idx=(py-(my()+31)-154)/39;if(idx>=0&&idx<SAT_FIGHTERS)A.g.selected_fighter=idx;}
 else if(A.page==0||A.page==1){int v=pick_track_map(px,py);if(v>=0)A.g.selected_track=v;}}
 if(inside(px,py,mx(),foot_y()+55,A.width-mx(),112)){int row=(py-(foot_y()+58))/28;if(row>=0&&row<4){int count=0;for(int i=0;i<SAT_TARGETS;i++)if(game_valid(i)){if(count==row){A.g.selected_track=i;break;}count++;}}}
}
static void save_path(void){const char *xdg=getenv("XDG_DATA_HOME");const char *home=getenv("HOME");char dir[920];if(xdg&&xdg[0])snprintf(dir,sizeof dir,"%s",xdg);else if(home&&home[0]){snprintf(dir,sizeof dir,"%s/.local",home);mkdir(dir,0700);snprintf(dir,sizeof dir,"%s/.local/share",home);}else snprintf(dir,sizeof dir,".");mkdir(dir,0700);snprintf(A.savepath,sizeof A.savepath,"%s/search-and-track-2",dir);mkdir(A.savepath,0700);size_t l=strlen(A.savepath);snprintf(A.savepath+l,sizeof A.savepath-l,"/watch.sat2");}
static void input_key(XKeyEvent *e){KeySym key=XLookupKeysym(e,0);char c=0;if(key>=XK_a&&key<=XK_z)c=(char)key;if(key>=XK_A&&key<=XK_Z)c=(char)(key-XK_A+'a');
 if(A.menu){if(c=='a')A.menu_faction=SAT_USA;if(c=='b')A.menu_faction=SAT_USSR;if(key==XK_Return){sat_new(&A.g,A.menu_faction,(unsigned)time(NULL));A.menu=0;A.page=0;}if(c=='l'){if(sat_load(&A.g,A.savepath)){A.menu=0;update_notice("Saved watch restored.");}}return;}
 if(key>=XK_1&&key<=XK_7){A.page=key-XK_1;return;}
 if(key==XK_Escape){A.menu=1;A.menu_faction=A.g.faction;return;}
 if(key==XK_space){A.g.paused=!A.g.paused;return;}
 if(key==XK_Tab){sat_cycle_track(&A.g,1);return;}
 if(key==XK_bracketright||key==XK_plus||key==XK_equal){A.g.time_scale=A.g.time_scale==1?4:A.g.time_scale==4?8:A.g.time_scale==8?16:32;return;}
 if(key==XK_bracketleft||key==XK_minus){A.g.time_scale=A.g.time_scale==32?16:A.g.time_scale==16?8:A.g.time_scale==8?4:1;return;}
 if(c=='n'){A.menu=1;A.menu_faction=A.g.faction;return;}
 if(c=='s'){update_notice(sat_save(&A.g,A.savepath)?"OPERATION SAVED TO LOCAL DISK":"ERROR SAVING OPERATION");return;}
 if(c=='l'){update_notice(sat_load(&A.g,A.savepath)?"OPERATION RESTORED FROM LOCAL DISK":"NO VALID SAVE FOUND");return;}
 if(key==XK_question){A.page=6;return;}
 if(c)do_control(c);
}
static double now_sec(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static void recreate_back(void){if(A.back)XFreePixmap(A.d,A.back);A.back=XCreatePixmap(A.d,A.w,(unsigned)A.width,(unsigned)A.height,(unsigned)DefaultDepth(A.d,DefaultScreen(A.d)));}
static unsigned long parse_color(const char *s){XColor c,exact;Colormap cm=DefaultColormap(A.d,DefaultScreen(A.d));if(XAllocNamedColor(A.d,cm,s,&c,&exact))return c.pixel;return BlackPixel(A.d,DefaultScreen(A.d));}
static void init_visual(void){const char *colors[CL_MAX]={"#071018","#0c1923","#122633","#24404a","#173039","#b5c8c7","#758d92","#65e4ac","#326c5a","#f3bd73","#fb7372","#71a5e6","#071b20","#102b2f","#e8f3eb","#73d6dd","#2a4650","#4f252a"};for(int i=0;i<CL_MAX;i++)A.color[i]=parse_color(colors[i]);
 const char *fontnames[]={"9x15","9x15bold","10x20"};for(int i=0;i<3;i++){A.fonts[i]=XLoadQueryFont(A.d,fontnames[i]);if(!A.fonts[i])A.fonts[i]=XLoadQueryFont(A.d,"fixed");}}
int main(int argc,char **argv){if(argc>=2&&!strcmp(argv[1],"--smoke")){SatGame *g=malloc(sizeof *g);if(!g)return 2;for(int faction=0;faction<2;faction++){sat_new(g,faction,42);for(int i=0;i<36000;i++)sat_tick(g,.1f);printf("%s simulation: %d score, %d%% integrity, %d events, %d waves\n",faction?"PVO":"NORAD",g->score,g->integrity,g->event_count,g->wave);if(g->wave<4){free(g);return 1;}}free(g);return 0;}
 memset(&A,0,sizeof A);A.width=1440;A.height=900;A.menu=1;A.menu_faction=SAT_USA;A.zoom=1;sat_new(&A.g,SAT_USA,42);save_path();A.d=XOpenDisplay(NULL);if(!A.d){fprintf(stderr,"Unable to connect to X11 display. Set DISPLAY or run in a desktop session.\n");return 1;}int sc=DefaultScreen(A.d);A.w=XCreateSimpleWindow(A.d,RootWindow(A.d,sc),80,60,(unsigned)A.width,(unsigned)A.height,0,0,BlackPixel(A.d,sc));XStoreName(A.d,A.w,"Search and Track 2 — Air Defense Command");XSelectInput(A.d,A.w,ExposureMask|KeyPressMask|ButtonPressMask|StructureNotifyMask);XSizeHints hints={0};hints.flags=PMinSize;hints.min_width=MIN_W;hints.min_height=MIN_H;XSetWMNormalHints(A.d,A.w,&hints);Atom wm=XInternAtom(A.d,"WM_DELETE_WINDOW",False);XSetWMProtocols(A.d,A.w,&wm,1);A.gc=XCreateGC(A.d,A.w,0,NULL);init_visual();recreate_back();XMapWindow(A.d,A.w);
 double last=now_sec(),renderlast=0,acc=0;int done=0;while(!done){while(XPending(A.d)){XEvent e;XNextEvent(A.d,&e);if(e.type==ClientMessage&&((Atom)e.xclient.data.l[0])==wm)done=1;else if(e.type==KeyPress)input_key(&e.xkey);else if(e.type==ButtonPress&&e.xbutton.button==Button1)click(e.xbutton.x,e.xbutton.y);else if(e.type==ConfigureNotify&&(e.xconfigure.width!=A.width||e.xconfigure.height!=A.height)){A.width=clamp_i(e.xconfigure.width,MIN_W,4096);A.height=clamp_i(e.xconfigure.height,MIN_H,2160);recreate_back();}}
 double now=now_sec(),dt=now-last;last=now;if(dt>.2)dt=.2;if(dt<0)dt=0;if(!A.menu&&!A.g.paused&&!A.g.complete){acc+=dt*A.g.time_scale;int steps=0;while(acc>=.1&&steps++<85){sat_tick(&A.g,.1f);acc-=.1;}if(steps>=85)acc=0;}else acc=0;
 if(now-renderlast>=1.0/30.0){A.render_t=(float)now;draw();renderlast=now;}
 struct timespec sleep_time={0,10000000};nanosleep(&sleep_time,NULL);}
 if(A.back)XFreePixmap(A.d,A.back);
 for(int i=0;i<3;i++)if(A.fonts[i])XFreeFont(A.d,A.fonts[i]);
 XFreeGC(A.d,A.gc);XDestroyWindow(A.d,A.w);XCloseDisplay(A.d);return 0;}
