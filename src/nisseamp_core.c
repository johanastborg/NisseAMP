#define GL_SILENCE_DEPRECATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "nisseamp_core.h"
#include <GLFW/glfw3.h>
#include <OpenGL/gl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#define WIN_BASE_W 275
#define WIN_BASE_H 116
#define WIN_SHADE_H 14

#define NUM_BANDS 19
#define VIS_BUF_SIZE 2048
#define MARQUEE_VISIBLE_CHARS 30

// Texture structure
typedef struct {
    GLuint id;
    int w;
    int h;
    int loaded;
} Texture;

// Visualizer color palette (24 RGB colors)
static float vis_colors[24][3];

// Textures
static Texture tex_main;
static Texture tex_titlebar;
static Texture tex_cbuttons;
static Texture tex_numbers;
static Texture tex_text;
static Texture tex_playpaus;
static Texture tex_monoster;
static Texture tex_shufrep;
static Texture tex_volume;
static Texture tex_balance;
static Texture tex_posbar;

// GLFW Window
static GLFWwindow* g_window = NULL;
static int g_scale = 2;
static int g_borderless = 1;
static int g_is_shade = 0;
static int g_should_close = 0;

// Audio state
static ma_device g_audio_device;
static ma_decoder g_decoder;
static int g_device_initialized = 0;
static int g_decoder_loaded = 0;
static int g_play_status = 0; // 0=stopped, 1=playing, 2=paused
static int g_is_synth_mode = 0;

// Audio settings
static float g_volume = 0.85f;
static float g_balance = 0.0f; // -1.0 to +1.0
static int g_shuffle = 0;
static int g_repeat = 1;
static int g_eq_open = 0;
static int g_pl_open = 0;
static int g_vis_mode = 1; // 0=off, 1=spectrum, 2=oscilloscope
static int g_time_mode = 0; // 0=elapsed, 1=remaining

// Track info
static char g_title[256] = "1. DJ Mike Llama - Llama Whippin' Intro (0:05)";
static int g_kbps = 320;
static int g_khz = 44;
static int g_stereo = 1;
static double g_elapsed_seconds = 0.0;
static double g_total_seconds = 5.33;
static ma_uint64 g_current_frame = 0;
static ma_uint64 g_total_frames = 0;
static int g_sample_rate = 44100;

// Temporary marquee message (e.g. "VOLUME: 85%")
static char g_temp_message[128] = "";
static double g_temp_message_expire = 0.0;
static double g_marquee_scroll_pos = 0.0;

// Visualizer audio buffer and spectrum analysis
static float g_vis_pcm[VIS_BUF_SIZE];
static int g_vis_write_idx = 0;
static float g_band_levels[NUM_BANDS];
static float g_band_peaks[NUM_BANDS];
static int g_band_peak_hold[NUM_BANDS];

// UI interaction state
static int g_dragging_window = 0;
static double g_drag_start_cursor_x = 0;
static double g_drag_start_cursor_y = 0;
static int g_drag_start_win_x = 0;
static int g_drag_start_win_y = 0;

// Slider dragging: 0=none, 1=volume, 2=balance, 3=seek
static int g_dragging_slider = 0;
static int g_pressed_button = 0;
// Button IDs:
// 1=prev, 2=play, 3=pause, 4=stop, 5=next, 6=eject
// 7=shuffle, 8=repeat, 9=eq, 10=pl
// 11=min, 12=shade, 13=close
// 14=clutter_o, 15=clutter_a, 16=clutter_i, 17=clutter_d, 18=clutter_v

// Drag & drop file
static char g_dropped_file[1024] = "";
static int g_has_dropped_file = 0;

// Procedural synthesizer state
static double g_synth_time = 0.0;

// Forward declarations
static void audio_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
static void synth_generate_audio(float* output, ma_uint32 frameCount);
static void compute_visualizer(void);
static void draw_sprite(Texture* tex, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh);
static void draw_string(const char* str, int x, int y, int max_chars);
static void draw_number_digit(int digit, int x, int y);
static void init_default_vis_colors(void);

// Texture loading helper
static Texture load_texture_png(const char* path) {
    Texture tex;
    tex.id = 0;
    tex.w = 0;
    tex.h = 0;
    tex.loaded = 0;

    int channels;
    unsigned char* data = stbi_load(path, &tex.w, &tex.h, &channels, 4);
    if (!data) {
        fprintf(stderr, "[NisseAMP] Failed to load texture: %s (%s)\n", path, stbi_failure_reason());
        return tex;
    }

    glGenTextures(1, &tex.id);
    glBindTexture(GL_TEXTURE_2D, tex.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex.w, tex.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
    tex.loaded = 1;
    return tex;
}

// Character map for TEXT.png
// 5x6 pixels per char
static int get_char_coords(char c, int* out_x, int* out_y) {
    c = tolower((unsigned char)c);
    if (c >= 'a' && c <= 'z') {
        *out_x = (c - 'a') * 5;
        *out_y = 0;
        return 1;
    }
    if (c == '"') { *out_x = 26 * 5; *out_y = 0; return 1; }
    if (c == '@') { *out_x = 27 * 5; *out_y = 0; return 1; }
    if (c == ' ') { *out_x = 30 * 5; *out_y = 0; return 1; }

    if (c >= '0' && c <= '9') {
        *out_x = (c - '0') * 5;
        *out_y = 6;
        return 1;
    }
    if (c == '.') { *out_x = 11 * 5; *out_y = 6; return 1; }
    if (c == ':') { *out_x = 12 * 5; *out_y = 6; return 1; }
    if (c == '(') { *out_x = 13 * 5; *out_y = 6; return 1; }
    if (c == ')') { *out_x = 14 * 5; *out_y = 6; return 1; }
    if (c == '-') { *out_x = 15 * 5; *out_y = 6; return 1; }
    if (c == '\'') { *out_x = 16 * 5; *out_y = 6; return 1; }
    if (c == '!') { *out_x = 17 * 5; *out_y = 6; return 1; }
    if (c == '_') { *out_x = 18 * 5; *out_y = 6; return 1; }
    if (c == '+') { *out_x = 19 * 5; *out_y = 6; return 1; }
    if (c == '\\') { *out_x = 20 * 5; *out_y = 6; return 1; }
    if (c == '/') { *out_x = 21 * 5; *out_y = 6; return 1; }
    if (c == '[' || c == '<') { *out_x = 22 * 5; *out_y = 6; return 1; }
    if (c == ']' || c == '>') { *out_x = 23 * 5; *out_y = 6; return 1; }
    if (c == '^') { *out_x = 24 * 5; *out_y = 6; return 1; }
    if (c == '&') { *out_x = 25 * 5; *out_y = 6; return 1; }
    if (c == '%') { *out_x = 26 * 5; *out_y = 6; return 1; }
    if (c == ',') { *out_x = 27 * 5; *out_y = 6; return 1; }
    if (c == '=') { *out_x = 28 * 5; *out_y = 6; return 1; }
    if (c == '$') { *out_x = 29 * 5; *out_y = 6; return 1; }
    if (c == '#') { *out_x = 30 * 5; *out_y = 6; return 1; }
    if (c == '?') { *out_x = 3 * 5; *out_y = 12; return 1; }
    if (c == '*') { *out_x = 4 * 5; *out_y = 12; return 1; }

    *out_x = 30 * 5;
    *out_y = 0;
    return 1;
}

// GLFW callbacks
static void drop_callback(GLFWwindow* window, int count, const char** paths) {
    (void)window;
    if (count > 0 && paths[0]) {
        strncpy(g_dropped_file, paths[0], sizeof(g_dropped_file) - 1);
        g_dropped_file[sizeof(g_dropped_file) - 1] = '\0';
        g_has_dropped_file = 1;
        nisseamp_load_audio(g_dropped_file);
        nisseamp_play();
    }
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    (void)mods;
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;

    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    // Convert to 275x116 space
    float x = (float)(mx / g_scale);
    float y = (float)(my / g_scale);

    if (action == GLFW_PRESS) {
        // Window shade mode check
        if (g_is_shade) {
            // Titlebar buttons in shade mode
            if (x >= 244 && x < 253 && y >= 3 && y < 12) { g_pressed_button = 11; return; } // Min
            if (x >= 254 && x < 263 && y >= 3 && y < 12) { g_pressed_button = 12; return; } // Shade
            if (x >= 264 && x < 273 && y >= 3 && y < 12) { g_pressed_button = 13; return; } // Close

            // Shade action buttons
            if (x >= 169 && x < 176 && y >= 2 && y < 12) { nisseamp_prev(); return; }
            if (x >= 176 && x < 186 && y >= 2 && y < 12) { nisseamp_play(); return; }
            if (x >= 186 && x < 195 && y >= 2 && y < 12) { nisseamp_pause(); return; }
            if (x >= 195 && x < 204 && y >= 2 && y < 12) { nisseamp_stop(); return; }
            if (x >= 204 && x < 214 && y >= 2 && y < 12) { nisseamp_next(); return; }

            // Drag window in shade mode
            g_dragging_window = 1;
            glfwGetCursorPos(window, &g_drag_start_cursor_x, &g_drag_start_cursor_y);
            glfwGetWindowPos(window, &g_drag_start_win_x, &g_drag_start_win_y);
            return;
        }

        // Full mode controls:
        // Window control buttons
        if (x >= 244 && x < 253 && y >= 3 && y < 12) { g_pressed_button = 11; return; } // Min
        if (x >= 254 && x < 263 && y >= 3 && y < 12) { g_pressed_button = 12; return; } // Shade
        if (x >= 264 && x < 273 && y >= 3 && y < 12) { g_pressed_button = 13; return; } // Close

        // Clutter bar buttons
        if (x >= 10 && x < 18 && y >= 25 && y < 33) { g_pressed_button = 14; return; } // O
        if (x >= 10 && x < 18 && y >= 33 && y < 40) { g_pressed_button = 15; return; } // A
        if (x >= 10 && x < 18 && y >= 40 && y < 47) { g_pressed_button = 16; return; } // I
        if (x >= 10 && x < 18 && y >= 47 && y < 55) { // D (Double size)
            g_pressed_button = 17;
            nisseamp_set_scale(g_scale == 2 ? 3 : (g_scale == 3 ? 1 : 2));
            return;
        }
        if (x >= 10 && x < 18 && y >= 55 && y < 62) { // V (Visualizer mode)
            g_pressed_button = 18;
            nisseamp_set_vis_mode((g_vis_mode + 1) % 3);
            return;
        }

        // Transport buttons
        if (x >= 16 && x < 39 && y >= 88 && y < 106) { g_pressed_button = 1; return; } // Prev
        if (x >= 39 && x < 62 && y >= 88 && y < 106) { g_pressed_button = 2; return; } // Play
        if (x >= 62 && x < 85 && y >= 88 && y < 106) { g_pressed_button = 3; return; } // Pause
        if (x >= 85 && x < 108 && y >= 88 && y < 106) { g_pressed_button = 4; return; } // Stop
        if (x >= 108 && x < 130 && y >= 88 && y < 106) { g_pressed_button = 5; return; } // Next
        if (x >= 136 && x < 158 && y >= 89 && y < 105) { g_pressed_button = 6; return; } // Eject

        // Shuffle / Repeat
        if (x >= 164 && x < 211 && y >= 89 && y < 104) {
            g_pressed_button = 7;
            nisseamp_set_shuffle(!g_shuffle);
            nisseamp_show_message(g_shuffle ? "SHUFFLE: ON" : "SHUFFLE: OFF", 2.0f);
            return;
        }
        if (x >= 210 && x < 238 && y >= 89 && y < 104) {
            g_pressed_button = 8;
            nisseamp_set_repeat(!g_repeat);
            nisseamp_show_message(g_repeat ? "REPEAT: ON" : "REPEAT: OFF", 2.0f);
            return;
        }

        // EQ / PL buttons
        if (x >= 219 && x < 242 && y >= 58 && y < 70) {
            g_pressed_button = 9;
            g_eq_open = !g_eq_open;
            nisseamp_show_message(g_eq_open ? "EQ: ACTIVE" : "EQ: CLOSED", 1.5f);
            return;
        }
        if (x >= 242 && x < 265 && y >= 58 && y < 70) {
            g_pressed_button = 10;
            g_pl_open = !g_pl_open;
            nisseamp_show_message(g_pl_open ? "PLAYLIST: ACTIVE" : "PLAYLIST: CLOSED", 1.5f);
            return;
        }

        // Time display click: toggle elapsed vs remaining
        if (x >= 36 && x < 100 && y >= 26 && y < 40) {
            nisseamp_set_time_mode(!g_time_mode);
            nisseamp_show_message(g_time_mode ? "TIME: REMAINING" : "TIME: ELAPSED", 1.5f);
            return;
        }

        // Visualizer click: cycle spectrum -> oscilloscope -> off
        if (x >= 24 && x < 99 && y >= 43 && y < 59) {
            nisseamp_set_vis_mode((g_vis_mode + 1) % 3);
            if (g_vis_mode == 1) nisseamp_show_message("VISUALIZER: SPECTRUM", 1.5f);
            else if (g_vis_mode == 2) nisseamp_show_message("VISUALIZER: OSCILLOSCOPE", 1.5f);
            else nisseamp_show_message("VISUALIZER: OFF", 1.5f);
            return;
        }

        // Sliders:
        // Volume Slider (107, 57, 68, 13)
        if (x >= 107 && x < 175 && y >= 57 && y < 70) {
            g_dragging_slider = 1;
            float v = (x - 107.0f) / (68.0f - 14.0f);
            if (v < 0.0f) v = 0.0f;
            if (v > 1.0f) v = 1.0f;
            nisseamp_set_volume(v);
            char buf[64];
            snprintf(buf, sizeof(buf), "VOLUME: %d%%", (int)(v * 100.0f));
            nisseamp_show_message(buf, 2.0f);
            return;
        }

        // Balance Slider (177, 57, 38, 13)
        if (x >= 177 && x < 215 && y >= 57 && y < 70) {
            g_dragging_slider = 2;
            float b = ((x - 177.0f) / (38.0f - 14.0f)) * 2.0f - 1.0f;
            if (b < -1.0f) b = -1.0f;
            if (b > 1.0f) b = 1.0f;
            if (fabsf(b) < 0.1f) b = 0.0f; // Center detent
            nisseamp_set_balance(b);
            char buf[64];
            if (fabsf(b) < 0.01f) snprintf(buf, sizeof(buf), "BALANCE: CENTER");
            else if (b < 0) snprintf(buf, sizeof(buf), "BALANCE: %d%% LEFT", (int)(-b * 100.0f));
            else snprintf(buf, sizeof(buf), "BALANCE: %d%% RIGHT", (int)(b * 100.0f));
            nisseamp_show_message(buf, 2.0f);
            return;
        }

        // Position / Seek Bar (16, 72, 248, 10)
        if (x >= 16 && x < 264 && y >= 72 && y < 82) {
            g_dragging_slider = 3;
            float pos = (x - 16.0f) / (248.0f - 29.0f);
            if (pos < 0.0f) pos = 0.0f;
            if (pos > 1.0f) pos = 1.0f;
            nisseamp_seek(pos);
            int cur_m = (int)(g_elapsed_seconds / 60.0);
            int cur_s = (int)fmod(g_elapsed_seconds, 60.0);
            int tot_m = (int)(g_total_seconds / 60.0);
            int tot_s = (int)fmod(g_total_seconds, 60.0);
            char buf[64];
            snprintf(buf, sizeof(buf), "SEEK TO: %02d:%02d/%02d:%02d (%d%%)", cur_m, cur_s, tot_m, tot_s, (int)(pos * 100.0f));
            nisseamp_show_message(buf, 2.0f);
            return;
        }

        // Otherwise: Drag window if clicking titlebar or unassigned metal area
        if (y < 20 || (y < 70 && x < 24) || y > 105) {
            g_dragging_window = 1;
            glfwGetCursorPos(window, &g_drag_start_cursor_x, &g_drag_start_cursor_y);
            glfwGetWindowPos(window, &g_drag_start_win_x, &g_drag_start_win_y);
            return;
        }
    } else if (action == GLFW_RELEASE) {
        if (g_dragging_window) {
            g_dragging_window = 0;
        }
        if (g_dragging_slider) {
            g_dragging_slider = 0;
        }
        if (g_pressed_button > 0) {
            int b = g_pressed_button;
            g_pressed_button = 0;
            switch (b) {
                case 1: nisseamp_prev(); break;
                case 2: nisseamp_play(); break;
                case 3: nisseamp_pause(); break;
                case 4: nisseamp_stop(); break;
                case 5: nisseamp_next(); break;
                case 6: // Eject / Open
                    nisseamp_play_synth();
                    break;
                case 11: // Minimize
                    glfwIconifyWindow(g_window);
                    break;
                case 12: // Shade
                    nisseamp_toggle_shade();
                    break;
                case 13: // Close
                    g_should_close = 1;
                    glfwSetWindowShouldClose(g_window, GLFW_TRUE);
                    break;
                default: break;
            }
        }
    }
}

static void cursor_pos_callback(GLFWwindow* window, double mx, double my) {
    if (g_dragging_window) {
        int cur_wx, cur_wy;
        glfwGetWindowPos(window, &cur_wx, &cur_wy);
        int new_x = g_drag_start_win_x + (int)(mx - g_drag_start_cursor_x);
        int new_y = g_drag_start_win_y + (int)(my - g_drag_start_cursor_y);
        glfwSetWindowPos(window, new_x, new_y);
        return;
    }

    float x = (float)(mx / g_scale);

    if (g_dragging_slider == 1) { // Volume
        float v = (x - 107.0f) / (68.0f - 14.0f);
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        nisseamp_set_volume(v);
        char buf[64];
        snprintf(buf, sizeof(buf), "VOLUME: %d%%", (int)(v * 100.0f));
        nisseamp_show_message(buf, 1.5f);
    } else if (g_dragging_slider == 2) { // Balance
        float b = ((x - 177.0f) / (38.0f - 14.0f)) * 2.0f - 1.0f;
        if (b < -1.0f) b = -1.0f;
        if (b > 1.0f) b = 1.0f;
        if (fabsf(b) < 0.1f) b = 0.0f;
        nisseamp_set_balance(b);
        char buf[64];
        if (fabsf(b) < 0.01f) snprintf(buf, sizeof(buf), "BALANCE: CENTER");
        else if (b < 0) snprintf(buf, sizeof(buf), "BALANCE: %d%% LEFT", (int)(-b * 100.0f));
        else snprintf(buf, sizeof(buf), "BALANCE: %d%% RIGHT", (int)(b * 100.0f));
        nisseamp_show_message(buf, 1.5f);
    } else if (g_dragging_slider == 3) { // Seek
        float pos = (x - 16.0f) / (248.0f - 29.0f);
        if (pos < 0.0f) pos = 0.0f;
        if (pos > 1.0f) pos = 1.0f;
        nisseamp_seek(pos);
        int cur_m = (int)(g_elapsed_seconds / 60.0);
        int cur_s = (int)fmod(g_elapsed_seconds, 60.0);
        int tot_m = (int)(g_total_seconds / 60.0);
        int tot_s = (int)fmod(g_total_seconds, 60.0);
        char buf[64];
        snprintf(buf, sizeof(buf), "SEEK TO: %02d:%02d/%02d:%02d (%d%%)", cur_m, cur_s, tot_m, tot_s, (int)(pos * 100.0f));
        nisseamp_show_message(buf, 1.5f);
    }
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)window;
    (void)xoffset;
    float new_vol = g_volume + (float)yoffset * 0.05f;
    if (new_vol < 0.0f) new_vol = 0.0f;
    if (new_vol > 1.0f) new_vol = 1.0f;
    nisseamp_set_volume(new_vol);
    char buf[64];
    snprintf(buf, sizeof(buf), "VOLUME: %d%%", (int)(new_vol * 100.0f));
    nisseamp_show_message(buf, 1.5f);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)window;
    (void)scancode;
    (void)mods;
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    switch (key) {
        case GLFW_KEY_Z: nisseamp_prev(); break;
        case GLFW_KEY_X: nisseamp_play(); break;
        case GLFW_KEY_C: nisseamp_pause(); break;
        case GLFW_KEY_V: nisseamp_stop(); break;
        case GLFW_KEY_B: nisseamp_next(); break;
        case GLFW_KEY_L: nisseamp_play_synth(); break;
        case GLFW_KEY_SPACE:
            if (g_play_status == 1) nisseamp_pause();
            else nisseamp_play();
            break;
        case GLFW_KEY_S:
            nisseamp_set_shuffle(!g_shuffle);
            nisseamp_show_message(g_shuffle ? "SHUFFLE: ON" : "SHUFFLE: OFF", 2.0f);
            break;
        case GLFW_KEY_R:
            nisseamp_set_repeat(!g_repeat);
            nisseamp_show_message(g_repeat ? "REPEAT: ON" : "REPEAT: OFF", 2.0f);
            break;
        case GLFW_KEY_D: // Toggle double size
            nisseamp_set_scale(g_scale == 2 ? 3 : (g_scale == 3 ? 1 : 2));
            break;
        case GLFW_KEY_W: // Windowshade
            nisseamp_toggle_shade();
            break;
        case GLFW_KEY_1: nisseamp_set_scale(1); break;
        case GLFW_KEY_2: nisseamp_set_scale(2); break;
        case GLFW_KEY_3: nisseamp_set_scale(3); break;
        case GLFW_KEY_4: nisseamp_set_scale(4); break;
        case GLFW_KEY_UP: {
            float v = g_volume + 0.05f;
            if (v > 1.0f) v = 1.0f;
            nisseamp_set_volume(v);
            char buf[64];
            snprintf(buf, sizeof(buf), "VOLUME: %d%%", (int)(v * 100.0f));
            nisseamp_show_message(buf, 1.5f);
            break;
        }
        case GLFW_KEY_DOWN: {
            float v = g_volume - 0.05f;
            if (v < 0.0f) v = 0.0f;
            nisseamp_set_volume(v);
            char buf[64];
            snprintf(buf, sizeof(buf), "VOLUME: %d%%", (int)(v * 100.0f));
            nisseamp_show_message(buf, 1.5f);
            break;
        }
        case GLFW_KEY_LEFT: {
            double new_time = g_elapsed_seconds - 5.0;
            if (new_time < 0.0) new_time = 0.0;
            if (g_total_seconds > 0.0) nisseamp_seek((float)(new_time / g_total_seconds));
            break;
        }
        case GLFW_KEY_RIGHT: {
            double new_time = g_elapsed_seconds + 5.0;
            if (new_time > g_total_seconds) new_time = g_total_seconds;
            if (g_total_seconds > 0.0) nisseamp_seek((float)(new_time / g_total_seconds));
            break;
        }
        case GLFW_KEY_ESCAPE:
            g_should_close = 1;
            glfwSetWindowShouldClose(g_window, GLFW_TRUE);
            break;
        default: break;
    }
}

// Default visualizer colors (from VISCOLOR.TXT)
static void init_default_vis_colors(void) {
    int default_rgb[24][3] = {
        {0, 0, 0},         // 0: background
        {24, 33, 41},      // 1: grey for dots
        {239, 49, 16},     // 2: top of spec (red)
        {206, 41, 16},     // 3
        {214, 90, 0},      // 4
        {214, 102, 0},     // 5
        {214, 115, 0},     // 6
        {198, 123, 8},     // 7
        {222, 165, 24},    // 8
        {214, 181, 33},    // 9
        {189, 222, 41},    // 10
        {148, 222, 33},    // 11
        {41, 206, 16},     // 12
        {50, 190, 16},     // 13
        {57, 181, 16},     // 14
        {49, 156, 8},      // 15
        {41, 148, 0},      // 16
        {24, 132, 8},      // 17: bottom of spec (green)
        {255, 255, 255},   // 18: osc bright
        {214, 214, 222},   // 19: osc 2
        {181, 189, 189},   // 20: osc 3
        {160, 170, 175},   // 21: osc 4
        {148, 156, 165},   // 22: osc 5
        {150, 150, 150}    // 23: analyzer peak dots
    };

    for (int i = 0; i < 24; i++) {
        vis_colors[i][0] = default_rgb[i][0] / 255.0f;
        vis_colors[i][1] = default_rgb[i][1] / 255.0f;
        vis_colors[i][2] = default_rgb[i][2] / 255.0f;
    }
}

// Procedural Retro Chiptune Synthesizer
static void synth_generate_audio(float* output, ma_uint32 frameCount) {
    // 128 BPM, 4 beats per bar, 16 steps per bar
    // Simple upbeat demoscene arpeggio chords: Am - F - C - G
    const float chord_notes[4][4] = {
        {220.0f, 261.63f, 329.63f, 440.0f}, // Am
        {174.61f, 220.0f, 261.63f, 349.23f}, // F
        {130.81f, 164.81f, 196.0f, 261.63f}, // C
        {196.0f, 246.94f, 293.66f, 392.0f}  // G
    };
    const float bass_notes[4] = {110.0f, 87.31f, 65.41f, 98.0f};

    double dt = 1.0 / 44100.0;
    double sec_per_step = 60.0 / (128.0 * 4.0); // 16th note

    for (ma_uint32 i = 0; i < frameCount; i++) {
        g_synth_time += dt;
        int bar_step = (int)(g_synth_time / sec_per_step) % 64;
        int chord_idx = (bar_step / 16) % 4;
        int arp_idx = (bar_step % 4);

        float lead_freq = chord_notes[chord_idx][arp_idx];
        float bass_freq = bass_notes[chord_idx];

        // Channel 1: Lead Pulse wave (with slight vibrato)
        double vib = sin(g_synth_time * 12.0) * 3.0;
        double p1 = fmod(g_synth_time * (lead_freq + vib), 1.0);
        float lead = (p1 < 0.35 ? 0.25f : -0.25f);

        // Channel 2: Arpeggio detuned pulse wave
        double p2 = fmod(g_synth_time * (chord_notes[chord_idx][(arp_idx + 2) % 4] * 1.005), 1.0);
        float lead2 = (p2 < 0.5 ? 0.18f : -0.18f);

        // Channel 3: Bass saw wave
        double p3 = fmod(g_synth_time * bass_freq, 1.0);
        float bass = (float)(p3 * 2.0 - 1.0) * 0.35f;

        // Channel 4: Noise drum (kick on 0,4,8,12; snare on 4,12; hi-hat on every 2)
        int step_in_beat = bar_step % 4;
        double step_frac = fmod(g_synth_time, sec_per_step) / sec_per_step;
        float drum = 0.0f;
        if (bar_step % 8 == 4) { // Snare
            drum += ((float)rand() / (float)RAND_MAX * 2.0f - 1.0f) * (float)exp(-step_frac * 8.0) * 0.35f;
        } else if (step_in_beat == 0) { // Kick
            float kick_freq = 120.0f * (float)exp(-step_frac * 15.0);
            drum += sinf((float)(step_frac * kick_freq * 6.283f)) * (float)exp(-step_frac * 4.0) * 0.45f;
        } else { // Hi-hat
            drum += ((float)rand() / (float)RAND_MAX * 2.0f - 1.0f) * (float)exp(-step_frac * 25.0) * 0.12f;
        }

        float mix = (lead + lead2 + bass + drum) * 0.7f;
        output[i * 2 + 0] = mix;
        output[i * 2 + 1] = mix;
    }

    g_elapsed_seconds = g_synth_time;
    g_total_seconds = 120.0; // Virtual loop
}

// Audio device callback
static void audio_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    (void)pDevice;
    (void)pInput;
    float* pOut = (float*)pOutput;

    if (g_play_status != 1) { // Not playing (stopped or paused)
        memset(pOut, 0, frameCount * 2 * sizeof(float));
        return;
    }

    if (g_is_synth_mode || !g_decoder_loaded) {
        synth_generate_audio(pOut, frameCount);
    } else {
        ma_uint64 framesRead = 0;
        ma_result res = ma_decoder_read_pcm_frames(&g_decoder, pOut, frameCount, &framesRead);
        if (res != MA_SUCCESS || framesRead == 0) {
            if (g_repeat) {
                ma_decoder_seek_to_pcm_frame(&g_decoder, 0);
                g_current_frame = 0;
                framesRead = 0;
                ma_decoder_read_pcm_frames(&g_decoder, pOut, frameCount, &framesRead);
            } else {
                g_play_status = 0;
                memset(pOut, 0, frameCount * 2 * sizeof(float));
                return;
            }
        }
        g_current_frame += framesRead;
        g_elapsed_seconds = (double)g_current_frame / g_sample_rate;

        // Zero out any unfilled frames
        if (framesRead < frameCount) {
            memset(pOut + framesRead * 2, 0, (frameCount - framesRead) * 2 * sizeof(float));
        }
    }

    // Apply volume & balance
    float left_vol = g_volume;
    float right_vol = g_volume;
    if (g_balance < 0.0f) {
        right_vol *= (1.0f + g_balance);
    } else if (g_balance > 0.0f) {
        left_vol *= (1.0f - g_balance);
    }

    for (ma_uint32 i = 0; i < frameCount; i++) {
        pOut[i * 2 + 0] *= left_vol;
        pOut[i * 2 + 1] *= right_vol;

        // Write mono sum to visualizer circular buffer
        float mono = (pOut[i * 2 + 0] + pOut[i * 2 + 1]) * 0.5f;
        g_vis_pcm[g_vis_write_idx] = mono;
        g_vis_write_idx = (g_vis_write_idx + 1) % VIS_BUF_SIZE;
    }
}

// Compute frequency bands for spectrum analyzer
static void compute_visualizer(void) {
    if (g_play_status != 1) {
        // Fall down to zero when not playing
        for (int i = 0; i < NUM_BANDS; i++) {
            g_band_levels[i] *= 0.85f;
            if (g_band_peaks[i] > 0.0f) g_band_peaks[i] -= 0.35f;
            if (g_band_peaks[i] < 0.0f) g_band_peaks[i] = 0.0f;
        }
        return;
    }

    // Compute 19 logarithmically spaced frequency bins (60 Hz to 15000 Hz)
    int n_samples = 256;
    int read_start = (g_vis_write_idx - n_samples + VIS_BUF_SIZE) % VIS_BUF_SIZE;

    for (int b = 0; b < NUM_BANDS; b++) {
        float f = 60.0f * powf(15000.0f / 60.0f, (float)b / (NUM_BANDS - 1));
        float omega = 2.0f * 3.14159265f * f / 44100.0f;

        float sin_acc = 0.0f;
        float cos_acc = 0.0f;
        for (int s = 0; s < n_samples; s++) {
            int idx = (read_start + s) % VIS_BUF_SIZE;
            float val = g_vis_pcm[idx];
            sin_acc += val * sinf(s * omega);
            cos_acc += val * cosf(s * omega);
        }
        float mag = sqrtf(sin_acc * sin_acc + cos_acc * cos_acc) / (float)n_samples;
        // Equalization boost for higher frequencies
        mag *= (1.0f + (float)b * 0.45f);

        float level = mag * 35.0f;
        if (level > 15.0f) level = 15.0f;

        // Smooth rise / decay
        if (level > g_band_levels[b]) {
            g_band_levels[b] = g_band_levels[b] * 0.3f + level * 0.7f;
        } else {
            g_band_levels[b] = g_band_levels[b] * 0.75f + level * 0.25f;
        }

        // Peak hold dots
        if (g_band_levels[b] >= g_band_peaks[b]) {
            g_band_peaks[b] = g_band_levels[b];
            g_band_peak_hold[b] = 8;
        } else {
            if (g_band_peak_hold[b] > 0) {
                g_band_peak_hold[b]--;
            } else {
                g_band_peaks[b] -= 0.4f;
                if (g_band_peaks[b] < 0.0f) g_band_peaks[b] = 0.0f;
            }
        }
    }
}

// 2D textured quad rendering
static void draw_sprite(Texture* tex, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh) {
    if (!tex || !tex->loaded) return;

    glBindTexture(GL_TEXTURE_2D, tex->id);
    glEnable(GL_TEXTURE_2D);

    float u0 = (float)sx / (float)tex->w;
    float v0 = (float)sy / (float)tex->h;
    float u1 = (float)(sx + sw) / (float)tex->w;
    float v1 = (float)(sy + sh) / (float)tex->h;

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(u0, v0); glVertex2f((float)dx, (float)dy);
    glTexCoord2f(u1, v0); glVertex2f((float)(dx + dw), (float)dy);
    glTexCoord2f(u1, v1); glVertex2f((float)(dx + dw), (float)(dy + dh));
    glTexCoord2f(u0, v1); glVertex2f((float)dx, (float)(dy + dh));
    glEnd();

    glDisable(GL_TEXTURE_2D);
}

// Render text using TEXT.png
static void draw_string(const char* str, int x, int y, int max_chars) {
    if (!str || !tex_text.loaded) return;
    int len = (int)strlen(str);
    if (len > max_chars) len = max_chars;

    for (int i = 0; i < len; i++) {
        int sx, sy;
        get_char_coords(str[i], &sx, &sy);
        draw_sprite(&tex_text, sx, sy, 5, 6, x + i * 5, y, 5, 6);
    }
}

// Render 9x13 digit from NUMBERS.png
static void draw_number_digit(int digit, int x, int y) {
    if (digit < 0 || digit > 9 || !tex_numbers.loaded) return;
    draw_sprite(&tex_numbers, digit * 9, 0, 9, 13, x, y, 9, 13);
}

// Core API implementations
int nisseamp_init(int scale, int borderless) {
    if (scale < 1) scale = 1;
    if (scale > 4) scale = 4;
    g_scale = scale;
    g_borderless = borderless;

    if (!glfwInit()) {
        fprintf(stderr, "[NisseAMP] Failed to initialize GLFW\n");
        return 0;
    }

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    if (borderless) {
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    } else {
        glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    }

    g_window = glfwCreateWindow(WIN_BASE_W * g_scale, WIN_BASE_H * g_scale, "NisseAMP", NULL, NULL);
    if (!g_window) {
        fprintf(stderr, "[NisseAMP] Failed to create GLFW window\n");
        glfwTerminate();
        return 0;
    }

    glfwMakeContextCurrent(g_window);
    glfwSwapInterval(1); // Enable vsync (60 FPS)

    glfwSetDropCallback(g_window, drop_callback);
    glfwSetMouseButtonCallback(g_window, mouse_button_callback);
    glfwSetCursorPosCallback(g_window, cursor_pos_callback);
    glfwSetScrollCallback(g_window, scroll_callback);
    glfwSetKeyCallback(g_window, key_callback);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    init_default_vis_colors();

    // Initialize miniaudio playback device
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format = ma_format_f32;
    deviceConfig.playback.channels = 2;
    deviceConfig.sampleRate = 44100;
    deviceConfig.dataCallback = audio_callback;

    if (ma_device_init(NULL, &deviceConfig, &g_audio_device) == MA_SUCCESS) {
        g_device_initialized = 1;
        ma_device_start(&g_audio_device);
    } else {
        fprintf(stderr, "[NisseAMP] Warning: Failed to initialize audio playback device\n");
    }

    return 1;
}

int nisseamp_load_skin(const char* skin_dir) {
    char path[1024];

    snprintf(path, sizeof(path), "%s/MAIN.png", skin_dir);
    tex_main = load_texture_png(path);

    snprintf(path, sizeof(path), "%s/TITLEBAR.png", skin_dir);
    tex_titlebar = load_texture_png(path);

    snprintf(path, sizeof(path), "%s/CBUTTONS.png", skin_dir);
    tex_cbuttons = load_texture_png(path);

    snprintf(path, sizeof(path), "%s/NUMBERS.png", skin_dir);
    tex_numbers = load_texture_png(path);

    snprintf(path, sizeof(path), "%s/TEXT.png", skin_dir);
    tex_text = load_texture_png(path);

    snprintf(path, sizeof(path), "%s/PLAYPAUS.png", skin_dir);
    tex_playpaus = load_texture_png(path);

    snprintf(path, sizeof(path), "%s/MONOSTER.png", skin_dir);
    tex_monoster = load_texture_png(path);

    snprintf(path, sizeof(path), "%s/SHUFREP.png", skin_dir);
    tex_shufrep = load_texture_png(path);

    snprintf(path, sizeof(path), "%s/VOLUME.png", skin_dir);
    tex_volume = load_texture_png(path);

    snprintf(path, sizeof(path), "%s/BALANCE.png", skin_dir);
    tex_balance = load_texture_png(path);

    snprintf(path, sizeof(path), "%s/POSBAR.png", skin_dir);
    tex_posbar = load_texture_png(path);

    // Load VISCOLOR.TXT if present
    snprintf(path, sizeof(path), "%s/VISCOLOR.TXT", skin_dir);
    FILE* fp = fopen(path, "r");
    if (fp) {
        char line[128];
        int color_idx = 0;
        while (fgets(line, sizeof(line), fp) && color_idx < 24) {
            int r, g, b;
            if (sscanf(line, "%d,%d,%d", &r, &g, &b) == 3) {
                vis_colors[color_idx][0] = r / 255.0f;
                vis_colors[color_idx][1] = g / 255.0f;
                vis_colors[color_idx][2] = b / 255.0f;
                color_idx++;
            }
        }
        fclose(fp);
    }

    return (tex_main.loaded && tex_cbuttons.loaded);
}

int nisseamp_load_audio(const char* filepath) {
    if (g_decoder_loaded) {
        ma_decoder_uninit(&g_decoder);
        g_decoder_loaded = 0;
    }

    ma_result res = ma_decoder_init_file(filepath, NULL, &g_decoder);
    if (res != MA_SUCCESS) {
        fprintf(stderr, "[NisseAMP] Failed to open audio file: %s (code %d)\n", filepath, res);
        return 0;
    }

    g_decoder_loaded = 1;
    g_is_synth_mode = 0;
    g_sample_rate = g_decoder.outputSampleRate;
    g_stereo = (g_decoder.outputChannels >= 2);
    g_khz = g_sample_rate / 1000;
    g_kbps = 320; // Default estimate

    ma_decoder_get_length_in_pcm_frames(&g_decoder, &g_total_frames);
    g_total_seconds = (double)g_total_frames / g_sample_rate;
    g_current_frame = 0;
    g_elapsed_seconds = 0.0;

    // Extract filename from path for track title
    const char* filename = strrchr(filepath, '/');
    if (filename) filename++;
    else filename = filepath;

    snprintf(g_title, sizeof(g_title), "1. %s (%d:%02d)", filename, (int)(g_total_seconds / 60.0), (int)fmod(g_total_seconds, 60.0));
    return 1;
}

void nisseamp_play(void) {
    g_play_status = 1;
    if (g_is_synth_mode) {
        g_synth_time = g_elapsed_seconds;
    }
}

void nisseamp_pause(void) {
    if (g_play_status == 1) {
        g_play_status = 2; // Paused
    } else if (g_play_status == 2) {
        g_play_status = 1; // Unpaused
    }
}

void nisseamp_stop(void) {
    g_play_status = 0;
    g_current_frame = 0;
    g_elapsed_seconds = 0.0;
    if (g_decoder_loaded) {
        ma_decoder_seek_to_pcm_frame(&g_decoder, 0);
    }
}

void nisseamp_prev(void) {
    nisseamp_seek(0.0f);
}

void nisseamp_next(void) {
    if (!g_is_synth_mode) {
        nisseamp_play_synth();
    } else {
        nisseamp_stop();
    }
}

void nisseamp_seek(float fraction) {
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;

    if (g_decoder_loaded && g_total_frames > 0) {
        ma_uint64 frame = (ma_uint64)(fraction * g_total_frames);
        ma_decoder_seek_to_pcm_frame(&g_decoder, frame);
        g_current_frame = frame;
        g_elapsed_seconds = (double)g_current_frame / g_sample_rate;
    } else if (g_is_synth_mode) {
        g_synth_time = fraction * g_total_seconds;
        g_elapsed_seconds = g_synth_time;
    }
}

void nisseamp_set_volume(float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    g_volume = volume;
}

void nisseamp_set_balance(float balance) {
    if (balance < -1.0f) balance = -1.0f;
    if (balance > 1.0f) balance = 1.0f;
    g_balance = balance;
}

float nisseamp_get_volume(void) { return g_volume; }
float nisseamp_get_balance(void) { return g_balance; }

void nisseamp_set_shuffle(int enable) { g_shuffle = enable; }
void nisseamp_set_repeat(int enable) { g_repeat = enable; }
int  nisseamp_get_shuffle(void) { return g_shuffle; }
int  nisseamp_get_repeat(void) { return g_repeat; }

void nisseamp_set_vis_mode(int mode) { g_vis_mode = mode % 3; }
int  nisseamp_get_vis_mode(void) { return g_vis_mode; }

void nisseamp_set_time_mode(int mode) { g_time_mode = mode; }
int  nisseamp_get_time_mode(void) { return g_time_mode; }

void nisseamp_set_scale(int scale) {
    if (scale < 1) scale = 1;
    if (scale > 4) scale = 4;
    g_scale = scale;
    int h = g_is_shade ? WIN_SHADE_H : WIN_BASE_H;
    glfwSetWindowSize(g_window, WIN_BASE_W * g_scale, h * g_scale);
}

int  nisseamp_get_scale(void) { return g_scale; }

void nisseamp_toggle_shade(void) {
    g_is_shade = !g_is_shade;
    int h = g_is_shade ? WIN_SHADE_H : WIN_BASE_H;
    glfwSetWindowSize(g_window, WIN_BASE_W * g_scale, h * g_scale);
}

int  nisseamp_get_shade(void) { return g_is_shade; }

void nisseamp_set_title(const char* title) {
    if (title) strncpy(g_title, title, sizeof(g_title) - 1);
}

void nisseamp_show_message(const char* msg, float duration_secs) {
    if (msg) {
        strncpy(g_temp_message, msg, sizeof(g_temp_message) - 1);
        g_temp_message_expire = glfwGetTime() + duration_secs;
    }
}

void nisseamp_set_track_info(int kbps, int khz, int is_stereo) {
    g_kbps = kbps;
    g_khz = khz;
    g_stereo = is_stereo;
}

int    nisseamp_get_status(void) { return g_play_status; }
double nisseamp_get_elapsed(void) { return g_elapsed_seconds; }
double nisseamp_get_total_time(void) { return g_total_seconds; }
int    nisseamp_should_close(void) {
    return g_should_close || glfwWindowShouldClose(g_window);
}

const char* nisseamp_get_dropped_file(void) {
    if (g_has_dropped_file) return g_dropped_file;
    return "";
}

void nisseamp_clear_dropped_file(void) {
    g_has_dropped_file = 0;
    g_dropped_file[0] = '\0';
}

void nisseamp_play_synth(void) {
    g_is_synth_mode = 1;
    g_synth_time = 0.0;
    g_elapsed_seconds = 0.0;
    g_total_seconds = 120.0;
    g_kbps = 256;
    g_khz = 44;
    g_stereo = 1;
    snprintf(g_title, sizeof(g_title), "NisseAMP Retro Chiptune (Am-F-C-G Synth)");
    nisseamp_play();
}

void nisseamp_poll_events(void) {
    glfwPollEvents();
}

// Render the Winamp Main Window
void nisseamp_render(void) {
    int fb_w, fb_h;
    glfwGetFramebufferSize(g_window, &fb_w, &fb_h);
    glViewport(0, 0, fb_w, fb_h);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    int current_h = g_is_shade ? WIN_SHADE_H : WIN_BASE_H;
    glOrtho(0, WIN_BASE_W, current_h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    compute_visualizer();

    // ----------------------------------------------------
    // WINDOWSHADE MODE RENDERING
    // ----------------------------------------------------
    if (g_is_shade) {
        // Active titlebar shade background: (27, 29, 275, 14)
        draw_sprite(&tex_titlebar, 27, 29, 275, 14, 0, 0, 275, 14);

        // Window controls
        // Minimize: (9, 0, 9, 9)
        draw_sprite(&tex_titlebar, (g_pressed_button == 11 ? 9 : 9), (g_pressed_button == 11 ? 9 : 0), 9, 9, 244, 3, 9, 9);
        // Shade button selected: (0, 27, 9, 9), pressed: (9, 27, 9, 9)
        draw_sprite(&tex_titlebar, (g_pressed_button == 12 ? 9 : 0), 27, 9, 9, 254, 3, 9, 9);
        // Close: (18, 0, 9, 9)
        draw_sprite(&tex_titlebar, (g_pressed_button == 13 ? 18 : 18), (g_pressed_button == 13 ? 9 : 0), 9, 9, 264, 3, 9, 9);

        // Mini position seek bar in shade mode (226, 4, 17, 7)
        draw_sprite(&tex_titlebar, 0, 36, 17, 7, 226, 4, 17, 7);
        float progress = (g_total_seconds > 0) ? (float)(g_elapsed_seconds / g_total_seconds) : 0.0f;
        int thumb_x = 226 + (int)(progress * 14.0f);
        draw_sprite(&tex_titlebar, 20, 36, 3, 7, thumb_x, 4, 3, 7);

        // Mini visualizer in shade mode (79, 5, 38, 7)
        if (g_vis_mode == 1) { // Spectrum
            for (int b = 0; b < 19; b++) {
                int vx = 79 + b * 2;
                int vh = (int)((g_band_levels[b] / 15.0f) * 6.0f);
                for (int y = 0; y < vh; y++) {
                    int col_idx = 17 - (y * 2);
                    if (col_idx < 2) col_idx = 2;
                    glColor3fv(vis_colors[col_idx]);
                    glBegin(GL_POINTS);
                    glVertex2f((float)vx, (float)(11 - y));
                    glEnd();
                }
            }
        }

        // Mini time in shade mode at (127, 4)
        int s_mins = (int)(g_elapsed_seconds / 60.0);
        int s_secs = (int)fmod(g_elapsed_seconds, 60.0);
        if (s_mins > 99) s_mins = 99;
        char mini_time[16];
        snprintf(mini_time, sizeof(mini_time), "%02d:%02d", s_mins, s_secs);
        draw_string(mini_time, 127, 4, 5);

        glfwSwapBuffers(g_window);
        return;
    }

    // ----------------------------------------------------
    // FULL MAIN WINDOW RENDERING (275 x 116)
    // ----------------------------------------------------

    // 1. Base Main Window Background (275 x 116)
    draw_sprite(&tex_main, 0, 0, 275, 116, 0, 0, 275, 116);

    // 2. Titlebar (Active): (27, 0, 275, 14)
    draw_sprite(&tex_titlebar, 27, 0, 275, 14, 0, 0, 275, 14);

    // 3. Titlebar Winamp Icon / Options: (0, 0, 9, 9)
    draw_sprite(&tex_titlebar, 0, 0, 9, 9, 6, 3, 9, 9);

    // 4. Window Control Buttons (Min, Shade, Close)
    // Minimize (244, 3)
    draw_sprite(&tex_titlebar, 9, (g_pressed_button == 11 ? 9 : 0), 9, 9, 244, 3, 9, 9);
    // Shade (254, 3)
    draw_sprite(&tex_titlebar, (g_pressed_button == 12 ? 9 : 0), (g_pressed_button == 12 ? 18 : 18), 9, 9, 254, 3, 9, 9);
    // Close (264, 3)
    draw_sprite(&tex_titlebar, 18, (g_pressed_button == 13 ? 9 : 0), 9, 9, 264, 3, 9, 9);

    // 5. Clutter Bar Buttons (O, A, I, D, V) at (10, 22)
    draw_sprite(&tex_titlebar, 304, 0, 8, 43, 10, 22, 8, 43);
    if (g_pressed_button == 14) draw_sprite(&tex_titlebar, 304, 47, 8, 8, 10, 25, 8, 8); // O
    if (g_pressed_button == 15) draw_sprite(&tex_titlebar, 312, 55, 8, 7, 10, 33, 8, 7); // A
    if (g_pressed_button == 16) draw_sprite(&tex_titlebar, 320, 62, 8, 7, 10, 40, 8, 7); // I
    if (g_pressed_button == 17 || g_scale > 1) draw_sprite(&tex_titlebar, 328, 69, 8, 8, 10, 47, 8, 8); // D
    if (g_pressed_button == 18 || g_vis_mode > 0) draw_sprite(&tex_titlebar, 336, 77, 8, 7, 10, 55, 8, 7); // V

    // 6. Playback Status Indicator (Play, Pause, Stop) at (26, 28)
    if (g_play_status == 1) { // Playing
        draw_sprite(&tex_playpaus, 0, 0, 9, 9, 26, 28, 9, 9);
    } else if (g_play_status == 2) { // Paused (blinking)
        if ((int)(glfwGetTime() * 2.0) % 2 == 0) {
            draw_sprite(&tex_playpaus, 9, 0, 9, 9, 26, 28, 9, 9);
        }
    } else { // Stopped
        draw_sprite(&tex_playpaus, 18, 0, 9, 9, 26, 28, 9, 9);
    }

    // 7. Time Display (at 39, 26)
    // Mode: Elapsed or Remaining
    double show_time = g_elapsed_seconds;
    int is_neg = 0;
    if (g_time_mode == 1) { // Remaining
        show_time = g_total_seconds - g_elapsed_seconds;
        if (show_time < 0.0) show_time = 0.0;
        is_neg = 1;
    }

    int mins = (int)(show_time / 60.0);
    int secs = (int)fmod(show_time, 60.0);
    if (mins > 99) mins = 99;

    int blink = (g_play_status == 2 && ((int)(glfwGetTime() * 2.0) % 2 == 0));
    if (!blink) {
        if (is_neg) {
            // Draw minus sign at (38, 32)
            draw_sprite(&tex_numbers, 20, 6, 5, 1, 38, 32, 5, 1);
        }
        draw_number_digit(mins / 10, 48, 26);
        draw_number_digit(mins % 10, 60, 26);
        draw_number_digit(secs / 10, 78, 26);
        draw_number_digit(secs % 10, 90, 26);
    }

    // 8. Visualizer at (24, 43, 75, 16)
    // Clear visualizer box to color 0 (black)
    glColor3fv(vis_colors[0]);
    glBegin(GL_QUADS);
    glVertex2f(24, 43); glVertex2f(99, 43);
    glVertex2f(99, 59); glVertex2f(24, 59);
    glEnd();

    if (g_vis_mode == 1) {
        // Spectrum Analyzer: 19 bars, width 3, gap 1
        for (int b = 0; b < NUM_BANDS; b++) {
            int bar_x = 24 + b * 4;
            int bar_h = (int)g_band_levels[b];
            if (bar_h > 16) bar_h = 16;

            for (int y = 0; y < bar_h; y++) {
                int col_idx = 17 - y;
                if (col_idx < 2) col_idx = 2;
                glColor3fv(vis_colors[col_idx]);
                glBegin(GL_QUADS);
                glVertex2f((float)bar_x, (float)(58 - y));
                glVertex2f((float)(bar_x + 3), (float)(58 - y));
                glVertex2f((float)(bar_x + 3), (float)(58 - y + 1));
                glVertex2f((float)bar_x, (float)(58 - y + 1));
                glEnd();
            }

            // Peak dot
            int peak_y = (int)g_band_peaks[b];
            if (peak_y >= 0 && peak_y < 16) {
                glColor3fv(vis_colors[23]);
                glBegin(GL_QUADS);
                glVertex2f((float)bar_x, (float)(58 - peak_y));
                glVertex2f((float)(bar_x + 3), (float)(58 - peak_y));
                glVertex2f((float)(bar_x + 3), (float)(58 - peak_y + 1));
                glVertex2f((float)bar_x, (float)(58 - peak_y + 1));
                glEnd();
            }
        }
    } else if (g_vis_mode == 2) {
        // Oscilloscope: 75 sample points
        int read_start = (g_vis_write_idx - 75 + VIS_BUF_SIZE) % VIS_BUF_SIZE;
        glColor3fv(vis_colors[18]);
        glBegin(GL_LINE_STRIP);
        for (int x = 0; x < 75; x++) {
            float s = g_vis_pcm[(read_start + x) % VIS_BUF_SIZE];
            float py = 51.0f - s * 7.0f;
            if (py < 43.0f) py = 43.0f;
            if (py > 58.0f) py = 58.0f;
            glVertex2f((float)(24 + x), py);
        }
        glEnd();
    }

    // 9. Marquee Song Title & Temporary Messages at (111, 27)
    // Visible width is 154 pixels (~30 characters of 5px each)
    const char* display_msg = g_title;
    double now = glfwGetTime();
    if (now < g_temp_message_expire && g_temp_message[0] != '\0') {
        display_msg = g_temp_message;
    } else {
        // Scroll title smoothly
        if (g_play_status == 1) {
            g_marquee_scroll_pos += 0.08;
        }
    }

    int title_len = (int)strlen(display_msg);
    char marquee_buf[128];
    if (title_len <= MARQUEE_VISIBLE_CHARS) {
        snprintf(marquee_buf, sizeof(marquee_buf), "%-30s", display_msg);
    } else {
        int offset = (int)g_marquee_scroll_pos % (title_len + 8);
        for (int i = 0; i < MARQUEE_VISIBLE_CHARS; i++) {
            int char_idx = (offset + i) % (title_len + 8);
            if (char_idx < title_len) {
                marquee_buf[i] = display_msg[char_idx];
            } else {
                marquee_buf[i] = ' ';
            }
        }
        marquee_buf[MARQUEE_VISIBLE_CHARS] = '\0';
    }
    draw_string(marquee_buf, 111, 27, MARQUEE_VISIBLE_CHARS);

    // 10. Track Kbps & Khz at (111, 43) and (156, 43)
    if (g_play_status != 0) {
        char kbps_str[16];
        snprintf(kbps_str, sizeof(kbps_str), "%3d", g_kbps);
        draw_string(kbps_str, 111, 43, 3);

        char khz_str[16];
        snprintf(khz_str, sizeof(khz_str), "%2d", g_khz);
        draw_string(khz_str, 156, 43, 2);
    }

    // 11. Mono / Stereo Indicators at (212, 41)
    if (g_stereo) {
        // Stereo lit (0, 0, 29, 12) at (239, 41)
        draw_sprite(&tex_monoster, 0, 0, 29, 12, 239, 41, 29, 12);
        // Mono unlit (29, 12, 27, 12) at (212, 41)
        draw_sprite(&tex_monoster, 29, 12, 27, 12, 212, 41, 27, 12);
    } else {
        // Mono lit (29, 0, 27, 12) at (212, 41)
        draw_sprite(&tex_monoster, 29, 0, 27, 12, 212, 41, 27, 12);
        // Stereo unlit (0, 12, 29, 12) at (239, 41)
        draw_sprite(&tex_monoster, 0, 12, 29, 12, 239, 41, 29, 12);
    }

    // 12. Volume Slider at (107, 57, 68, 13)
    // 28 volume levels, each 15 high in VOLUME.png
    int vol_level = (int)(g_volume * 27.0f);
    if (vol_level < 0) vol_level = 0;
    if (vol_level > 27) vol_level = 27;
    draw_sprite(&tex_volume, 0, vol_level * 15, 68, 13, 107, 57, 68, 13);
    // Volume thumb: normal (15, 422, 14, 11), active (0, 422, 14, 11)
    int vol_thumb_x = 107 + (int)(g_volume * (68 - 14));
    draw_sprite(&tex_volume, (g_dragging_slider == 1 ? 0 : 15), 422, 14, 11, vol_thumb_x, 58, 14, 11);

    // 13. Balance Slider at (177, 57, 38, 13)
    // 28 balance levels, each 15 high at x=9
    int bal_level = (int)(((g_balance + 1.0f) / 2.0f) * 27.0f);
    if (bal_level < 0) bal_level = 0;
    if (bal_level > 27) bal_level = 27;
    draw_sprite(&tex_balance, 9, bal_level * 15, 38, 13, 177, 57, 38, 13);
    // Balance thumb
    int bal_thumb_x = 177 + (int)(((g_balance + 1.0f) / 2.0f) * (38 - 14));
    draw_sprite(&tex_balance, (g_dragging_slider == 2 ? 0 : 15), 422, 14, 11, bal_thumb_x, 58, 14, 11);

    // 14. EQ and PL Buttons at (219, 58) and (242, 58)
    // EQ: off (0, 61, 23, 12), on (0, 73), pressed off (46, 61), pressed on (46, 73)
    int eq_sx = (g_pressed_button == 9) ? 46 : 0;
    int eq_sy = g_eq_open ? 73 : 61;
    draw_sprite(&tex_shufrep, eq_sx, eq_sy, 23, 12, 219, 58, 23, 12);

    // PL: off (23, 61, 23, 12), on (23, 73), pressed off (69, 61), pressed on (69, 73)
    int pl_sx = (g_pressed_button == 10) ? 69 : 23;
    int pl_sy = g_pl_open ? 73 : 61;
    draw_sprite(&tex_shufrep, pl_sx, pl_sy, 23, 12, 242, 58, 23, 12);

    // 15. Position / Seek Bar at (16, 72, 248, 10)
    draw_sprite(&tex_posbar, 0, 0, 248, 10, 16, 72, 248, 10);
    // Seek thumb: (248, 0, 29, 10), active (278, 0, 29, 10)
    float seek_frac = (g_total_seconds > 0.0) ? (float)(g_elapsed_seconds / g_total_seconds) : 0.0f;
    if (seek_frac < 0.0f) seek_frac = 0.0f;
    if (seek_frac > 1.0f) seek_frac = 1.0f;
    int seek_thumb_x = 16 + (int)(seek_frac * (248 - 29));
    int seek_sx = (g_dragging_slider == 3) ? 278 : 248;
    draw_sprite(&tex_posbar, seek_sx, 0, 29, 10, seek_thumb_x, 72, 29, 10);

    // 16. Transport Action Buttons (Prev, Play, Pause, Stop, Next, Eject)
    // Row 0 = normal (y=0, h=18), Row 1 = pressed (y=18, h=18)
    // Prev at (16, 88)
    draw_sprite(&tex_cbuttons, 0, (g_pressed_button == 1 ? 18 : 0), 23, 18, 16, 88, 23, 18);
    // Play at (39, 88)
    draw_sprite(&tex_cbuttons, 23, (g_pressed_button == 2 ? 18 : 0), 23, 18, 39, 88, 23, 18);
    // Pause at (62, 88)
    draw_sprite(&tex_cbuttons, 46, (g_pressed_button == 3 ? 18 : 0), 23, 18, 62, 88, 23, 18);
    // Stop at (85, 88)
    draw_sprite(&tex_cbuttons, 69, (g_pressed_button == 4 ? 18 : 0), 23, 18, 85, 88, 23, 18);
    // Next at (108, 88)
    draw_sprite(&tex_cbuttons, 92, (g_pressed_button == 5 ? 18 : 0), 22, 18, 108, 88, 22, 18);
    // Eject at (136, 89)
    draw_sprite(&tex_cbuttons, 114, (g_pressed_button == 6 ? 16 : 0), 22, 16, 136, 89, 22, 16);

    // 17. Shuffle & Repeat Buttons at (164, 89) and (210, 89)
    // Shuffle: Off (28, 0, 47, 15), On (28, 30), Pressed Off (28, 15), Pressed On (28, 45)
    int shuf_sy = 0;
    if (g_shuffle) shuf_sy = (g_pressed_button == 7 ? 45 : 30);
    else shuf_sy = (g_pressed_button == 7 ? 15 : 0);
    draw_sprite(&tex_shufrep, 28, shuf_sy, 47, 15, 164, 89, 47, 15);

    // Repeat: Off (0, 0, 28, 15), On (0, 30), Pressed Off (0, 15), Pressed On (0, 45)
    int rep_sy = 0;
    if (g_repeat) rep_sy = (g_pressed_button == 8 ? 45 : 30);
    else rep_sy = (g_pressed_button == 8 ? 15 : 0);
    draw_sprite(&tex_shufrep, 0, rep_sy, 28, 15, 210, 89, 28, 15);

    glfwSwapBuffers(g_window);
}

int nisseamp_save_screenshot(const char* filepath) {
    if (!g_window) return 0;
    int fb_w, fb_h;
    glfwGetFramebufferSize(g_window, &fb_w, &fb_h);
    unsigned char* pixels = (unsigned char*)malloc(fb_w * fb_h * 3);
    if (!pixels) return 0;

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, fb_w, fb_h, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    char ppm_path[1024];
    snprintf(ppm_path, sizeof(ppm_path), "%s.ppm", filepath);
    FILE* f = fopen(ppm_path, "wb");
    if (!f) { free(pixels); return 0; }
    fprintf(f, "P6\n%d %d\n255\n", fb_w, fb_h);
    for (int y = fb_h - 1; y >= 0; y--) {
        fwrite(pixels + y * fb_w * 3, 1, fb_w * 3, f);
    }
    fclose(f);
    free(pixels);

    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "sips -s format png '%s' --out '%s' >/dev/null 2>&1 && rm -f '%s'", ppm_path, filepath, ppm_path);
    system(cmd);
    return 1;
}

void nisseamp_cleanup(void) {
    if (g_device_initialized) {
        ma_device_uninit(&g_audio_device);
        g_device_initialized = 0;
    }
    if (g_decoder_loaded) {
        ma_decoder_uninit(&g_decoder);
        g_decoder_loaded = 0;
    }
    if (g_window) {
        glfwDestroyWindow(g_window);
        g_window = NULL;
    }
    glfwTerminate();
}
