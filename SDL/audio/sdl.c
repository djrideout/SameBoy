#include "audio.h"
#include <SDL.h>
#include <SDL_mixer.h>

#ifndef _WIN32
#define AUDIO_FREQUENCY 96000
#include <unistd.h>
#else
#include <Windows.h>
/* Windows (well, at least my VM) can't handle 96KHz sound well :( */

/* felsqualle says: For SDL 2.0.6+ using the WASAPI driver, the highest freq.
 we can get is 48000. 96000 also works, but always has some faint crackling in
 the audio, no matter how high or low I set the buffer length...
 Not quite satisfied with that solution, because acc. to SDL2 docs,
 96k + WASAPI *should* work. */

#define AUDIO_FREQUENCY 48000
#endif

/* Compatibility with older SDL versions */
#ifndef SDL_AUDIO_ALLOW_SAMPLES_CHANGE
#define SDL_AUDIO_ALLOW_SAMPLES_CHANGE 0
#endif

static SDL_AudioDeviceID device_id;
static SDL_AudioSpec want_aspec, have_aspec;
static Mix_Music *music;

#define AUDIO_BUFFER_SIZE 512
static unsigned buffer_pos = 0;
static GB_sample_t audio_buffer[AUDIO_BUFFER_SIZE];

static bool _audio_is_playing(void)
{
    return SDL_GetAudioDeviceStatus(device_id) == SDL_AUDIO_PLAYING;
}

static void _audio_clear_queue(void)
{
    SDL_ClearQueuedAudio(device_id);
}

static void _audio_set_paused(bool paused)
{
    _audio_clear_queue();
    SDL_PauseAudioDevice(device_id, paused);
}

static unsigned _audio_get_frequency(void)
{
    return have_aspec.freq;
}

static size_t _audio_get_queue_length(void)
{
    return SDL_GetQueuedAudioSize(device_id) / sizeof(GB_sample_t);
}

static void _audio_queue_sample(GB_sample_t *sample)
{
    audio_buffer[buffer_pos++] = *sample;

    if (buffer_pos == AUDIO_BUFFER_SIZE) {
        buffer_pos = 0;
        SDL_QueueAudio(device_id, (const void *)audio_buffer, sizeof(audio_buffer));
    }
}

static bool _audio_init(void)
{
    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        printf("Failed to initialize SDL audio: %s", SDL_GetError());
        return false;
    }

    int result = 0;
    int flags = MIX_INIT_MP3;
    if (flags != (result = Mix_Init(flags))) {
        printf("Could not initialize mixer (result: %d).\n", result);
        printf("Mix_Init: %s\n", Mix_GetError());
        return false;
    }

    /* Configure Audio */
    memset(&want_aspec, 0, sizeof(want_aspec));
    want_aspec.freq = AUDIO_FREQUENCY;
    want_aspec.format = AUDIO_S16SYS;
    want_aspec.channels = 2;
    want_aspec.samples = 512;
    
    SDL_version _sdl_version;
    SDL_GetVersion(&_sdl_version);
    unsigned sdl_version = _sdl_version.major * 1000 + _sdl_version.minor * 100 + _sdl_version.patch;
    
#ifndef _WIN32
    /* SDL 2.0.5 on macOS and Linux introduced a bug where certain combinations of buffer lengths and frequencies
     fail to produce audio correctly. */
    if (sdl_version >= 2005) {
        want_aspec.samples = 2048;
    }
#else
    if (sdl_version < 2006) {
        /* Since WASAPI audio was introduced in SDL 2.0.6, we have to lower the audio frequency
         to 44100 because otherwise we would get garbled audio output.*/
        want_aspec.freq = 44100;
    }
#endif
    
    device_id = SDL_OpenAudioDevice(0, 0, &want_aspec, &have_aspec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
    Mix_OpenAudio(want_aspec.freq, want_aspec.format, want_aspec.channels, want_aspec.samples / want_aspec.channels);
    
    return true;
}

static void _audio_deinit(void)
{
    _audio_set_paused(true);
    Mix_FreeMusic(music);
    SDL_CloseAudioDevice(device_id);
}

static void _audio_play_music(uint8_t music_id)
{
    Mix_FreeMusic(music);
    char filename[12];
    sprintf(filename, "Music/%x.mp3", music_id);
    music = Mix_LoadMUS(filename);
    if (music != NULL) {
        Mix_PlayMusic(music, 999);
    }
}

static void _audio_music_volume(int volume)
{
    if (volume != Mix_VolumeMusic(-1)) { // Passing -1 to volume queries the current volume
        Mix_VolumeMusic(volume);
    }
}

static void _audio_music_fade_out(int ms)
{
    Mix_FadeOutMusic(ms);
}

GB_AUDIO_DRIVER(SDL);
