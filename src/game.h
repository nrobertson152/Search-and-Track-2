#ifndef SEARCH_TRACK_GAME_H
#define SEARCH_TRACK_GAME_H
#include <stdint.h>
#include <stddef.h>

#define SAT_TARGETS 48
#define SAT_FIGHTERS 8
#define SAT_RADARS 5
#define SAT_SAMS 3
#define SAT_EVENTS 140
#define SAT_WORLD_W 1000.0f
#define SAT_WORLD_H 720.0f

enum { SAT_USA=0, SAT_USSR=1 };
enum { SAT_UNKNOWN=0, SAT_FRIENDLY=1, SAT_HOSTILE=2 };
enum { SAT_CIVIL=0, SAT_BOMBER=1, SAT_RECON=2, SAT_DECOY=3, SAT_ESCORT=4 };
enum { SAT_READY=0, SAT_LAUNCHING=1, SAT_INTERCEPT=2, SAT_RTB=3, SAT_TURNAROUND=4 };

typedef struct {
    int used, alive, type, declared, authorized, visually_identified, transponder;
    int serial, destroyed_by, escaped, attacked;
    float x,y, vx,vy, altitude, rcs, jammer, signature;
    float quality, last_hit, estx, esty, estvx, estvy, seen_at;
    float last_update;
    char callsign[22];
} SatTarget;
typedef struct {
    char name[34]; float x,y,range,scan,sweep,gain,health,altitude;
    int active, scans, detections;
} SatRadar;
typedef struct {
    char name[24]; float x,y; int ammo, reload;
} SatSam;
typedef struct {
    char name[24], model[16]; float x,y,base_x,base_y, fuel,speed,launch_timer;
    int status,target,ammo,kills,base;
} SatFighter;
typedef struct {char text[150]; float at; int level;} SatEvent;
typedef struct {
    int faction, paused, time_scale, selected_track, selected_fighter, selected_radar;
    int score, integrity, engagements, friendly_losses, wave, victory, complete;
    int intercepts, penetrations, identifications, false_alerts, radar_losses;
    int track_counter, next_event, event_count;
    float time, weather, noise, next_spawn, next_civilian, next_repair;
    uint32_t rng;
    SatTarget targets[SAT_TARGETS];
    SatRadar radars[SAT_RADARS];
    SatSam sams[SAT_SAMS];
    SatFighter fighters[SAT_FIGHTERS];
    SatEvent events[SAT_EVENTS];
} SatGame;

void sat_new(SatGame *g, int faction, unsigned seed);
void sat_tick(SatGame *g, float dt);
void sat_event(SatGame *g,int level,const char *fmt,...);
void sat_scramble(SatGame *g,int track);
void sat_identify(SatGame *g,int track,int classification);
void sat_authorize(SatGame *g,int track);
void sat_sam_fire(SatGame *g,int track,int battery);
void sat_radar_toggle(SatGame *g,int radar);
void sat_cycle_track(SatGame *g,int direction);
int sat_save(const SatGame *g,const char *path);
int sat_load(SatGame *g,const char *path);
const char *sat_type_name(int type);
float sat_distance(float x1,float y1,float x2,float y2);
float sat_heading(float vx,float vy);
#endif
