#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <errno.h>
#include "libretro.h"
#include <alsa/asoundlib.h>

#define fatal(msg, ...)                      \
    {                                        \
    fprintf(stderr, "FATAL: ");          \
    fprintf(stderr, msg, ##__VA_ARGS__); \
    fprintf(stderr, "\n");               \
    shutdown();                          \
        exit(1);                             \
    }

#define load_sym(H, N)                                          \
    {                                                           \
    (*(void **)&N) = dlsym(H, #N);                          \
    if (!N)                                                 \
    {                                                       \
        fatal("failed to load symbol '#N': %s", dlerror()); \
        }                                                       \
    }

// #define DEBUG 1
#ifdef DEBUG
#define initscr() 0
#define endwin() 0
#define mvprintw(a, b, c) 0
#define timeout(x) 0
#define getch() 0
#define refresh() 0
#define attron(x) 0
#define attroff(x) 0
#define COLOR_RED 0
#define COLOR_GREEN 0
#define COLOR_BLUE 0
#define KEY_ENTER 1
#endif

struct g_app
{
    // WINDOW *win;
    snd_pcm_t *pcm;
    unsigned joypad[RETRO_DEVICE_ID_JOYPAD_L3 + 1];
    unsigned enter;
} g = {0};

void shutdown()
{
    if (g.pcm)
    {
        snd_pcm_close(g.pcm);
    }

    // if (g.win)
    // {
    //     delwin(g.win);
    // }

    endwin();
}

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

    uint16_t *pixels = (uint16_t *)data;

    start_color();
    init_pair(COLOR_RED, COLOR_RED, COLOR_RED);
    init_pair(COLOR_GREEN, COLOR_GREEN, COLOR_GREEN);
    init_pair(COLOR_BLUE, COLOR_BLUE, COLOR_BLUE);

    int step = 2;
    int skip = 0;
    for (int i = 0; i < width * height; i += step)
    {
        unsigned w = (i - skip) % width / step;
        unsigned h = (i - skip) / width / step;

        // ARRRRRGGGGBBBBB

        uint16_t a = (pixels[i] >> 14) & 1;
        // tranparent
        if (!a)
        {
            // wmove(g.win, h, w);
            // wprintw(g.win, " ");
            mvprintw(h, w, " ");
            continue;
        }

        uint16_t red = (pixels[i] >> 9) & 0x1F;
        uint16_t green = (pixels[i] >> 6) & 0x1F;
        uint16_t blue = pixels[i] & 0x1F;

        unsigned color = 0;
        if (red > green && red > blue)
        {
            color = COLOR_RED;
        }
        if (green > red && green > blue)
        {
            color = COLOR_GREEN;
        }
        if (blue > red && blue > green)
        {
            color = COLOR_BLUE;
        }

        if (!color)
        {
            // wmove(g.win, h, w);
            // wprintw(g.win, " ");
            mvprintw(h, w, " ");
            continue;
        }

        attron(COLOR_PAIR(color));
        // wmove(g.win, h, w);
        // wprintw(g.win, " ");
        mvprintw(h, w, " ");
        attroff(COLOR_PAIR(color));
    }

    // wrefresh(g.win);
    refresh();
}

size_t set_audio_sample_batch(const int16_t *data, size_t frames)
{
    if (!g.pcm)
    {
        return 0;
    }

    int n = snd_pcm_writei(g.pcm, data, frames);
    if (n < 0)
    {
        snd_pcm_recover(g.pcm, n, 0);
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
    int ch = getch();
    if (ch == ERR)
    {
        return;
    }

    if (ch == 'j')
        g.joypad[RETRO_DEVICE_ID_JOYPAD_LEFT] = 1;
    else
        g.joypad[RETRO_DEVICE_ID_JOYPAD_LEFT] = 0;

    if (ch == 'l')
        g.joypad[RETRO_DEVICE_ID_JOYPAD_RIGHT] = 1;
    else
        g.joypad[RETRO_DEVICE_ID_JOYPAD_RIGHT] = 0;

    if (ch == 'i')
        g.joypad[RETRO_DEVICE_ID_JOYPAD_UP] = 1;
    else
        g.joypad[RETRO_DEVICE_ID_JOYPAD_UP] = 0;

    if (ch == 'k')
        g.joypad[RETRO_DEVICE_ID_JOYPAD_DOWN] = 1;
    else
        g.joypad[RETRO_DEVICE_ID_JOYPAD_DOWN] = 0;

    if (ch == 'z')
        g.joypad[RETRO_DEVICE_ID_JOYPAD_B] = 1;
    else
        g.joypad[RETRO_DEVICE_ID_JOYPAD_B] = 0;

    if (ch == 'x')
        g.joypad[RETRO_DEVICE_ID_JOYPAD_A] = 1;
    else
        g.joypad[RETRO_DEVICE_ID_JOYPAD_A] = 0;

    if (ch == ' ')
        g.joypad[RETRO_DEVICE_ID_JOYPAD_START] = 1;
    else
        g.joypad[RETRO_DEVICE_ID_JOYPAD_START] = 0;
}

int16_t set_input_state(unsigned port, unsigned device, unsigned index, unsigned id)
{
    if (port || index || device != RETRO_DEVICE_JOYPAD)
    {
        return 0;
    }

    return g.joypad[id];
}

int main(int argc, char *argv[])
{

    if (argc != 3)
        fatal("usage: %s <corepath> <rompath>", argv[0]);

    void *handle = dlopen(argv[1], RTLD_LAZY);
    if (!handle)
        fatal("failed to load core %s: %s", argv[1], dlerror());

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
        fatal("failed to open rom: %s", strerror(errno));

    fseek(file, 0, SEEK_END);
    game.size = ftell(file);
    rewind(file);

    struct retro_system_info system = {0};
    retro_get_system_info(&system);

    if (!system.need_fullpath)
    {
        game.data = malloc(game.size);
        if (!game.data)
            fatal("failed to allocate %ld bytes: %s", game.size, strerror(errno));

        if (!fread((void *)game.data, game.size, 1, file))
            fatal("failed to read game rom: %s", strerror(errno));
    }

    if (!retro_load_game(&game))
        fatal("core failed to load game");

    // init sound
    struct retro_system_av_info av = {0};
    retro_get_system_av_info(&av);

    int err = snd_pcm_open(&g.pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0)
        fatal("failed to open playback device: %s", snd_strerror(err));

    err = snd_pcm_set_params(g.pcm, SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED, 2, av.timing.sample_rate, 1, 64 * 1000);
    if (err < 0)
        fatal("failed to configure playback device: %s", snd_strerror(err));

    // init window
    initscr();
    cbreak();
    keypad(stdscr, TRUE);
    timeout(5);

    // main loop
    for (;;)
        retro_run();

    shutdown();
}