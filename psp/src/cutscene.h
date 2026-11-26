/* Scripted cutscenes: camera shots and moves, actors walking, facing and
 * gesturing, voiced subtitles, fades and titles. */
#ifndef UC_CUTSCENE_H
#define UC_CUTSCENE_H

#include "actor.h"

/* actor slots used by the story */
enum { A_ANDRIY = 0, A_TARAS, A_GALYA, A_KABAN, A_SELLER, A_THUG1, A_THUG2, N_ACTORS };

enum { CS_NONE = 0, CS_INTRO, CS_GARAGE, CS_MARKET, CS_KIOSK, CS_FAIL, CS_KABAN, CS_COUNT };

/* requests a scene makes to the game */
enum { CS_REQ_CHASER = 1, CS_REQ_KABAN_SETUP = 2 };

#define CS_MAX_CMDS 48

typedef struct {
    int type, actor, other, gest;
    V3 p, q, r, s;
    float t;
    const char *text, *text2;
} CsCmd;

typedef struct {
    int id, running, n, i, line;
    CsCmd cmd[CS_MAX_CMDS];
    float wait, t;
    /* camera */
    int has_cam, auto_cam;
    V3 eye0, tgt0, eye1, tgt1;
    float cam_t, cam_dur;
    /* subtitles */
    const char *sub_who, *sub_text;
    float sub_t;
    int speaker;
    float min_say;
    /* screen effects */
    float fade, fade_from, fade_to, fade_t, fade_dur;
    const char *title, *title2;
    float title_t;
    int requests;    /* CS_REQ_* bits raised this frame */
    char voice_id[24];
} Cutscene;

typedef struct {
    Ped *actors[N_ACTORS];
    /* starts a voiced line; voice_id names an optional voice/<id>.wav */
    void (*say)(int actor, const char *text, const char *voice_id);
    int (*speaking)(void);
} CsIO;

void cs_start(Cutscene *c, int id);
/* advance: X pressed (finishes the current line); skip: START (ends the scene) */
void cs_update(Cutscene *c, const CsIO *io, float dt, int advance, int skip);
void cs_camera(const Cutscene *c, const CsIO *io, V3 *eye, V3 *target);
const char *cs_speaker_name(int actor);

#endif
