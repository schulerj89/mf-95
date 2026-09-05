#ifndef MF_AUDIO_H
#define MF_AUDIO_H

#include "mf_types.h"
#include <stddef.h>

#define MF_AUDIO_MAX_VOICES      16
#define MF_AUDIO_DEFAULT_RATE    44100
#define MF_AUDIO_MIX_BUFFER_SIZE 1024

typedef struct {
    int voice_id;
    bool active;
    bool loop;
    bool is_stereo;
    const int16_t *pcm;
    uint32_t frame_count;     /* Total frames (1 frame = 1 sample mono, 2 samples stereo) */
    uint32_t sample_rate;
    uint32_t loop_start;
    uint32_t loop_end;
    uint32_t cursor_16_16;    /* Fixed-point 16.16 frame index */
    uint32_t step_16_16;      /* Fixed-point 16.16 playback speed step */
    int16_t volume_l;         /* 0..32767 */
    int16_t volume_r;         /* 0..32767 */
    uint8_t priority;         /* Higher priority voices are protected from stealing */
} mf_audio_voice_t;

typedef struct mf_audio {
    uint32_t output_sample_rate;
    int16_t master_volume;    /* 0..32767 */
    mf_audio_voice_t voices[MF_AUDIO_MAX_VOICES];
    int next_voice_id;
    bool host_playback_active;
    void *host_context;       /* Platform audio device / thread context */
} mf_audio_t;

/* Core Audio Lifecycle */
void mf_audio_init(mf_audio_t *audio, uint32_t output_sample_rate);
void mf_audio_shutdown(mf_audio_t *audio);
void mf_audio_set_master_volume(mf_audio_t *audio, int16_t master_volume);

/* Multi-Sound Playback Controls */
/* Plays a sound simultaneously across available voices.
 * Returns allocated voice_id (>= 1) or -1 if no voice could be assigned. */
int mf_audio_play_sound(mf_audio_t *audio,
                        const int16_t *pcm,
                        uint32_t frame_count,
                        uint32_t sample_rate,
                        bool is_stereo,
                        int16_t volume_l,
                        int16_t volume_r,
                        bool loop,
                        uint8_t priority);

bool mf_audio_stop_voice(mf_audio_t *audio, int voice_id);
void mf_audio_stop_all(mf_audio_t *audio);
bool mf_audio_is_voice_playing(const mf_audio_t *audio, int voice_id);
int  mf_audio_active_voice_count(const mf_audio_t *audio);

/* Voice Property Adjustments */
bool mf_audio_set_voice_volume(mf_audio_t *audio, int voice_id, int16_t vol_l, int16_t vol_r);
bool mf_audio_set_voice_pitch(mf_audio_t *audio, int voice_id, float pitch_multiplier);
bool mf_audio_set_voice_loop(mf_audio_t *audio, int voice_id, bool loop, uint32_t loop_start, uint32_t loop_end);

/* Software Mixer
 * Mixes all active voices simultaneously into interleaved 16-bit stereo buffer (L, R, L, R...). */
void mf_audio_mix_samples(mf_audio_t *audio, int16_t *out_interleaved_stereo, size_t frames_to_mix);

/* Host Platform Playback (Win32 waveOut) */
bool mf_audio_start_host_playback(mf_audio_t *audio);
void mf_audio_stop_host_playback(mf_audio_t *audio);

/* Verification & Export */
bool mf_audio_export_wav(const int16_t *interleaved_stereo, size_t frame_count,
                         uint32_t sample_rate, const char *filepath);
bool mf_audio_self_test(void);

#endif /* MF_AUDIO_H */
