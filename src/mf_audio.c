#include "mf_audio.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#define MF_HOST_BUFFERS 4
#define MF_HOST_FRAMES  1024

typedef struct {
    HWAVEOUT wave_out;
    HANDLE mix_thread;
    HANDLE stop_event;
    CRITICAL_SECTION lock;
    WAVEHDR headers[MF_HOST_BUFFERS];
    int16_t buffer_pcm[MF_HOST_BUFFERS][MF_HOST_FRAMES * 2];
    volatile bool running;
} mf_win32_audio_context_t;

static DWORD WINAPI host_audio_thread_proc(LPVOID param) {
    mf_audio_t *audio = (mf_audio_t *)param;
    mf_win32_audio_context_t *ctx = (mf_win32_audio_context_t *)audio->host_context;

    while (ctx->running) {
        for (int b = 0; b < MF_HOST_BUFFERS; ++b) {
            if (ctx->headers[b].dwFlags & WHDR_DONE || !(ctx->headers[b].dwFlags & WHDR_PREPARED)) {
                EnterCriticalSection(&ctx->lock);
                mf_audio_mix_samples(audio, ctx->buffer_pcm[b], MF_HOST_FRAMES);
                LeaveCriticalSection(&ctx->lock);

                if (ctx->headers[b].dwFlags & WHDR_PREPARED) {
                    waveOutUnprepareHeader(ctx->wave_out, &ctx->headers[b], sizeof(WAVEHDR));
                }

                ctx->headers[b].lpData = (LPSTR)ctx->buffer_pcm[b];
                ctx->headers[b].dwBufferLength = MF_HOST_FRAMES * 2 * sizeof(int16_t);
                ctx->headers[b].dwFlags = 0;

                waveOutPrepareHeader(ctx->wave_out, &ctx->headers[b], sizeof(WAVEHDR));
                waveOutWrite(ctx->wave_out, &ctx->headers[b], sizeof(WAVEHDR));
            }
        }
        Sleep(10);
    }
    return 0;
}
#endif

void mf_audio_init(mf_audio_t *audio, uint32_t output_sample_rate) {
    if (!audio) return;
    memset(audio, 0, sizeof(mf_audio_t));
    audio->output_sample_rate = (output_sample_rate == 0) ? MF_AUDIO_DEFAULT_RATE : output_sample_rate;
    audio->master_volume = 32767;
    audio->next_voice_id = 1;
}

void mf_audio_shutdown(mf_audio_t *audio) {
    if (!audio) return;
    mf_audio_stop_host_playback(audio);
    mf_audio_stop_all(audio);
}

void mf_audio_set_master_volume(mf_audio_t *audio, int16_t master_volume) {
    if (!audio) return;
    audio->master_volume = (master_volume < 0) ? 0 : master_volume;
}

int mf_audio_play_sound(mf_audio_t *audio,
                        const int16_t *pcm,
                        uint32_t frame_count,
                        uint32_t sample_rate,
                        bool is_stereo,
                        int16_t volume_l,
                        int16_t volume_r,
                        bool loop,
                        uint8_t priority) {
    if (!audio || !pcm || frame_count == 0 || sample_rate == 0) return -1;

    /* 1. Look for inactive voice */
    int slot = -1;
    for (int i = 0; i < MF_AUDIO_MAX_VOICES; ++i) {
        if (!audio->voices[i].active) {
            slot = i;
            break;
        }
    }

    /* 2. If all busy, search for lowest priority non-looping voice to steal */
    if (slot == -1) {
        uint8_t lowest_priority = 255;
        for (int i = 0; i < MF_AUDIO_MAX_VOICES; ++i) {
            if (!audio->voices[i].loop && audio->voices[i].priority < lowest_priority) {
                lowest_priority = audio->voices[i].priority;
                slot = i;
            }
        }
    }

    /* 3. If still nothing, steal voice with lowest priority overall */
    if (slot == -1) {
        uint8_t lowest_priority = 255;
        for (int i = 0; i < MF_AUDIO_MAX_VOICES; ++i) {
            if (audio->voices[i].priority < lowest_priority) {
                lowest_priority = audio->voices[i].priority;
                slot = i;
            }
        }
    }

    if (slot == -1) return -1;

    mf_audio_voice_t *v = &audio->voices[slot];
    v->voice_id = audio->next_voice_id++;
    if (audio->next_voice_id <= 0) audio->next_voice_id = 1;

    v->pcm = pcm;
    v->frame_count = frame_count;
    v->sample_rate = sample_rate;
    v->is_stereo = is_stereo;
    v->volume_l = volume_l;
    v->volume_r = volume_r;
    v->loop = loop;
    v->loop_start = 0;
    v->loop_end = frame_count;
    v->cursor_16_16 = 0;
    v->step_16_16 = (uint32_t)(((uint64_t)sample_rate << 16) / audio->output_sample_rate);
    v->priority = priority;
    v->active = true;

    return v->voice_id;
}

bool mf_audio_stop_voice(mf_audio_t *audio, int voice_id) {
    if (!audio || voice_id <= 0) return false;
    for (int i = 0; i < MF_AUDIO_MAX_VOICES; ++i) {
        if (audio->voices[i].active && audio->voices[i].voice_id == voice_id) {
            audio->voices[i].active = false;
            return true;
        }
    }
    return false;
}

void mf_audio_stop_all(mf_audio_t *audio) {
    if (!audio) return;
    for (int i = 0; i < MF_AUDIO_MAX_VOICES; ++i) {
        audio->voices[i].active = false;
    }
}

bool mf_audio_is_voice_playing(const mf_audio_t *audio, int voice_id) {
    if (!audio || voice_id <= 0) return false;
    for (int i = 0; i < MF_AUDIO_MAX_VOICES; ++i) {
        if (audio->voices[i].active && audio->voices[i].voice_id == voice_id) {
            return true;
        }
    }
    return false;
}

int mf_audio_active_voice_count(const mf_audio_t *audio) {
    if (!audio) return 0;
    int count = 0;
    for (int i = 0; i < MF_AUDIO_MAX_VOICES; ++i) {
        if (audio->voices[i].active) {
            count++;
        }
    }
    return count;
}

bool mf_audio_set_voice_volume(mf_audio_t *audio, int voice_id, int16_t vol_l, int16_t vol_r) {
    if (!audio || voice_id <= 0) return false;
    for (int i = 0; i < MF_AUDIO_MAX_VOICES; ++i) {
        if (audio->voices[i].active && audio->voices[i].voice_id == voice_id) {
            audio->voices[i].volume_l = vol_l;
            audio->voices[i].volume_r = vol_r;
            return true;
        }
    }
    return false;
}

bool mf_audio_set_voice_pitch(mf_audio_t *audio, int voice_id, float pitch_multiplier) {
    if (!audio || voice_id <= 0 || pitch_multiplier <= 0.0f) return false;
    for (int i = 0; i < MF_AUDIO_MAX_VOICES; ++i) {
        if (audio->voices[i].active && audio->voices[i].voice_id == voice_id) {
            uint64_t base_step = ((uint64_t)audio->voices[i].sample_rate << 16) / audio->output_sample_rate;
            audio->voices[i].step_16_16 = (uint32_t)((double)base_step * (double)pitch_multiplier);
            return true;
        }
    }
    return false;
}

bool mf_audio_set_voice_loop(mf_audio_t *audio, int voice_id, bool loop, uint32_t loop_start, uint32_t loop_end) {
    if (!audio || voice_id <= 0) return false;
    for (int i = 0; i < MF_AUDIO_MAX_VOICES; ++i) {
        if (audio->voices[i].active && audio->voices[i].voice_id == voice_id) {
            audio->voices[i].loop = loop;
            audio->voices[i].loop_start = loop_start;
            audio->voices[i].loop_end = (loop_end == 0) ? audio->voices[i].frame_count : loop_end;
            return true;
        }
    }
    return false;
}

void mf_audio_mix_samples(mf_audio_t *audio, int16_t *out_interleaved_stereo, size_t frames_to_mix) {
    if (!audio || !out_interleaved_stereo || frames_to_mix == 0) return;

    /* Temporary 32-bit mixing accumulation buffers for precision */
    int32_t accum_l[MF_AUDIO_MIX_BUFFER_SIZE];
    int32_t accum_r[MF_AUDIO_MIX_BUFFER_SIZE];

    size_t frames_remaining = frames_to_mix;
    int16_t *dest_ptr = out_interleaved_stereo;

    while (frames_remaining > 0) {
        size_t chunk = (frames_remaining > MF_AUDIO_MIX_BUFFER_SIZE) ? MF_AUDIO_MIX_BUFFER_SIZE : frames_remaining;
        memset(accum_l, 0, chunk * sizeof(int32_t));
        memset(accum_r, 0, chunk * sizeof(int32_t));

        /* Additively mix each active voice */
        for (int v_idx = 0; v_idx < MF_AUDIO_MAX_VOICES; ++v_idx) {
            mf_audio_voice_t *v = &audio->voices[v_idx];
            if (!v->active) continue;

            for (size_t f = 0; f < chunk; ++f) {
                uint32_t frame_idx = v->cursor_16_16 >> 16;
                uint32_t frac = v->cursor_16_16 & 0xFFFF;

                if (!v->loop) {
                    if (frame_idx >= v->frame_count) {
                        v->active = false;
                        break;
                    }
                } else {
                    if (frame_idx >= v->loop_end) {
                        uint32_t loop_len = (v->loop_end > v->loop_start) ? (v->loop_end - v->loop_start) : 1;
                        frame_idx = v->loop_start + ((frame_idx - v->loop_start) % loop_len);
                        v->cursor_16_16 = (frame_idx << 16) | frac;
                    }
                }

                int32_t s0_l = 0, s0_r = 0;
                int32_t s1_l = 0, s1_r = 0;

                if (v->is_stereo) {
                    s0_l = v->pcm[frame_idx * 2 + 0];
                    s0_r = v->pcm[frame_idx * 2 + 1];
                    uint32_t next_idx = frame_idx + 1;
                    if (next_idx < v->frame_count) {
                        s1_l = v->pcm[next_idx * 2 + 0];
                        s1_r = v->pcm[next_idx * 2 + 1];
                    } else {
                        s1_l = v->loop ? v->pcm[v->loop_start * 2 + 0] : s0_l;
                        s1_r = v->loop ? v->pcm[v->loop_start * 2 + 1] : s0_r;
                    }
                } else {
                    s0_l = s0_r = v->pcm[frame_idx];
                    uint32_t next_idx = frame_idx + 1;
                    if (next_idx < v->frame_count) {
                        s1_l = s1_r = v->pcm[next_idx];
                    } else {
                        s1_l = s1_r = v->loop ? v->pcm[v->loop_start] : s0_l;
                    }
                }

                /* Linear interpolation */
                int32_t interp_l = s0_l + (((s1_l - s0_l) * (int32_t)frac) >> 16);
                int32_t interp_r = s0_r + (((s1_r - s0_r) * (int32_t)frac) >> 16);

                accum_l[f] += (interp_l * v->volume_l) >> 15;
                accum_r[f] += (interp_r * v->volume_r) >> 15;

                v->cursor_16_16 += v->step_16_16;
            }
        }

        /* Master volume scaling and clipping */
        for (size_t f = 0; f < chunk; ++f) {
            int32_t final_l = (accum_l[f] * audio->master_volume) >> 15;
            int32_t final_r = (accum_r[f] * audio->master_volume) >> 15;

            dest_ptr[f * 2 + 0] = mf_clip_s16(final_l);
            dest_ptr[f * 2 + 1] = mf_clip_s16(final_r);
        }

        dest_ptr += chunk * 2;
        frames_remaining -= chunk;
    }
}

bool mf_audio_start_host_playback(mf_audio_t *audio) {
#if defined(_WIN32)
    if (!audio || audio->host_playback_active) return true;

    mf_win32_audio_context_t *ctx = (mf_win32_audio_context_t *)calloc(1, sizeof(mf_win32_audio_context_t));
    if (!ctx) return false;

    InitializeCriticalSection(&ctx->lock);

    WAVEFORMATEX wfx;
    memset(&wfx, 0, sizeof(wfx));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 2;
    wfx.nSamplesPerSec = audio->output_sample_rate;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = wfx.nChannels * (wfx.wBitsPerSample / 8);
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    MMRESULT res = waveOutOpen(&ctx->wave_out, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
    if (res != MMSYSERR_NOERROR) {
        DeleteCriticalSection(&ctx->lock);
        free(ctx);
        return false;
    }

    ctx->running = true;
    audio->host_context = ctx;
    audio->host_playback_active = true;

    ctx->mix_thread = CreateThread(NULL, 0, host_audio_thread_proc, audio, 0, NULL);
    return true;
#else
    (void)audio;
    return false;
#endif
}

void mf_audio_stop_host_playback(mf_audio_t *audio) {
#if defined(_WIN32)
    if (!audio || !audio->host_playback_active || !audio->host_context) return;
    mf_win32_audio_context_t *ctx = (mf_win32_audio_context_t *)audio->host_context;

    ctx->running = false;
    if (ctx->mix_thread) {
        WaitForSingleObject(ctx->mix_thread, 2000);
        CloseHandle(ctx->mix_thread);
        ctx->mix_thread = NULL;
    }

    if (ctx->wave_out) {
        waveOutReset(ctx->wave_out);
        for (int b = 0; b < MF_HOST_BUFFERS; ++b) {
            if (ctx->headers[b].dwFlags & WHDR_PREPARED) {
                waveOutUnprepareHeader(ctx->wave_out, &ctx->headers[b], sizeof(WAVEHDR));
            }
        }
        waveOutClose(ctx->wave_out);
        ctx->wave_out = NULL;
    }

    DeleteCriticalSection(&ctx->lock);
    free(ctx);
    audio->host_context = NULL;
    audio->host_playback_active = false;
#else
    (void)audio;
#endif
}

bool mf_audio_export_wav(const int16_t *interleaved_stereo, size_t frame_count,
                         uint32_t sample_rate, const char *filepath) {
    if (!interleaved_stereo || frame_count == 0 || !filepath) return false;

    FILE *f = fopen(filepath, "wb");
    if (!f) return false;

    uint32_t data_bytes = (uint32_t)(frame_count * 2 * sizeof(int16_t));
    uint32_t file_bytes = 36 + data_bytes;
    uint16_t channels = 2;
    uint16_t bits = 16;
    uint32_t byte_rate = sample_rate * channels * (bits / 8);
    uint16_t block_align = channels * (bits / 8);

    uint8_t header[44] = {
        'R', 'I', 'F', 'F',
        (uint8_t)(file_bytes), (uint8_t)(file_bytes >> 8), (uint8_t)(file_bytes >> 16), (uint8_t)(file_bytes >> 24),
        'W', 'A', 'V', 'E',
        'f', 'm', 't', ' ',
        16, 0, 0, 0, /* Subchunk1Size (16 for PCM) */
        1, 0,        /* AudioFormat (1 = PCM) */
        (uint8_t)channels, 0,
        (uint8_t)(sample_rate), (uint8_t)(sample_rate >> 8), (uint8_t)(sample_rate >> 16), (uint8_t)(sample_rate >> 24),
        (uint8_t)(byte_rate), (uint8_t)(byte_rate >> 8), (uint8_t)(byte_rate >> 16), (uint8_t)(byte_rate >> 24),
        (uint8_t)block_align, 0,
        (uint8_t)bits, 0,
        'd', 'a', 't', 'a',
        (uint8_t)(data_bytes), (uint8_t)(data_bytes >> 8), (uint8_t)(data_bytes >> 16), (uint8_t)(data_bytes >> 24)
    };

    if (fwrite(header, 1, 44, f) != 44) {
        fclose(f);
        return false;
    }

    if (fwrite(interleaved_stereo, sizeof(int16_t) * 2, frame_count, f) != frame_count) {
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}

bool mf_audio_self_test(void) {
    mf_audio_t audio;
    mf_audio_init(&audio, 44100);

    /* Generate 3 synthetic audio buffers:
     * 1. 440 Hz whistle sine wave (mono, 4410 frames = 100ms)
     * 2. 220 Hz crowd drone sine wave (mono, 4410 frames = 100ms, looping)
     * 3. 880 Hz helmet tackle burst (mono, 2205 frames = 50ms) */
    const size_t tone1_len = 4410;
    const size_t tone2_len = 4410;
    const size_t tone3_len = 2205;

    int16_t *tone1 = (int16_t *)malloc(tone1_len * sizeof(int16_t));
    int16_t *tone2 = (int16_t *)malloc(tone2_len * sizeof(int16_t));
    int16_t *tone3 = (int16_t *)malloc(tone3_len * sizeof(int16_t));

    if (!tone1 || !tone2 || !tone3) {
        free(tone1); free(tone2); free(tone3);
        return false;
    }

    for (size_t i = 0; i < tone1_len; ++i) {
        double t = (double)i / 44100.0;
        tone1[i] = (int16_t)(sin(2.0 * 3.1415926535 * 440.0 * t) * 10000.0);
    }
    for (size_t i = 0; i < tone2_len; ++i) {
        double t = (double)i / 44100.0;
        tone2[i] = (int16_t)(sin(2.0 * 3.1415926535 * 220.0 * t) * 8000.0);
    }
    for (size_t i = 0; i < tone3_len; ++i) {
        double t = (double)i / 44100.0;
        tone3[i] = (int16_t)(sin(2.0 * 3.1415926535 * 880.0 * t) * 12000.0);
    }

    /* Start all 3 sounds simultaneously */
    int v1 = mf_audio_play_sound(&audio, tone1, (uint32_t)tone1_len, 44100, false, 20000, 20000, false, 1);
    int v2 = mf_audio_play_sound(&audio, tone2, (uint32_t)tone2_len, 44100, false, 15000, 15000, true, 2);
    int v3 = mf_audio_play_sound(&audio, tone3, (uint32_t)tone3_len, 44100, false, 25000, 25000, false, 3);

    if (v1 <= 0 || v2 <= 0 || v3 <= 0) {
        free(tone1); free(tone2); free(tone3);
        return false;
    }

    /* Verify all 3 voices are reported active */
    if (mf_audio_active_voice_count(&audio) != 3) {
        free(tone1); free(tone2); free(tone3);
        return false;
    }

    /* Mix 1024 frames of multi-sound output */
    int16_t mixed_output[1024 * 2];
    memset(mixed_output, 0, sizeof(mixed_output));
    mf_audio_mix_samples(&audio, mixed_output, 1024);

    /* Verify non-zero output and multi-channel additive mixing */
    int64_t sum_energy = 0;
    for (int i = 0; i < 1024 * 2; ++i) {
        sum_energy += abs(mixed_output[i]);
    }
    if (sum_energy == 0) {
        free(tone1); free(tone2); free(tone3);
        return false;
    }

    /* Stop voice 1, confirm voice 2 and 3 remain active */
    mf_audio_stop_voice(&audio, v1);
    if (mf_audio_is_voice_playing(&audio, v1)) {
        free(tone1); free(tone2); free(tone3);
        return false;
    }
    if (!mf_audio_is_voice_playing(&audio, v2) || !mf_audio_is_voice_playing(&audio, v3)) {
        free(tone1); free(tone2); free(tone3);
        return false;
    }
    if (mf_audio_active_voice_count(&audio) != 2) {
        free(tone1); free(tone2); free(tone3);
        return false;
    }

    /* Stop all voices */
    mf_audio_stop_all(&audio);
    if (mf_audio_active_voice_count(&audio) != 0) {
        free(tone1); free(tone2); free(tone3);
        return false;
    }

    free(tone1);
    free(tone2);
    free(tone3);
    mf_audio_shutdown(&audio);
    return true;
}
