package NisseAMP::Core;

use strict;
use warnings;
use Cwd qw(abs_path);
use File::Basename qw(dirname);
use Exporter 'import';

our @EXPORT_OK = qw(
    init load_skin load_audio
    play pause stop prev next seek
    set_volume set_balance get_volume get_balance
    set_shuffle set_repeat get_shuffle get_repeat
    set_vis_mode get_vis_mode
    set_time_mode get_time_mode
    set_scale get_scale
    toggle_shade get_shade
    set_title show_message set_track_info
    get_status get_elapsed get_total_time
    should_close get_dropped_file clear_dropped_file
    play_synth poll_events render cleanup save_screenshot
);

our %EXPORT_TAGS = (all => \@EXPORT_OK);

# Auto-compile src/nisseamp_core.o if missing or out of date
my $root_dir = dirname(dirname(dirname(abs_path(__FILE__))));
my $src_dir  = "$root_dir/src";
my $obj_file = "$src_dir/nisseamp_core.o";
my $c_file   = "$src_dir/nisseamp_core.c";
my $h_file   = "$src_dir/nisseamp_core.h";

if (!-f $obj_file || -M $c_file < -M $obj_file || -M $h_file < -M $obj_file) {
    print "[NisseAMP] Compiling native C core...\n";
    my $cmd = "clang -c $c_file -I/opt/homebrew/opt/glfw/include -I$src_dir -O2 -Wall -Wextra -Wno-unused-parameter -o $obj_file";
    my $rc = system($cmd);
    die "[NisseAMP] Failed to compile $c_file (exit code $rc)" if $rc != 0;
}

my $inline_dir = "$root_dir/_Inline";
mkdir $inline_dir unless -d $inline_dir;

use Inline;
Inline->bind(C => q{
#include "nisseamp_core.h"

int pl_init(int scale, int borderless) { return nisseamp_init(scale, borderless); }
int pl_load_skin(const char* dir)      { return nisseamp_load_skin(dir); }
int pl_load_audio(const char* file)    { return nisseamp_load_audio(file); }

void pl_play()                         { nisseamp_play(); }
void pl_pause()                        { nisseamp_pause(); }
void pl_stop()                         { nisseamp_stop(); }
void pl_prev()                         { nisseamp_prev(); }
void pl_next()                         { nisseamp_next(); }
void pl_seek(double frac)              { nisseamp_seek((float)frac); }

void pl_set_volume(double v)           { nisseamp_set_volume((float)v); }
void pl_set_balance(double b)          { nisseamp_set_balance((float)b); }
double pl_get_volume()                 { return (double)nisseamp_get_volume(); }
double pl_get_balance()                { return (double)nisseamp_get_balance(); }

void pl_set_shuffle(int s)             { nisseamp_set_shuffle(s); }
void pl_set_repeat(int r)              { nisseamp_set_repeat(r); }
int  pl_get_shuffle()                  { return nisseamp_get_shuffle(); }
int  pl_get_repeat()                   { return nisseamp_get_repeat(); }

void pl_set_vis_mode(int m)            { nisseamp_set_vis_mode(m); }
int  pl_get_vis_mode()                 { return nisseamp_get_vis_mode(); }

void pl_set_time_mode(int m)           { nisseamp_set_time_mode(m); }
int  pl_get_time_mode()                { return nisseamp_get_time_mode(); }

void pl_set_scale(int s)               { nisseamp_set_scale(s); }
int  pl_get_scale()                    { return nisseamp_get_scale(); }

void pl_toggle_shade()                 { nisseamp_toggle_shade(); }
int  pl_get_shade()                    { return nisseamp_get_shade(); }

void pl_set_title(const char* t)       { nisseamp_set_title(t); }
void pl_show_message(const char* m, double sec) { nisseamp_show_message(m, (float)sec); }
void pl_set_track_info(int kbps, int khz, int stereo) { nisseamp_set_track_info(kbps, khz, stereo); }

int    pl_get_status()                 { return nisseamp_get_status(); }
double pl_get_elapsed()                { return nisseamp_get_elapsed(); }
double pl_get_total_time()             { return nisseamp_get_total_time(); }
int    pl_should_close()               { return nisseamp_should_close(); }

const char* pl_get_dropped_file()      { return nisseamp_get_dropped_file(); }
void pl_clear_dropped_file()           { nisseamp_clear_dropped_file(); }

void pl_play_synth()                   { nisseamp_play_synth(); }
void pl_poll_events()                  { nisseamp_poll_events(); }
void pl_render()                       { nisseamp_render(); }
int  pl_save_screenshot(const char* f) { return nisseamp_save_screenshot(f); }
void pl_cleanup()                      { nisseamp_cleanup(); }

},
INC      => "-I$src_dir -I/opt/homebrew/opt/glfw/include",
MYEXTLIB => $obj_file,
LIBS     => "-L/opt/homebrew/opt/glfw/lib -lglfw -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -framework CoreAudio -framework AudioToolbox -framework CoreFoundation",
DIRECTORY => "$root_dir/_Inline"
);

# Exported wrappers
sub init             { pl_init(@_) }
sub load_skin        { pl_load_skin(@_) }
sub load_audio       { pl_load_audio(@_) }
sub play             { pl_play() }
sub pause            { pl_pause() }
sub stop             { pl_stop() }
sub prev             { pl_prev() }
sub next             { pl_next() }
sub seek             { pl_seek(@_) }
sub set_volume       { pl_set_volume(@_) }
sub set_balance      { pl_set_balance(@_) }
sub get_volume       { pl_get_volume() }
sub get_balance      { pl_get_balance() }
sub set_shuffle      { pl_set_shuffle(@_) }
sub set_repeat       { pl_set_repeat(@_) }
sub get_shuffle      { pl_get_shuffle() }
sub get_repeat       { pl_get_repeat() }
sub set_vis_mode     { pl_set_vis_mode(@_) }
sub get_vis_mode     { pl_get_vis_mode() }
sub set_time_mode    { pl_set_time_mode(@_) }
sub get_time_mode    { pl_get_time_mode() }
sub set_scale        { pl_set_scale(@_) }
sub get_scale        { pl_get_scale() }
sub toggle_shade     { pl_toggle_shade() }
sub get_shade        { pl_get_shade() }
sub set_title        { pl_set_title(@_) }
sub show_message     { pl_show_message(@_) }
sub set_track_info   { pl_set_track_info(@_) }
sub get_status       { pl_get_status() }
sub get_elapsed      { pl_get_elapsed() }
sub get_total_time   { pl_get_total_time() }
sub should_close     { pl_should_close() }
sub get_dropped_file { pl_get_dropped_file() }
sub clear_dropped_file { pl_clear_dropped_file() }
sub play_synth       { pl_play_synth() }
sub poll_events      { pl_poll_events() }
sub render           { pl_render() }
sub save_screenshot  { pl_save_screenshot(@_) }
sub cleanup          { pl_cleanup() }

1;
