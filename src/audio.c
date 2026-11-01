#include "audio.h"
#include "assets.h"

#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RATE 22050
#define MAX_SAMPLES (RATE * 2)
#define NVOICES 10

typedef struct { int16_t *d; int n; } Buf;
typedef struct { const int16_t *d; int n; float pos, pitch; bool on; } Voice;

static const char *const SFX_NAMES[SFX_COUNT] = {
    "shoot", "hit", "kill", "hurt", "special", "wave", "select",
    "beep", "chime", "panic", "pickup", "warn", "victory"
};

static SDL_AudioDeviceID dev;
static Buf bufs[SFX_COUNT];
static Voice voices[NVOICES];
static float mus_vol, mus_target;
static bool muted;
static float master = 0.36f;      /* VOL_DEFAULT squared; changed through audio_set_volume() */
static uint32_t noise_state = 12345u;

/* ---- synthesis helpers ---- */
static float noise(void)
{
    noise_state ^= noise_state << 13; noise_state ^= noise_state >> 17; noise_state ^= noise_state << 5;
    return (float)(noise_state & 0xFFFF) / 32768.0f - 1.0f;
}

typedef struct { int16_t *d; int n; } Cursor;

/* wave: 0 square, 1 noise, 2 triangle */
static void seg(Cursor *c, int wave, float f0, float f1, float dur, float vol)
{
    int n = (int)(dur * RATE);
    float phase = 0;
    for (int i = 0; i < n && c->n < MAX_SAMPLES; i++) {
        float t = (float)i / (float)(n > 1 ? n - 1 : 1);
        float f = f0 + (f1 - f0) * t;
        phase += f / RATE;
        phase -= floorf(phase);
        float s;
        if (wave == 0) s = phase < 0.5f ? 1.0f : -1.0f;
        else if (wave == 1) s = noise();
        else s = 4.0f * fabsf(phase - 0.5f) - 1.0f;
        float env = 1.0f - t;
        if (i < 40) env *= (float)i / 40.0f;
        c->d[c->n++] = (int16_t)(s * env * vol * 12000.0f);
    }
}

static void gap(Cursor *c, float dur) { seg(c, 0, 0, 0, dur, 0); }

static void synth_all(void)
{
    for (int s = 0; s < SFX_COUNT; s++) {
        bufs[s].d = calloc(MAX_SAMPLES, sizeof(int16_t));
        if (!bufs[s].d) continue;
        Cursor c = { bufs[s].d, 0 };
        switch (s) {
        case SFX_SHOOT:   seg(&c, 0, 900, 380, 0.07f, 0.5f); break;
        case SFX_HIT:     seg(&c, 1, 0, 0, 0.05f, 0.6f); break;
        case SFX_KILL:    seg(&c, 1, 0, 0, 0.08f, 0.6f); seg(&c, 0, 420, 90, 0.18f, 0.5f); break;
        case SFX_HURT:    seg(&c, 0, 240, 70, 0.35f, 0.7f); seg(&c, 1, 0, 0, 0.10f, 0.4f); break;
        case SFX_SPECIAL: seg(&c, 2, 250, 1300, 0.30f, 0.9f); break;
        case SFX_WAVE:    seg(&c, 0, 523, 523, 0.08f, 0.5f); seg(&c, 0, 659, 659, 0.08f, 0.5f);
                          seg(&c, 0, 784, 784, 0.14f, 0.5f); break;
        case SFX_SELECT:  seg(&c, 0, 660, 660, 0.04f, 0.4f); break;
        case SFX_BEEP:    seg(&c, 0, 1000, 1000, 0.05f, 0.4f); break;
        case SFX_CHIME:   seg(&c, 2, 523, 523, 0.12f, 0.8f); seg(&c, 2, 659, 659, 0.12f, 0.8f);
                          seg(&c, 2, 784, 784, 0.12f, 0.8f); seg(&c, 2, 1047, 1047, 0.30f, 0.8f); break;
        case SFX_PANIC:   seg(&c, 1, 0, 0, 0.25f, 0.7f); seg(&c, 0, 110, 55, 0.5f, 0.7f); break;
        case SFX_PICKUP:  seg(&c, 0, 600, 600, 0.06f, 0.5f); seg(&c, 0, 900, 900, 0.06f, 0.5f);
                          seg(&c, 0, 1200, 1200, 0.10f, 0.5f); break;
        case SFX_WARN:    seg(&c, 0, 440, 440, 0.07f, 0.45f); gap(&c, 0.05f); seg(&c, 0, 440, 440, 0.07f, 0.45f); break;
        case SFX_VICTORY: seg(&c, 2, 392, 392, 0.10f, 0.8f); seg(&c, 2, 523, 523, 0.10f, 0.8f);
                          seg(&c, 2, 659, 659, 0.10f, 0.8f); seg(&c, 2, 784, 784, 0.30f, 0.8f); break;
        default: break;
        }
        bufs[s].n = c.n;
    }
}

/* Optional <assets>/sfx/<name>.wav overrides. Missing/invalid files are ignored. */
static void load_overrides(void)
{
    for (int s = 0; s < SFX_COUNT; s++) {
        char rel[64], path[1024];
        snprintf(rel, sizeof rel, "sfx/%s.wav", SFX_NAMES[s]);
        if (!assets_find(rel, path, sizeof path)) continue;
        SDL_AudioSpec spec; Uint8 *buf = NULL; Uint32 len = 0;
        if (!SDL_LoadWAV(path, &spec, &buf, &len)) {
            fprintf(stderr, "init-wars: cannot load %s (%s); using built-in sound\n", path, SDL_GetError());
            continue;
        }
        SDL_AudioCVT cvt;
        if (SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq, AUDIO_S16SYS, 1, RATE) < 0 ||
            len == 0 || len > (Uint32)(MAX_SAMPLES * 2)) {
            fprintf(stderr, "init-wars: unsupported WAV %s; using built-in sound\n", path);
            SDL_FreeWAV(buf);
            continue;
        }
        cvt.len = (int)len;
        cvt.buf = malloc((size_t)cvt.len * (size_t)(cvt.len_mult > 0 ? cvt.len_mult : 1));
        if (!cvt.buf) { SDL_FreeWAV(buf); continue; }
        memcpy(cvt.buf, buf, len);
        SDL_FreeWAV(buf);
        if (SDL_ConvertAudio(&cvt) == 0 && cvt.len_cvt > 0 && cvt.len_cvt <= MAX_SAMPLES * 2) {
            memset(bufs[s].d, 0, MAX_SAMPLES * sizeof(int16_t));
            memcpy(bufs[s].d, cvt.buf, (size_t)cvt.len_cvt);
            bufs[s].n = cvt.len_cvt / 2;
        }
        free(cvt.buf);
    }
}

/* ---- music: a short minor-key loop (original) ---- */
static const float BASS[8] = { 110.0f, 110.0f, 130.8f, 130.8f, 98.0f, 98.0f, 123.5f, 123.5f };
static const float LEAD[16] = { 440, 0, 523, 0, 659, 0, 523, 0, 392, 0, 494, 0, 587, 0, 494, 0 };

static void mix(void *ud, Uint8 *stream, int len)
{
    (void)ud;
    int16_t *out = (int16_t *)stream;
    int n = len / 2;
    static int step_samples, step;
    static float bph, lph;
    const int step_len = RATE * 60 / 150 / 2;
    for (int i = 0; i < n; i++) {
        float s = 0;
        for (int v = 0; v < NVOICES; v++) {
            Voice *vo = &voices[v];
            if (!vo->on) continue;
            int p = (int)vo->pos;
            if (p >= vo->n) { vo->on = false; continue; }
            s += vo->d[p];
            vo->pos += vo->pitch;
        }
        mus_vol += (mus_target - mus_vol) * 0.0004f;
        if (mus_vol > 0.001f && !muted) {
            if (++step_samples >= step_len) { step_samples = 0; step = (step + 1) & 15; }
            float bf = BASS[step >> 1], lf = LEAD[step];
            bph += bf / RATE; bph -= floorf(bph);
            float m = (bph < 0.5f ? 1.0f : -1.0f) * 0.5f;
            if (lf > 0) {
                lph += lf / RATE; lph -= floorf(lph);
                float env = 1.0f - (float)step_samples / (float)step_len;
                m += (lph < 0.25f ? 1.0f : -1.0f) * 0.4f * env;
            }
            s += m * 2200.0f * mus_vol;
        }
        s *= master;
        if (muted) s = 0;
        if (s > 32000) s = 32000;
        if (s < -32000) s = -32000;
        out[i] = (int16_t)s;
    }
}

bool audio_init(void)
{
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "init-wars: audio unavailable (%s); running silent\n", SDL_GetError());
        return false;
    }
    synth_all();
    load_overrides();
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = RATE; want.format = AUDIO_S16SYS; want.channels = 1; want.samples = 1024;
    want.callback = mix;
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!dev) {
        fprintf(stderr, "init-wars: cannot open audio device (%s); running silent\n", SDL_GetError());
        return false;
    }
    if (have.freq != RATE || have.format != AUDIO_S16SYS || have.channels != 1) {
        fprintf(stderr, "init-wars: unexpected audio format; running silent\n");
        SDL_CloseAudioDevice(dev);
        dev = 0;
        return false;
    }
    SDL_PauseAudioDevice(dev, 0);
    return true;
}

bool audio_available(void) { return dev != 0; }

void audio_shutdown(void)
{
    if (dev) { SDL_CloseAudioDevice(dev); dev = 0; }
    for (int i = 0; i < SFX_COUNT; i++) { free(bufs[i].d); bufs[i].d = NULL; bufs[i].n = 0; }
}

void audio_play(Sfx s, float pitch)
{
    if (!dev || s < 0 || s >= SFX_COUNT || !bufs[s].d || bufs[s].n == 0) return;
    SDL_LockAudioDevice(dev);
    Voice *slot = &voices[0];
    for (int i = 0; i < NVOICES; i++) {
        if (!voices[i].on) { slot = &voices[i]; break; }
        if (voices[i].pos > slot->pos) slot = &voices[i];
    }
    slot->d = bufs[s].d; slot->n = bufs[s].n; slot->pos = 0;
    slot->pitch = pitch > 0.1f ? pitch : 1.0f; slot->on = true;
    SDL_UnlockAudioDevice(dev);
}

void audio_set_music(float volume)
{
    if (!dev) return;
    SDL_LockAudioDevice(dev);
    mus_target = volume < 0 ? 0 : (volume > 1 ? 1 : volume);
    SDL_UnlockAudioDevice(dev);
}

void audio_set_mute(bool m)
{
    if (!dev) return;
    SDL_LockAudioDevice(dev);
    muted = m;
    SDL_UnlockAudioDevice(dev);
}

void audio_set_volume(int step)
{
    if (!dev) return;
    SDL_LockAudioDevice(dev);
    master = audio_volume_gain(step);
    SDL_UnlockAudioDevice(dev);
}
