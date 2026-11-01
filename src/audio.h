/* audio.h - tiny synthesised sound effects and chiptune loop on SDL's audio device. */
#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>

typedef enum {
    SFX_SHOOT, SFX_HIT, SFX_KILL, SFX_HURT, SFX_SPECIAL, SFX_WAVE, SFX_SELECT,
    SFX_BEEP, SFX_CHIME, SFX_PANIC, SFX_PICKUP, SFX_WARN, SFX_VICTORY, SFX_COUNT
} Sfx;

/* Returns false if no audio device could be opened; the game then runs silently. */
bool audio_init(void);
void audio_shutdown(void);
void audio_play(Sfx s, float pitch);
void audio_set_music(float volume);   /* target volume 0..1, faded smoothly */
void audio_set_mute(bool mute);
#define VOL_MAX     10
#define VOL_DEFAULT 6
/* Master volume as a step 0..VOL_MAX. Gain is squared so each step sounds even; VOL_MAX is full level. */
void audio_set_volume(int step);
static inline float audio_volume_gain(int step)
{
    if (step < 0) step = 0;
    if (step > VOL_MAX) step = VOL_MAX;
    float l = (float)step / (float)VOL_MAX;
    return l * l;
}
bool audio_available(void);

#endif
