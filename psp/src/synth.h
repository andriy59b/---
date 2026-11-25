/* Software synthesizer: original chiptune music in 90s styles and sound effects. */
#ifndef UC_SYNTH_H
#define UC_SYNTH_H

#define SYNTH_RATE 44100

enum { MUS_MENU = 0, MUS_DISCO, MUS_WALTZ, MUS_ROCK, MUS_COUNT };

enum {
    SFX_STEP = 0,
    SFX_BREAK,
    SFX_PLACE,
    SFX_UI,
    SFX_CONFIRM,
    SFX_HORN,
    SFX_MISSION,
    SFX_FAIL,
    SFX_DOOR,
    SFX_CRASH,
    SFX_COUNT
};

void synth_init(void);
/* Renders interleaved stereo 16-bit samples (called from the audio thread). */
void synth_render(short *stereo, int frames);

/* Control (main thread). */
void synth_music(int track);          /* -1 stops the music */
int  synth_current_track(void);
int  synth_music_loops(void);         /* how many times the current track has looped */
void synth_sfx(int id);
void synth_engine(int on, float rpm); /* rpm 0..1 */
void synth_volumes(float music, float sfx);
const char *synth_track_name(int track);
int  synth_track_steps(int track, int channel); /* parsed length, for tests */

#endif
