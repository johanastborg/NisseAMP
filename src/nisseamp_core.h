#ifndef NISSEAMP_CORE_H
#define NISSEAMP_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

// Initialize GLFW, OpenGL window, textures, and audio engine
// scale: 1, 2, 3, 4 (default 2)
// borderless: 1 for authentic Winamp floating window, 0 for OS frame
int nisseamp_init(int scale, int borderless);

// Load skin from directory containing PNG/BMP files
int nisseamp_load_skin(const char* skin_dir);

// Load audio file (MP3, WAV, FLAC)
int nisseamp_load_audio(const char* filepath);

// Playback controls
void nisseamp_play(void);
void nisseamp_pause(void);
void nisseamp_stop(void);
void nisseamp_prev(void);
void nisseamp_next(void);
void nisseamp_seek(float fraction); // 0.0 to 1.0

// Volume & balance
void nisseamp_set_volume(float volume);   // 0.0 to 1.0
void nisseamp_set_balance(float balance); // -1.0 (L) to +1.0 (R)
float nisseamp_get_volume(void);
float nisseamp_get_balance(void);

// Modes & Toggles
void nisseamp_set_shuffle(int enable);
void nisseamp_set_repeat(int enable);
int  nisseamp_get_shuffle(void);
int  nisseamp_get_repeat(void);

void nisseamp_set_vis_mode(int mode); // 0=off, 1=spectrum, 2=oscilloscope
int  nisseamp_get_vis_mode(void);

void nisseamp_set_time_mode(int mode); // 0=elapsed, 1=remaining
int  nisseamp_get_time_mode(void);

void nisseamp_set_scale(int scale);
int  nisseamp_get_scale(void);

void nisseamp_toggle_shade(void);
int  nisseamp_get_shade(void);

// Text & Info
void nisseamp_set_title(const char* title);
void nisseamp_show_message(const char* msg, float duration_secs);
void nisseamp_set_track_info(int kbps, int khz, int is_stereo);

// State query
int    nisseamp_get_status(void); // 0=stopped, 1=playing, 2=paused
double nisseamp_get_elapsed(void);
double nisseamp_get_total_time(void);
int    nisseamp_should_close(void);

// Drag & drop check (returns filename or empty string)
const char* nisseamp_get_dropped_file(void);
void nisseamp_clear_dropped_file(void);

// Procedural synth toggle
void nisseamp_play_synth(void);

// Screenshot
int nisseamp_save_screenshot(const char* filepath);

// Event loop tick & render
void nisseamp_poll_events(void);
void nisseamp_render(void);

// Cleanup
void nisseamp_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // NISSEAMP_CORE_H
