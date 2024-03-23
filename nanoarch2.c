// #include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <errno.h>
#include "libretro.h"
#include <alsa/asoundlib.h>

#define load_sym(H, N)                                                  \
    (*(void **)&N) = dlsym(H, #N);                                      \
    if (!N)                                                             \
    {                                                                   \
        fprintf(stderr, "failed to load symbol '#N': %s\n", dlerror()); \
        return 1;                                                       \
    }

snd_pcm_t *pcm = NULL;

bool set_environment(unsigned cmd, void *data)
{
    return 0;
}

void set_video_refresh(const void *data, unsigned width, unsigned height, size_t pitch)
{
    if (!data)
    {
        return;
    }

    for (int x = 0; x < height; x++)
    {
        for (int y = 0; y < width; y++)
        {
            printf("%d", ((unsigned **)data)[x][y]);
        }
    }
}

void set_audio_sample(int16_t left, int16_t right)
{
}

size_t set_audio_sample_batch(const int16_t *data, size_t frames)
{
    if (!pcm)
    {
        return 0;
    }

    int n = snd_pcm_writei(pcm, data, frames);
    if (n < 0)
    {
        snd_pcm_recover(pcm, n, 0);
    return 0;
    }

    return n;
}

void set_audio_sample(int16_t left, int16_t right)
{
    int16_t buf[2] = {left, right};
    set_audio_sample_batch(buf, 1);
}

void set_input_poll(void)
{
}

int16_t set_input_state(unsigned port, unsigned device, unsigned index, unsigned id)
{
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <corepath> <rompath>\n", argv[0]);
        return 1;
    }

    void *handle = dlopen(argv[1], RTLD_LAZY);
    if (!handle)
    {
        fprintf(stderr, "failed to load core %s: %s\n", argv[1], dlerror());
        return 1;
    }

    void (*retro_set_environment)(retro_environment_t);
    void (*retro_set_video_refresh)(retro_video_refresh_t);
    void (*retro_set_audio_sample)(retro_audio_sample_t);
    void (*retro_set_audio_sample_batch)(retro_audio_sample_batch_t);
    void (*retro_set_input_poll)(retro_input_poll_t);
    void (*retro_set_input_state)(retro_input_state_t);

    void (*retro_init)(void) = NULL;
    void (*retro_deinit)(void) = NULL;
    unsigned (*retro_api_version)(void) = NULL;
    void (*retro_get_system_info)(struct retro_system_info *) = NULL;
    void (*retro_get_system_av_info)(struct retro_system_av_info *) = NULL;
    void (*retro_set_controller_port_device)(unsigned, unsigned) = NULL;
    void (*retro_reset)(void) = NULL;
    void (*retro_run)(void) = NULL;
    size_t (*retro_serialize_size)(void) = NULL;
    bool (*retro_serialize)(void *, size_t) = NULL;
    bool (*retro_unserialize)(const void *, size_t) = NULL;
    bool (*retro_load_game)(const struct retro_game_info *) = NULL;
    void (*retro_unload_game)(void) = NULL;

    load_sym(handle, retro_set_environment);
    load_sym(handle, retro_set_video_refresh);
    load_sym(handle, retro_set_audio_sample);
    load_sym(handle, retro_set_audio_sample_batch);
    load_sym(handle, retro_set_input_poll);
    load_sym(handle, retro_set_input_state);

    load_sym(handle, retro_init);
    load_sym(handle, retro_deinit);
    load_sym(handle, retro_api_version);
    load_sym(handle, retro_get_system_info);
    load_sym(handle, retro_get_system_av_info);
    load_sym(handle, retro_set_controller_port_device);
    load_sym(handle, retro_reset);
    load_sym(handle, retro_run);
    load_sym(handle, retro_serialize_size);
    load_sym(handle, retro_serialize);
    load_sym(handle, retro_unserialize);
    load_sym(handle, retro_load_game);
    load_sym(handle, retro_unload_game);

    retro_set_environment(set_environment);
    retro_set_video_refresh(set_video_refresh);
    retro_set_audio_sample(set_audio_sample);
    retro_set_audio_sample_batch(set_audio_sample_batch);
    retro_set_input_poll(set_input_poll);
    retro_set_input_state(set_input_state);

    // load core
    retro_init();
    fprintf(stderr, "core loaded\n");

    // load game
    struct retro_game_info game = {argv[2], 0};

    FILE *file = fopen(argv[2], "rb");
    if (!file)
    {
        fprintf(stderr, "failed to open rom: %s\n", strerror(errno));
        return 1;
    }

    fseek(file, 0, SEEK_END);
    game.size = ftell(file);
    rewind(file);

    struct retro_system_info system = {0};
    retro_get_system_info(&system);

    if (!system.need_fullpath)
    {
        game.data = malloc(game.size);
        if (!game.data)
        {
            fprintf(stderr, "failed to allocate %ld bytes: %s\n", game.size, strerror(errno));
            return 1;
        }

        if (!fread((void *)game.data, game.size, 1, file))
        {
            fprintf(stderr, "failed to read game rom: %s\n", strerror(errno));
            return 1;
        }
    }

    if (!retro_load_game(&game))
    {
        fprintf(stderr, "core failed to load game\n");
        return 1;
    }

    struct retro_system_av_info av = {0};
    retro_get_system_av_info(&av);

    // init sound

    int err = snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0)
    {
        fprintf(stderr, "failed to open playback device: %s", snd_strerror(err));
        return 1;
    }

    err = snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED, 2, av.timing.sample_rate, 1, 64 * 1000);
    if (err < 0)
    {
        fprintf(stderr, "failed to configure playback device: %s", snd_strerror(err));
        return 1;
    }

    for (;;)
    {
        retro_run();
    }

    // configure video and init audio

    // initscr();
    // printw("negoou");
    // refresh();
    // getch();
    // endwin();
}