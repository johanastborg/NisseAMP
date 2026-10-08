package NisseAMP;

use strict;
use warnings;
use File::Basename qw(dirname basename);
use Cwd qw(abs_path);
use Time::HiRes qw(sleep time);

use NisseAMP::Core ();
use NisseAMP::Skin;
use NisseAMP::Player;

our $VERSION = '1.0.0';

sub new {
    my ($class, %args) = @_;
    my $root = dirname(dirname(abs_path(__FILE__)));

    my $self = {
        root_dir   => $root,
        scale      => $args{scale} // 2,
        borderless => $args{borderless} // 1,
        skin_dir   => $args{skin_dir} || "$root/assets/skin",
        audio_file => $args{audio_file},
        player     => undef,
        skin       => undef,
        running    => 0,
    };
    bless $self, $class;

    $self->{skin} = NisseAMP::Skin->new(skin_dir => $self->{skin_dir});
    $self->{player} = NisseAMP::Player->new();

    return $self;
}

sub init {
    my ($self) = @_;
    print "[NisseAMP] Initializing NisseAMP v$VERSION...\n";

    if (!NisseAMP::Core::init($self->{scale}, $self->{borderless})) {
        die "[NisseAMP] Fatal: OpenGL / GLFW initialization failed\n";
    }

    if (!$self->{skin}->load($self->{skin_dir})) {
        die "[NisseAMP] Fatal: Failed to load Winamp skin from $self->{skin_dir}\n";
    }

    # Queue audio file if provided, otherwise load demo track
    my $demo_track = "$self->{root_dir}/assets/audio/llama-2.91.mp3";
    if ($self->{audio_file} && -f $self->{audio_file}) {
        $self->{player}->add_track($self->{audio_file});
        $self->{player}->play_track(0);
    } elsif (-f $demo_track) {
        $self->{player}->add_track($demo_track);
        $self->{player}->play_track(0);
    } else {
        # Fall back to procedural synth
        NisseAMP::Core::play_synth();
    }

    $self->{running} = 1;
    print "[NisseAMP] Ready! Window scale: ${ \$self->{scale} }x\n";
    print "[NisseAMP] Keyboard shortcuts:\n";
    print "   Z: Prev | X: Play | C: Pause | V: Stop | B: Next | L: Synth Demo\n";
    print "   D: Toggle 2x/3x Size | W: Windowshade Mode | Up/Down: Volume | Left/Right: Seek\n";
    print "   Drag & drop any MP3/WAV/FLAC onto the window to play!\n";
    return 1;
}

sub tick {
    my ($self) = @_;
    NisseAMP::Core::poll_events();

    # Check for dragged & dropped files from Finder
    my $dropped = NisseAMP::Core::get_dropped_file();
    if ($dropped && -f $dropped) {
        print "[NisseAMP] Loading dropped file: $dropped\n";
        $self->{player}->add_track($dropped);
        my $new_idx = scalar(@{ $self->{player}->{playlist} }) - 1;
        $self->{player}->play_track($new_idx);
        NisseAMP::Core::clear_dropped_file();
    }
}

sub render {
    my ($self) = @_;
    NisseAMP::Core::render();
}

sub should_close {
    my ($self) = @_;
    return NisseAMP::Core::should_close();
}

sub run {
    my ($self) = @_;
    $self->init() unless $self->{running};

    while (!$self->should_close) {
        $self->tick();
        $self->render();
    }

    $self->cleanup();
}

sub cleanup {
    my ($self) = @_;
    if ($self->{running}) {
        NisseAMP::Core::cleanup();
        $self->{running} = 0;
        print "[NisseAMP] Cleaned up and exited cleanly.\n";
    }
}

1;
