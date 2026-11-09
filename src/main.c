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
#define MIN_H 860
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
#define CL_METAL 18
#define CL_METAL_LIT 19
#define CL_METAL_DARK 20
#define CL_BEZEL 21
#define CL_SCREEN_EDGE 22
#define CL_SCAN 23
#define CL_GLOW 24
#define CL_WARM 25
#define CL_SKY 26
#define CL_MAX 27

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
static void screw(int x,int y){dot(x,y,3,CL_METAL_LIT);dot(x,y,2,CL_METAL_DARK);line(x-1,y+1,x+1,y-1,CL_MUTED);}
static void lamp(int x,int y,int color,int on){dot(x,y,6,CL_METAL_DARK);dot(x,y,4,on?color:CL_GRID);dot(x-1,y-1,1,on?CL_WHITE:CL_MUTED);}
static void button(int x,int y,int w,int h,const char *title,int accent,int active){
 box(x+2,y+3,w,h,CL_METAL_DARK);box(x,y,w,h,CL_METAL);
 line(x,y,x+w-1,y,CL_METAL_LIT);line(x,y,x,y+h-1,CL_METAL_LIT);
 line(x+w-1,y,x+w-1,y+h,CL_METAL_DARK);line(x,y+h-1,x+w-1,y+h-1,CL_METAL_DARK);
 box(x+4,y+4,w-8,h-9,active?CL_PANEL:CL_SURFACE);
 box(x+5,y+5,4,h-11,accent);
 if(active)box(x+10,y+h-8,w-16,2,accent);
 txt(x+16,y+h/2+5,0,active?CL_WHITE:CL_TEXT,title);
}

static void bar(int x,int y,int w,int h,float v,int col){box(x,y,w,h,CL_GRID);if(v>0){int q=(int)(w*fmaxf(0,fminf(1,v)));box(x,y,q,h,col);}}
static void rule(int x,int y,int w){line(x,y,x+w,y,CL_LINE);}
static void panel(int x,int y,int w,int h,const char *heading){
 /* Rolled-steel bezel, inset glass, raised cap and fixing screws. */
 box(x+2,y+3,w,h,CL_METAL_DARK);
 box(x,y,w,h,CL_METAL);
 box(x+2,y+2,w-4,h-4,CL_BEZEL);
 box(x+6,y+6,w-12,h-12,CL_SURFACE);
 line(x+2,y+2,x+w-3,y+2,CL_METAL_LIT);
 line(x+2,y+2,x+2,y+h-3,CL_METAL_LIT);
 line(x+5,y+h-5,x+w-5,y+h-5,CL_METAL_DARK);
 line(x+w-5,y+5,x+w-5,y+h-5,CL_METAL_DARK);
 screw(x+7,y+7);screw(x+w-8,y+7);
 if(h>55){screw(x+7,y+h-8);screw(x+w-8,y+h-8);}
 if(heading){box(x+10,y+10,w-20,27,CL_PANEL);
   box(x+10,y+10,3,27,CL_GREEN);txt(x+20,y+28,0,CL_WHITE,heading);
   line(x+10,y+38,x+w-11,y+38,CL_LINE);}
}

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
static void headline(const char *title,const char *sub){
 int x=mx(),y=my(),w=mw();
 box(x,y,w,31,CL_METAL);box(x+2,y+3,w-4,25,CL_PANEL);
 box(x+3,y+3,4,25,CL_GREEN);txt(x+16,y+21,1,CL_WHITE,title);
 if(w>690)txt(x+w-345,y+21,0,CL_DIM,sub);
 line(x,y+30,x+w,y+30,CL_METAL_LIT);
}

static const char *navlabels[]={"TACTICAL PLOT","RADAR PPI","OPERATIONS","SENSOR NET","INTELLIGENCE","EVENT LOG","FIELD MANUAL"};
static void draw_frame(void){
 /* Command bunker shell: cold steel structure, illuminated instruments. */
 box(0,0,A.width,A.height,CL_METAL_DARK);
 for(int y=0;y<A.height;y+=8)line(0,y,A.width,y,CL_METAL);
 box(4,4,A.width-8,67,CL_METAL);
 box(7,7,A.width-14,61,CL_SURFACE);
 box(7,7,5,61,CL_GREEN);
 line(7,8,A.width-8,8,CL_METAL_LIT);line(7,68,A.width-8,68,CL_LINE);
 screw(20,17);screw(20,61);
 txt(34,32,2,CL_WHITE,"SEARCH / TRACK II");
 txt(35,53,0,CL_WARM,"AIR DEFENSE COMMAND  |  OPERATIONS CONTROL");
 int faction=A.g.faction;
 box(340,16,4,43,CL_GREEN);
 label(354,34,1,CL_GREEN,"%s",faction==SAT_USA?"NORAD  /  NORTH ATLANTIC SECTOR":"PVO  /  BARENTS DEFENSE DISTRICT");
 txt(355,54,0,CL_DIM,"CONTINUOUS SURVEILLANCE     ///     THEATER EXERCISE");
 int now=(8*3600+30*60+(int)A.g.time)%86400;
 box(A.width-325,12,307,50,CL_BEZEL);
 box(A.width-320,17,297,41,CL_SCREEN_EDGE);
 label(A.width-309,36,1,CL_WARM,"%02d:%02d:%02d Z",now/3600,(now%3600)/60,now%60);
 label(A.width-308,51,0,A.g.integrity<45?CL_RED:CL_GREEN,"DEFENSE INTEGRITY  %03d%%",A.g.integrity);
 lamp(A.width-41,29,A.g.integrity<45?CL_RED:CL_GREEN,(int)(A.render_t*2)%2||A.g.integrity>=45);
 /* Fixed station selector rack. */
 box(3,76,195,A.height-91,CL_METAL);
 box(9,82,183,A.height-105,CL_BEZEL);
 box(13,86,175,A.height-113,CL_SURFACE);
 screw(17,90);screw(184,90);screw(17,A.height-35);screw(184,A.height-35);
 txt(28,113,0,CL_WARM,"STATION SELECTOR");
 line(25,122,179,122,CL_LINE);
 for(int i=0;i<7;i++){
  int y=131+i*52,sel=A.page==i;
  box(23,y,160,43,CL_METAL_DARK);
  box(26,y+2,154,38,sel?CL_PANEL:CL_METAL);
  box(29,y+6,4,30,sel?CL_GREEN:CL_GRID);
  lamp(42,y+20,sel?CL_GREEN:CL_DIM,sel);
  label(54,y+17,0,sel?CL_WHITE:CL_TEXT,"%02d",i+1);
  txt(76,y+18,0,sel?CL_GREEN:CL_DIM,navlabels[i]);
  if(sel)line(54,y+34,171,y+34,CL_GREEN2);
 }
 int zy=526;
 line(25,zy-10,178,zy-10,CL_LINE);
 txt(28,zy+15,0,CL_WARM,"WATCH CONTROL");
 lamp(36,zy+38,A.g.paused?CL_AMBER:CL_GREEN,1);
 txt(51,zy+43,0,A.g.paused?CL_AMBER:CL_GREEN,A.g.paused?"HOLD / PAUSED":"LIVE / ACTIVE");
 label(27,zy+72,0,CL_TEXT,"TIME COMPRESS   %02dx",A.g.time_scale);
 txt(28,zy+100,0,CL_DIM,"SPACE  RUN / HOLD");
 txt(28,zy+121,0,CL_DIM,"[ / ]  TIME RATE");
 txt(28,zy+142,0,CL_DIM,"TAB    NEXT CONTACT");
 txt(28,zy+163,0,CL_DIM,"S / L  SAVE / LOAD");
 int by=A.height-119;
 box(25,by,151,1,CL_LINE);
 txt(28,by+25,0,CL_TEXT,"N   NEW WATCH");
 txt(28,by+45,0,CL_TEXT,"?   FIELD MANUAL");
 txt(28,by+72,0,CL_GREEN,"SYSTEM / LOCAL");
 /* Decorative upper/lower monitor rack rails. */
 line(202,74,A.width-7,74,CL_METAL_LIT);
 line(202,A.height-13,A.width-7,A.height-13,CL_METAL_LIT);
}

/* Opaque CRT: scanlines and illuminated registration corners, no GL dependency. */
static void crt_scanlines(int x,int y,int w,int h){
 for(int sy=y+3;sy<y+h-3;sy+=5)line(x+3,sy,x+w-4,sy,CL_SCAN);
}
static void reticle(int x,int y,int w,int h){
 int c=CL_GREEN2,k=18;
 line(x+6,y+6,x+k+6,y+6,c);line(x+6,y+6,x+6,y+k+6,c);
 line(x+w-6,y+6,x+w-k-6,y+6,c);line(x+w-6,y+6,x+w-6,y+k+6,c);
 line(x+6,y+h-6,x+k+6,y+h-6,c);line(x+6,y+h-6,x+6,y+h-k-6,c);
 line(x+w-6,y+h-6,x+w-k-6,y+h-6,c);line(x+w-6,y+h-6,x+w-6,y+h-k-6,c);
}
static void draw_map_grid(void){
 int x=mx(),y=my()+31,w=mw(),h=mh()-31;
 box(x,y,w,h,CL_BEZEL);box(x+5,y+5,w-10,h-10,CL_SEA);
 for(int by=0;by<h;by+=26)box(x+5,y+by,w-10,1,CL_SCREEN_EDGE);
 for(int xx=0;xx<=1000;xx+=100){int sx=world_x((float)xx);line(sx,y+5,sx,y+h-6,CL_GRID);label(sx+4,y+20,0,CL_MUTED,"%03d",xx);}
 for(int yy=0;yy<=720;yy+=90){int sy=world_y((float)yy);if(sy<y+5)continue;line(x+5,sy,x+w-6,sy,CL_GRID);label(x+11,sy-5,0,CL_MUTED,"%03d",yy);}
 XPoint p[32];int n=0;
 float coast[14][2]={{0,0},{340,0},{365,65},{345,125},{390,176},{359,246},{386,322},{362,370},{422,456},{381,510},{405,590},{380,660},{425,720},{0,720}};
 for(int i=0;i<14;i++)p[n++]=(XPoint){(short)world_x(A.g.faction==SAT_USA?coast[i][0]:1000-coast[i][0]),(short)world_y(coast[i][1])};
 poly(p,n,CL_LAND);fg(CL_LINE);XDrawLines(A.d,A.back,A.gc,p,n,CoordModeOrigin);
 for(int i=0;i<10;i++){
  float lx=A.g.faction==SAT_USA?75+i*26:925-i*26;
  label(world_x(lx),world_y(65+i*59),0,CL_MUTED,"%c-%02d",A.g.faction==SAT_USA?'N':'P',i+1);
 }
 for(int i=0;i<70;i++){
  int px=x+11+(i*1171%(w-22)),py=y+15+(i*2003%(h-27));
  if((i%4)==0)box(px,py,2,1,CL_GRID);
 }
 /* Radar-surveillance authenticity: common azimuth graduation and datum axes. */
 line(x+w/2,y+5,x+w/2,y+h-6,CL_GREEN2);
 line(x+5,y+h/2,x+w-6,y+h/2,CL_GREEN2);
 crt_scanlines(x,y,w,h);reticle(x,y,w,h);
 box(x+15,y+h-37,323,25,CL_BEZEL);
 txt(x+25,y+h-19,0,CL_GREEN,"SYNTHETIC AIR PICTURE / LIVE PLOTS");
 box(x+w-174,y+42,156,32,CL_BEZEL);
 txt(x+w-161,y+63,0,CL_WARM,"SURV  ON   1:250K");
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
static int scope_center_y(void){return my()+31+(mh()-31-60)/2;}
static int scope_radius(void){return min_i(mw()-104,mh()-167)/2;}
static void draw_ppi(void){
 int x=mx(),y=my()+31,w=mw(),h=mh()-31;
 box(x,y,w,h,CL_METAL_DARK);box(x+5,y+5,w-10,h-10,CL_METAL);
 box(x+15,y+8,w-30,h-16,CL_BEZEL);
 int cx=scope_center_x(),cy=scope_center_y(),rad=scope_radius();
 /* Concentric hardware bezel and dark convex CRT phosphor glass. */
 for(int r=rad+21;r>=rad+7;r-=3)oval(cx,cy,r,r%2?CL_METAL_LIT:CL_METAL_DARK);
 dot(cx,cy,rad+5,CL_METAL_DARK);
 dot(cx,cy,rad,CL_LAND);
 dot(cx-rad/3,cy-rad/3,rad/2,CL_SCREEN_EDGE);
 for(int j=1;j<=4;j++){
  int rr=rad*j/4;oval(cx,cy,rr,j==4?CL_GREEN2:CL_GRID);
  label(cx+8,cy-rr+18,0,CL_MUTED,"%d",(int)(radar_range()*j/4));
 }
 for(int a=0;a<360;a+=5){
  float ang=a*(float)PI/180.f;
  int tick=a%30==0?14:5;
  int sx=cx+(int)(sinf(ang)*(rad+7)),sy=cy-(int)(cosf(ang)*(rad+7));
  int ex=cx+(int)(sinf(ang)*(rad+7+tick)),ey=cy-(int)(cosf(ang)*(rad+7+tick));
  line(sx,sy,ex,ey,a%30==0?CL_TEXT:CL_MUTED);
  if(a%30==0){label(cx+(int)(sinf(ang)*(rad+32))-12,cy-(int)(cosf(ang)*(rad+32))+4,0,CL_DIM,"%03d",a);}
 }
 line(cx-rad,cy,cx+rad,cy,CL_GRID);
 line(cx,cy-rad,cx,cy+rad,CL_GRID);
 /* Dim ghost persistence behind the live sweep; screen remains a simulation. */
 SatRadar *r=&A.g.radars[A.g.selected_radar];
 if(r->active){
  for(int k=27;k>=0;k--){
   float a=(r->sweep-k*1.65f)*(float)PI/180.f;
   int length=rad-(k%4)*2;
   line(cx,cy,cx+(int)(sinf(a)*length),cy-(int)(cosf(a)*length),k<3?CL_GREEN:k<11?CL_GREEN2:CL_GRID);
  }
 }
 for(int i=0;i<64;i++){
  int rr=(i*1799)%rad;float a=(float)((i*67+11)*PI/180.0);
  int sx=cx+(int)(cosf(a)*rr),sy=cy+(int)(sinf(a)*rr);
  dot(sx,sy,1,i%6?CL_GRID:CL_GREEN2);
 }
 float scale=(float)rad/radar_range();
 for(int i=0;i<SAT_TARGETS;i++){
  SatTarget *t=&A.g.targets[i];if(!game_valid(i))continue;
  float dx=t->estx-r->x,dy=t->esty-r->y;if(hypotf(dx,dy)>radar_range())continue;
  int px=cx+(int)(dx*scale),py=cy+(int)(dy*scale),c=target_col(t);
  dot(px,py,i==A.g.selected_track?12:8,CL_GREEN2);
  dot(px,py,i==A.g.selected_track?6:4,c);
  if(i==A.g.selected_track){oval(px,py,16,CL_WHITE);line(px-24,py,px-18,py,CL_WHITE);}
  label(px+15,py-10,0,c,"%s",t->callsign);
 }
 dot(cx,cy,4,CL_GREEN);dot(cx,cy,2,CL_WHITE);
 /* Embedded instrumentation strip beneath the scope. */
 box(x+24,y+h-55,w-48,39,CL_METAL);
 box(x+29,y+h-51,w-58,30,CL_BEZEL);
 lamp(x+45,y+h-35,CL_GREEN,r->active);
 label(x+61,y+h-30,0,CL_WARM,"%s",r->active?"ANTENNA / RADIATING":"ANTENNA / STANDBY");
 label(x+280,y+h-30,0,CL_TEXT,"RANGE  %03d KM",(int)radar_range());
 label(x+470,y+h-30,0,CL_TEXT,"GAIN  %02.0f%%",r->gain*100);
 label(x+w-190,y+h-30,0,CL_GREEN,"SWEEP  %03.0f DEG",r->sweep);
 headline("PPI / EARLY WARNING RADAR","PHOSPHOR SCOPE   /   TRACK ACQUISITION");
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
static void draw_menu(void){
 int cx=A.width/2,cy=A.height/2;
 box(0,0,A.width,A.height,CL_METAL_DARK);
 /* Deep observation pit / table silhouette. War-room, not a website. */
 for(int y=0;y<A.height;y+=14)line(0,y,A.width,y,CL_METAL);
 for(int r=80;r<A.width;r+=92)oval(cx,cy-90,r,CL_METAL);
 box(18,16,A.width-36,84,CL_METAL);
 box(23,21,A.width-46,74,CL_SURFACE);
 line(23,21,A.width-24,21,CL_METAL_LIT);
 box(23,21,6,74,CL_GREEN);
 txt(44,52,2,CL_WHITE,"SEARCH / TRACK II");
 txt(45,77,0,CL_WARM,"AIR DEFENSE COMMAND     //     OPERATIONAL CONTROL SYSTEM");
 lamp(A.width-103,57,CL_GREEN,1);
 txt(A.width-83,62,0,CL_GREEN,"READY");
 /* Three surrounding instrument monitors imply a real room beyond the selection table. */
 for(int i=0;i<3;i++){
  int xx=cx-472+i*323,yy=117;
  box(xx,yy,297,136,CL_METAL);
  box(xx+6,yy+6,285,124,CL_BEZEL);
  box(xx+12,yy+12,273,112,CL_LAND);
  int mxp=xx+148,myp=yy+69;
  for(int r=21;r<=48;r+=13)oval(mxp,myp,r,CL_GRID);
  line(mxp-58,myp,mxp+58,myp,CL_GRID);line(mxp,myp-51,mxp,myp+51,CL_GRID);
  float az=A.render_t*.27f+i;
  line(mxp,myp,mxp+(int)(cosf(az)*48),myp+(int)(sinf(az)*48),CL_GREEN);
  lamp(xx+27,yy+25,i==1?CL_AMBER:CL_GREEN,1);
  screw(xx+8,yy+8);screw(xx+289,yy+8);
 }
 panel(cx-382,cy-181,764,477,"OPERATIONAL CONTROL  /  COMMAND AUTHORITY");
 txt(cx-345,cy-115,2,CL_WHITE,"ASSUME THE WATCH");
 txt(cx-345,cy-86,0,CL_TEXT,"Select a command authority. Your actions determine the fate of the sector.");
 txt(cx-345,cy-66,0,CL_DIM,"Real-time radar tracking / interceptor vectoring / weapons authorization");
 int yy=cy-31;
 button(cx-345,yy,335,145,"",CL_CYAN,A.menu_faction==SAT_USA);
 button(cx+10,yy,335,145,"",CL_RED,A.menu_faction==SAT_USSR);
 lamp(cx-319,yy+30,CL_CYAN,A.menu_faction==SAT_USA);
 lamp(cx+36,yy+30,CL_RED,A.menu_faction==SAT_USSR);
 txt(cx-303,yy+35,1,A.menu_faction==SAT_USA?CL_WHITE:CL_CYAN,"UNITED STATES / NORAD");
 txt(cx+52,yy+35,1,A.menu_faction==SAT_USSR?CL_WHITE:CL_RED,"SOVIET UNION / PVO");
 line(cx-321,yy+47,cx-37,yy+47,CL_LINE);line(cx+34,yy+47,cx+317,yy+47,CL_LINE);
 txt(cx-319,yy+73,0,CL_TEXT,"NORTH ATLANTIC DEFENSE");
 txt(cx+36,yy+73,0,CL_TEXT,"BARENTS DEFENSE DISTRICT");
 txt(cx-319,yy+99,0,CL_WARM,"F-106 DELTA DART / F-4");
 txt(cx+36,yy+99,0,CL_WARM,"Su-15 FLAGON / MiG-21");
 txt(cx-319,yy+126,0,CL_DIM,"5 SENSOR SITES  /  3 SAMS");
 txt(cx+36,yy+126,0,CL_DIM,"5 SENSOR SITES  /  3 SAMS");
 button(cx-345,cy+132,690,57,"   ENTER   /   INITIATE SECTOR WATCH",CL_GREEN,1);
 lamp(cx+280,cy+158,CL_GREEN,1);
 txt(cx-343,cy+218,0,CL_DIM,"A  USA    B  USSR    ENTER  DEPLOY    L  RESTORE SAVED OPERATION");
 box(0,A.height-42,A.width,42,CL_METAL);
 box(8,A.height-34,A.width-16,25,CL_SURFACE);
 txt(25,A.height-16,0,CL_GREEN,"SURVEILLANCE NETWORK    |    AUTHORIZED TRAINING SIMULATION    |    OFFLINE SYSTEM");
}

static void draw(void){if(!A.back)return;if(A.menu)draw_menu();else render_game();XCopyArea(A.d,A.back,A.w,A.gc,0,0,(unsigned)A.width,(unsigned)A.height,0,0);XFlush(A.d);}
static int pick_track_map(int px,int py){int best=-1;float nearest=26;for(int i=0;i<SAT_TARGETS;i++){SatTarget *t=&A.g.targets[i];if(!game_valid(i))continue;int x,y;if(A.page==1){SatRadar *r=&A.g.radars[A.g.selected_radar];float scale=scope_radius()/radar_range();x=scope_center_x()+(int)((t->estx-r->x)*scale);y=scope_center_y()+(int)((t->esty-r->y)*scale);}else{x=world_x(t->estx);y=world_y(t->esty);}float d=hypotf(px-x,py-y);if(d<nearest){nearest=d;best=i;}}return best;}
static void do_control(char k){int t=A.g.selected_track;switch(k){case 'f':sat_scramble(&A.g,t);break;case 'h':sat_identify(&A.g,t,SAT_HOSTILE);break;case 'i':sat_identify(&A.g,t,SAT_FRIENDLY);break;case 'e':sat_authorize(&A.g,t);break;case 'k':sat_sam_fire(&A.g,t,-1);break;case 'p':sat_radar_toggle(&A.g,A.g.selected_radar);break;case 'z':A.zoom=(A.zoom+1)%3;break;default:break;}}
static void update_notice(const char *s){snprintf(A.notice,sizeof A.notice,"%s",s);A.notice_until=A.render_t+8.0f;}
static void click(int px,int py){if(A.menu){int cx=A.width/2,cy=A.height/2;int y=cy-58;if(inside(px,py,cx-323,y,311,135))A.menu_faction=SAT_USA;else if(inside(px,py,cx+11,y,311,135))A.menu_faction=SAT_USSR;else if(inside(px,py,cx-323,cy+111,645,56)){sat_new(&A.g,A.menu_faction,(unsigned)time(NULL));A.menu=0;A.page=0;}return;}
 for(int i=0;i<7;i++)if(inside(px,py,23,131+i*52,161,43)){A.page=i;return;}
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
static void init_visual(void){
 const char *colors[CL_MAX]={
 "#0a0e0d", /* void */
 "#192421", /* recessed graphite display surround */
 "#263630", /* equipment control plate */
 "#42564c", /* engraved rules */
 "#274139", /* plot grid */
 "#d0d6c2", /* text */
 "#89998b", /* dim lettering */
 "#8ef7ba", /* live phosphor */
 "#417d67", /* phosphor faded */
 "#e4ba6a", /* caution tungsten */
 "#e47867", /* combat red */
 "#80a8c5", /* identification blue */
 "#0c1a17", /* water */
 "#172820", /* land */
 "#f0ecd7", /* illuminated white */
 "#89d7c1", /* cyan */
 "#436054", /* muted */
 "#59322f", /* dead red */
 "#3b433a", /* chassis steel */
 "#626958", /* chassis edge */
 "#161d19", /* cavity */
 "#0c1512", /* CRT bezel */
 "#142b21", /* CRT edge */
 "#10221b", /* scanlines */
 "#b1ffe2", /* phosphor peak */
 "#deb978", /* warm label */
 "#243c36"  /* auxiliary */
 };for(int i=0;i<CL_MAX;i++)A.color[i]=parse_color(colors[i]);
 const char *fontnames[]={"9x15","9x15bold","10x20"};
 for(int i=0;i<3;i++){A.fonts[i]=XLoadQueryFont(A.d,fontnames[i]);if(!A.fonts[i])A.fonts[i]=XLoadQueryFont(A.d,"fixed");}
}

int main(int argc,char **argv){if(argc>=2&&!strcmp(argv[1],"--smoke")){SatGame *g=malloc(sizeof *g);if(!g)return 2;for(int faction=0;faction<2;faction++){sat_new(g,faction,42);for(int i=0;i<36000;i++)sat_tick(g,.1f);printf("%s simulation: %d score, %d%% integrity, %d events, %d waves\n",faction?"PVO":"NORAD",g->score,g->integrity,g->event_count,g->wave);if(g->wave<4){free(g);return 1;}}free(g);return 0;}
 memset(&A,0,sizeof A);A.width=1440;A.height=900;A.menu=1;A.menu_faction=SAT_USA;A.zoom=1;sat_new(&A.g,SAT_USA,42);save_path();A.d=XOpenDisplay(NULL);if(!A.d){fprintf(stderr,"Unable to connect to X11 display. Set DISPLAY or run in a desktop session.\n");return 1;}int sc=DefaultScreen(A.d);A.w=XCreateSimpleWindow(A.d,RootWindow(A.d,sc),80,60,(unsigned)A.width,(unsigned)A.height,0,0,BlackPixel(A.d,sc));XStoreName(A.d,A.w,"Search and Track 2 — Air Defense Command");XSelectInput(A.d,A.w,ExposureMask|KeyPressMask|ButtonPressMask|StructureNotifyMask);XSizeHints hints={0};hints.flags=PMinSize;hints.min_width=MIN_W;hints.min_height=MIN_H;XSetWMNormalHints(A.d,A.w,&hints);Atom wm=XInternAtom(A.d,"WM_DELETE_WINDOW",False);XSetWMProtocols(A.d,A.w,&wm,1);A.gc=XCreateGC(A.d,A.w,0,NULL);init_visual();recreate_back();XMapWindow(A.d,A.w);
 double last=now_sec(),renderlast=0,acc=0;int done=0;while(!done){while(XPending(A.d)){XEvent e;XNextEvent(A.d,&e);if(e.type==ClientMessage&&((Atom)e.xclient.data.l[0])==wm)done=1;else if(e.type==KeyPress)input_key(&e.xkey);else if(e.type==ButtonPress&&e.xbutton.button==Button1)click(e.xbutton.x,e.xbutton.y);else if(e.type==ConfigureNotify&&(e.xconfigure.width!=A.width||e.xconfigure.height!=A.height)){A.width=clamp_i(e.xconfigure.width,MIN_W,4096);A.height=clamp_i(e.xconfigure.height,MIN_H,2160);recreate_back();}}
 double now=now_sec(),dt=now-last;last=now;if(dt>.2)dt=.2;if(dt<0)dt=0;if(!A.menu&&!A.g.paused&&!A.g.complete){acc+=dt*A.g.time_scale;int steps=0;while(acc>=.1&&steps++<85){sat_tick(&A.g,.1f);acc-=.1;}if(steps>=85)acc=0;}else acc=0;
 if(now-renderlast>=1.0/30.0){A.render_t=(float)now;draw();renderlast=now;}
 struct timespec sleep_time={0,10000000};nanosleep(&sleep_time,NULL);}
 if(A.back)XFreePixmap(A.d,A.back);
 for(int i=0;i<3;i++)if(A.fonts[i])XFreeFont(A.d,A.fonts[i]);
 XFreeGC(A.d,A.gc);XDestroyWindow(A.d,A.w);XCloseDisplay(A.d);return 0;}