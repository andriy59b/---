#include "voice.h"
#include "synth.h"
#include "texgen.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FS ((float)SYNTH_RATE)
#define MAX_PH 600

enum { PH_SIL = 0, PH_VOWEL, PH_SON, PH_FRIC, PH_VFRIC, PH_STOP, PH_BURST, PH_TRILL };

typedef struct {
    int type;
    float f1, f2, f3, av, an, nfc, pitch;
    int len;
} Phon;

typedef struct { float a0, b1, b2, y1, y2; } Reso;

/* request from the main thread */
static char req_text[640];
static volatile float req_pitch = 120.0f, req_formant = 1.0f;
static volatile int req_id, cur_id, req_stop;
static const short *volatile req_pcm;
static volatile int req_pcm_n, req_pcm_rate, req_pcm_id;
static volatile int busy;
static volatile float level_out;
static volatile float vol = 1.0f;

/* playback state (audio thread) */
static Phon ph[MAX_PH];
static int nph, iph, pos_in_ph;
static float f1, f2, f3, av, an, f0, f0_target, phase, prev_gl, lvl;
static Reso r1, r2, r3, rn;
static int upd;
static unsigned int noise = 0x13579BDFu;
static const short *pcm;
static int pcm_n, pcm_played_id;
static float pcm_pos, pcm_step;

/* ----------------------------------------------------------- main side */

void voice_say(const char *utf8, float pitch_hz, float formant_scale)
{
    strncpy(req_text, utf8, sizeof(req_text) - 1);
    req_text[sizeof(req_text) - 1] = 0;
    req_pitch = pitch_hz;
    req_formant = formant_scale;
    req_pcm = NULL;
    busy = 1;
    req_id++;
}

void voice_play_pcm(const short *mono, int samples, int rate)
{
    req_pcm_n = samples;
    req_pcm_rate = rate;
    req_pcm = mono;
    busy = 1;
    req_pcm_id++;
}

void voice_stop(void) { req_stop = 1; }
int voice_busy(void) { return busy; }
float voice_level(void) { return level_out; }
void voice_volume(float v) { vol = v; }

float voice_duration(const char *s)
{
    float t = 0.25f;
    const char *p = s;
    while (*p) {
        unsigned int c = utf8_next(&p);
        if (c == ' ') t += 0.05f;
        else if (c == ',' || c == 0x2014) t += 0.17f;
        else if (c == '.' || c == '!' || c == '?') t += 0.3f;
        else t += 0.068f;
    }
    return t;
}

/* ------------------------------------------------------------- parsing */

static void reso_set(Reso *r, float f, float bw)
{
    float rr = expf(-PI_F * bw / FS);
    r->b1 = 2.0f * rr * cosf(2.0f * PI_F * f / FS);
    r->b2 = -rr * rr;
    r->a0 = 1.0f - rr;
}

static float reso_run(Reso *r, float x)
{
    float y = r->a0 * x + r->b1 * r->y1 + r->b2 * r->y2;
    r->y2 = r->y1;
    r->y1 = y;
    return y;
}

static void add(int type, float a, float b, float c, float voice, float nz, float fc, float dur, float pitch)
{
    Phon *p;
    if (nph >= MAX_PH) return;
    p = &ph[nph++];
    p->type = type;
    p->f1 = a;
    p->f2 = b;
    p->f3 = c;
    p->av = voice;
    p->an = nz;
    p->nfc = fc;
    p->len = (int)(dur * FS);
    p->pitch = pitch;
}

static unsigned int lower(unsigned int c)
{
    if (c >= 0x410 && c <= 0x42F) return c + 0x20;
    if (c >= 'A' && c <= 'Z') return c + 32;
    if (c == 0x404) return 0x454;
    if (c == 0x406) return 0x456;
    if (c == 0x407) return 0x457;
    if (c == 0x490) return 0x491;
    return c;
}

static float rnd01(void)
{
    noise = noise * 1664525u + 1013904223u;
    return (noise >> 8 & 0xFFFF) / 65535.0f;
}

static void parse(const char *text, float fs)
{
    const char *p = text;
    int n_letters = 0, k = 0, question = 0, exclaim = 0;
    float rate = 0.9f + 0.2f * rnd01();
    nph = 0;
    { /* count letters and find the closing punctuation */
        const char *q = text;
        while (*q) {
            unsigned int c = utf8_next(&q);
            if (c == '?') question = 1;
            if (c == '!') exclaim = 1;
            if (c > 0x400 || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) n_letters++;
        }
        if (n_letters < 1) n_letters = 1;
    }
    add(PH_SIL, 500, 1500, 2500, 0, 0, 0, 0.06f, 1.0f);
    while (*p) {
        unsigned int c = lower(utf8_next(&p));
        float prog = (float)k / n_letters, pm, vd = 0.092f * rate, cd = 0.058f * rate;
        pm = 1.12f - 0.22f * prog + (rnd01() - 0.5f) * 0.1f;
        if (question && prog > 0.7f) pm += (prog - 0.7f) * 1.3f;
        if (exclaim) pm += 0.08f;
        switch (c) {
        /* vowels */
        case 0x430: add(PH_VOWEL, 760 * fs, 1300 * fs, 2600 * fs, 1, 0, 0, vd * 1.1f, pm); k++; break;   /* а */
        case 0x43E: add(PH_VOWEL, 520 * fs, 900 * fs, 2500 * fs, 1, 0, 0, vd, pm); k++; break;          /* о */
        case 0x443: add(PH_VOWEL, 340 * fs, 780 * fs, 2400 * fs, 1, 0, 0, vd, pm); k++; break;          /* у */
        case 0x435: add(PH_VOWEL, 560 * fs, 1800 * fs, 2600 * fs, 1, 0, 0, vd, pm); k++; break;         /* е */
        case 0x438: add(PH_VOWEL, 420 * fs, 1750 * fs, 2500 * fs, 1, 0, 0, vd * 0.9f, pm); k++; break;  /* и */
        case 0x456: add(PH_VOWEL, 300 * fs, 2250 * fs, 3000 * fs, 1, 0, 0, vd * 0.9f, pm); k++; break;  /* і */
        case 0x454: case 0x457: case 0x44E: case 0x44F: /* є ї ю я: j-glide + vowel */
            add(PH_SON, 300 * fs, 2200 * fs, 3000 * fs, 0.8f, 0, 0, cd * 0.6f, pm);
            if (c == 0x454) add(PH_VOWEL, 560 * fs, 1850 * fs, 2600 * fs, 1, 0, 0, vd, pm);
            else if (c == 0x457) add(PH_VOWEL, 300 * fs, 2250 * fs, 3000 * fs, 1, 0, 0, vd, pm);
            else if (c == 0x44E) add(PH_VOWEL, 340 * fs, 900 * fs, 2400 * fs, 1, 0, 0, vd, pm);
            else add(PH_VOWEL, 740 * fs, 1400 * fs, 2600 * fs, 1, 0, 0, vd, pm);
            k++;
            break;
        /* sonorants */
        case 0x43C: case 0x43D: add(PH_SON, 280 * fs, 1100 * fs, 2400 * fs, 0.55f, 0, 0, cd, pm); k++; break; /* м н */
        case 0x43B: add(PH_SON, 380 * fs, 1100 * fs, 2500 * fs, 0.7f, 0, 0, cd, pm); k++; break;               /* л */
        case 0x440: add(PH_TRILL, 450 * fs, 1300 * fs, 2500 * fs, 0.7f, 0.05f, 2000, cd, pm); k++; break;      /* р */
        case 0x432: add(PH_SON, 320 * fs, 1100 * fs, 2400 * fs, 0.6f, 0.05f, 1500, cd, pm); k++; break;        /* в */
        case 0x439: add(PH_SON, 300 * fs, 2200 * fs, 3000 * fs, 0.7f, 0, 0, cd * 0.8f, pm); k++; break;        /* й */
        /* fricatives */
        case 0x441: case 0x446: add(PH_FRIC, 400, 1500, 2500, 0, 0.5f, 6500, cd * 1.3f, pm); k++; break;   /* с ц */
        case 0x448: case 0x449: case 0x447: add(PH_FRIC, 400, 1500, 2500, 0, 0.5f, 3000, cd * 1.3f, pm); k++; break; /* ш щ ч */
        case 0x444: add(PH_FRIC, 400, 1500, 2500, 0, 0.25f, 1800, cd, pm); k++; break;                      /* ф */
        case 0x445: add(PH_FRIC, 400, 1500, 2500, 0, 0.35f, 1400, cd, pm); k++; break;                      /* х */
        case 0x437: add(PH_VFRIC, 300 * fs, 1500 * fs, 2500 * fs, 0.35f, 0.35f, 5500, cd, pm); k++; break;  /* з */
        case 0x436: add(PH_VFRIC, 300 * fs, 1600 * fs, 2500 * fs, 0.35f, 0.35f, 2800, cd, pm); k++; break;  /* ж */
        case 0x433: add(PH_VFRIC, 450 * fs, 1300 * fs, 2500 * fs, 0.45f, 0.2f, 1200, cd, pm); k++; break;   /* г */
        /* stops: closure + burst */
        case 0x43F: case 0x442: case 0x43A:
            add(PH_STOP, 400, 1500, 2500, 0, 0, 0, cd * 0.7f, pm);
            add(PH_BURST, 400, 1500, 2500, 0, 0.6f, c == 0x43F ? 1200 : c == 0x442 ? 4000 : 2200, 0.018f, pm);
            k++;
            break;
        case 0x431: case 0x434: case 0x491:
            add(PH_STOP, 250 * fs, 900 * fs, 2400 * fs, 0.25f, 0, 0, cd * 0.6f, pm);
            add(PH_BURST, 400 * fs, 1500 * fs, 2500 * fs, 0.4f, 0.35f, c == 0x431 ? 1000 : c == 0x434 ? 3500 : 2000, 0.02f, pm);
            k++;
            break;
        case 0x44C: case '\'': case 0x2019: break;                                          /* ь, apostrophe */
        case ' ': add(PH_SIL, 500, 1500, 2500, 0, 0, 0, 0.045f, pm); break;
        case ',': case 0x2014: case '-': case ':': add(PH_SIL, 500, 1500, 2500, 0, 0, 0, 0.17f, pm); break;
        case '.': case '!': case '?': case 0x2026: add(PH_SIL, 500, 1500, 2500, 0, 0, 0, 0.28f, pm); break;
        default:
            if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
                add(PH_VOWEL, 600 * fs, 1500 * fs, 2600 * fs, 0.8f, 0, 0, vd * 0.8f, pm);
                k++;
            }
            break;
        }
    }
    add(PH_SIL, 500, 1500, 2500, 0, 0, 0, 0.12f, 0.8f);
}

/* ----------------------------------------------------------- audio side */

static void start_text(void)
{
    char buf[640];
    memcpy(buf, req_text, sizeof(buf));
    buf[sizeof(buf) - 1] = 0;
    parse(buf, req_formant);
    iph = 0;
    pos_in_ph = 0;
    f0 = f0_target = req_pitch;
    f1 = 500; f2 = 1500; f3 = 2500;
    av = an = 0.0f;
    pcm = NULL;
}

float voice_sample(void)
{
    float out = 0.0f, a;

    if (req_stop) {
        req_stop = 0;
        nph = iph = 0;
        pcm = NULL;
        busy = 0;
    }
    if (req_pcm && req_pcm_id != pcm_played_id) {
        pcm = req_pcm;
        pcm_n = req_pcm_n;
        pcm_pos = 0.0f;
        pcm_step = req_pcm_rate / FS;
        pcm_played_id = req_pcm_id;
        nph = iph = 0;
        cur_id = req_id;
    } else if (req_id != cur_id) {
        cur_id = req_id;
        start_text();
    }

    if (pcm) {
        int i = (int)pcm_pos;
        if (i >= pcm_n) {
            pcm = NULL;
            busy = 0;
        } else {
            out = pcm[i] / 32768.0f;
            pcm_pos += pcm_step;
        }
        a = fabsf(out) * 3.0f;
        lvl += (a - lvl) * (a > lvl ? 0.01f : 0.0015f);
        level_out = clampf(lvl, 0.0f, 1.0f);
        return out * vol;
    }

    if (iph >= nph) {
        if (nph) {
            nph = 0;
            busy = 0;
        }
        lvl *= 0.998f;
        level_out = lvl;
        return 0.0f;
    }

    {
        const Phon *p = &ph[iph];
        float k = 0.0035f, src, gl, nz, v;
        int vowel_like = p->type == PH_VOWEL || p->type == PH_SON || p->type == PH_TRILL || p->type == PH_VFRIC;
        if (vowel_like || p->type == PH_STOP || p->type == PH_BURST) {
            f1 += (p->f1 - f1) * k;
            f2 += (p->f2 - f2) * k;
            f3 += (p->f3 - f3) * k;
        }
        av += (p->av - av) * 0.006f;
        an += (p->an - an) * (p->type == PH_BURST ? 0.05f : 0.01f);
        if (p->type == PH_VOWEL) f0_target = req_pitch * p->pitch;
        f0 += (f0_target - f0) * 0.0015f;
        if (++upd >= 32) {
            upd = 0;
            reso_set(&r1, f1, 90.0f);
            reso_set(&r2, f2, 110.0f);
            reso_set(&r3, f3, 160.0f);
            reso_set(&rn, p->nfc > 0 ? p->nfc : 2000.0f, 1800.0f);
        }
        /* glottal pulse with a little jitter */
        phase += f0 * (1.0f + (rnd01() - 0.5f) * 0.01f) / FS;
        if (phase >= 1.0f) phase -= 1.0f;
        gl = phase < 0.62f ? 0.5f - 0.5f * cosf(2.0f * PI_F * phase / 0.62f) : 0.0f;
        src = (gl - prev_gl) * 18.0f;
        prev_gl = gl;
        nz = rnd01() * 2.0f - 1.0f;
        src += nz * 0.04f;
        if (p->type == PH_TRILL) src *= 0.6f + 0.4f * sinf(2.0f * PI_F * 26.0f * pos_in_ph / FS);
        v = reso_run(&r1, src) * 1.0f + reso_run(&r2, src) * 0.55f + reso_run(&r3, src) * 0.25f;
        out = (v * av * 0.9f + reso_run(&rn, nz) * an * 2.2f) * 0.3f;

        a = av * clampf((f1 - 250.0f) / 550.0f, 0.15f, 1.0f);
        lvl += (a - lvl) * 0.004f;
        level_out = lvl;

        if (++pos_in_ph >= p->len) {
            pos_in_ph = 0;
            iph++;
        }
    }
    return out * vol;
}

/* ---------------------------------------------------------------- WAV */

static unsigned int rd32(const unsigned char *b) { return b[0] | (b[1] << 8) | (b[2] << 16) | ((unsigned int)b[3] << 24); }
static unsigned int rd16(const unsigned char *b) { return b[0] | (b[1] << 8); }

int wav_load(const char *path, short **out, int *samples, int *rate)
{
    FILE *f = fopen(path, "rb");
    unsigned char hdr[12], ch[8];
    int channels = 1, bits = 16, ok = 0;
    *out = NULL;
    if (!f) return 0;
    if (fread(hdr, 1, 12, f) != 12 || memcmp(hdr, "RIFF", 4) || memcmp(hdr + 8, "WAVE", 4)) {
        fclose(f);
        return 0;
    }
    while (fread(ch, 1, 8, f) == 8) {
        unsigned int len = rd32(ch + 4);
        if (!memcmp(ch, "fmt ", 4)) {
            unsigned char fmt[16];
            if (len < 16 || fread(fmt, 1, 16, f) != 16) break;
            channels = (int)rd16(fmt + 2);
            *rate = (int)rd32(fmt + 4);
            bits = (int)rd16(fmt + 14);
            if (len > 16) fseek(f, (long)(len - 16), SEEK_CUR);
        } else if (!memcmp(ch, "data", 4)) {
            int frames, i;
            short *raw;
            if (bits != 16 || channels < 1 || channels > 2) break;
            frames = (int)(len / (unsigned int)(2 * channels));
            raw = (short *)malloc((size_t)frames * channels * 2);
            if (!raw) break;
            frames = (int)fread(raw, (size_t)(2 * channels), (size_t)frames, f);
            if (channels == 2)
                for (i = 0; i < frames; i++) raw[i] = (short)((raw[i * 2] + raw[i * 2 + 1]) / 2);
            *out = raw;
            *samples = frames;
            ok = frames > 0;
            break;
        } else {
            fseek(f, (long)(len + (len & 1)), SEEK_CUR);
        }
    }
    fclose(f);
    return ok;
}
