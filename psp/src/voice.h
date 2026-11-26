/* Character voices: a small formant speech synthesizer that turns Ukrainian
 * text into speech-like sound (each character has its own pitch and timbre),
 * or plays recorded WAV lines when they exist. Provides a level for lip sync. */
#ifndef UC_VOICE_H
#define UC_VOICE_H

/* Main thread */
void voice_say(const char *utf8, float pitch_hz, float formant_scale);
void voice_play_pcm(const short *mono, int samples, int rate); /* buffer must outlive playback */
void voice_stop(void);
int  voice_busy(void);
float voice_level(void);          /* 0..1, mouth openness */
void voice_volume(float v);

/* Audio thread (called by the synthesizer for each output sample) */
float voice_sample(void);

/* Loads a 16-bit PCM WAV file (mono or stereo, any rate) as mono. */
int wav_load(const char *path, short **out, int *samples, int *rate);

/* Estimated speaking time for a line (seconds). */
float voice_duration(const char *utf8);

#endif
