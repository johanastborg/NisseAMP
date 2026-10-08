#!/usr/bin/env perl
use strict;
use warnings;
use Getopt::Long;
use File::Basename qw(dirname);
use Cwd qw(abs_path);

use lib dirname(abs_path(__FILE__)) . '/lib';
use NisseAMP;

my $scale      = 2;
my $borderless = 1;
my $border     = 0;
my $skin_dir   = '';
my $synth      = 0;
my $help       = 0;

GetOptions(
    'scale=i'   => \$scale,
    'border'    => \$border,
    'skin=s'    => \$skin_dir,
    'synth'     => \$synth,
    'help|h'    => \$help,
) or die "Usage: $0 [--scale 1|2|3|4] [--border] [--skin <dir>] [--synth] [audio_file]\n";

if ($help) {
    print <<"HELP";
NisseAMP - Authentic Winamp 2.x Clone in Perl & OpenGL
Usage: perl $0 [options] [audio_file.mp3]

Options:
  --scale <1..4>    Window scaling factor (default: 2 -> 550x232)
  --border          Use standard decorated OS window frame (default: frameless floating)
  --skin <dir>      Path to skin directory containing skin PNG/BMP files
  --synth           Start in procedural retro chiptune synth mode
  --help, -h        Show this help message

Controls:
  * Drag window anywhere by titlebar or background to move
  * Click transport buttons: Prev (|<), Play (>), Pause (||), Stop ([]), Next (>|)
  * Click Eject (^) to toggle synth / open demo
  * Drag Volume, Balance, or Seek sliders
  * Click Time digits to toggle Elapsed vs Remaining time
  * Click Visualizer to toggle Spectrum Analyzer <-> Oscilloscope <-> Off
  * Drag & Drop any MP3, WAV, or FLAC file directly onto the window!

Keyboard Shortcuts:
  Z: Previous track           Up Arrow:    Volume +5%
  X: Play                     Down Arrow:  Volume -5%
  C: Pause / Resume           Left Arrow:  Seek -5s
  V: Stop                     Right Arrow: Seek +5s
  B: Next track               D:           Toggle 2x/3x Double Size
  L: Retro Synth Demo         W:           Toggle Windowshade mode
  S: Toggle Shuffle           1, 2, 3, 4:  Set 1x, 2x, 3x, 4x scale
  R: Toggle Repeat            Space:       Play / Pause toggle
  Esc: Exit
HELP
    exit 0;
}

$borderless = 0 if $border;

my $audio_file = $ARGV[0] || '';
if ($synth) {
    $audio_file = '';
}

my %opts = (
    scale      => $scale,
    borderless => $borderless,
);
$opts{skin_dir}   = $skin_dir if $skin_dir;
$opts{audio_file} = $audio_file if $audio_file;

my $app = NisseAMP->new(%opts);
$app->run();
