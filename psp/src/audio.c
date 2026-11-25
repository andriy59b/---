/* PSP audio output: a high-priority thread feeding the synthesizer to sceAudio. */
#include <pspkernel.h>
#include <pspaudio.h>

#include "audio.h"
#include "synth.h"

#define FRAMES 1024

static volatile int audio_running;
static SceUID audio_th = -1;

static int audio_thread(SceSize args, void *argp)
{
    static short buf[2][FRAMES * 2];
    int cur = 0, ch;
    (void)args; (void)argp;
    ch = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, FRAMES, PSP_AUDIO_FORMAT_STEREO);
    if (ch < 0) return 0;
    while (audio_running) {
        synth_render(buf[cur], FRAMES);
        sceAudioOutputPannedBlocking(ch, PSP_AUDIO_VOLUME_MAX, PSP_AUDIO_VOLUME_MAX, buf[cur]);
        cur ^= 1;
    }
    sceAudioChRelease(ch);
    return 0;
}

void audio_start(void)
{
    synth_init();
    audio_running = 1;
    audio_th = sceKernelCreateThread("uc_audio", audio_thread, 0x12, 0x10000, PSP_THREAD_ATTR_USER, NULL);
    if (audio_th >= 0) sceKernelStartThread(audio_th, 0, NULL);
}

void audio_stop(void)
{
    SceUInt timeout = 500000;
    if (audio_th < 0) return;
    audio_running = 0;
    sceKernelWaitThreadEnd(audio_th, &timeout);
    audio_th = -1;
}
