package NisseAMP::Player;

use strict;
use warnings;
use File::Basename qw(basename);
use NisseAMP::Core ();

sub new {
    my ($class, %args) = @_;
    my $self = {
        playlist      => $args{playlist} || [],
        current_index => 0,
        synth_mode    => 0,
        volume        => $args{volume} // 0.85,
        balance       => $args{balance} // 0.0,
        shuffle       => $args{shuffle} // 0,
        repeat        => $args{repeat} // 1,
        vis_mode      => $args{vis_mode} // 1,
        time_mode     => $args{time_mode} // 0,
    };
    bless $self, $class;
    return $self;
}

sub add_track {
    my ($self, $filepath) = @_;
    return unless $filepath && -f $filepath;
    push @{ $self->{playlist} }, $filepath;
    return scalar @{ $self->{playlist} };
}

sub play_track {
    my ($self, $index) = @_;
    my $count = scalar @{ $self->{playlist} };
    return if $count == 0;

    $index = 0 if $index < 0;
    $index = $count - 1 if $index >= $count;
    $self->{current_index} = $index;
    $self->{synth_mode} = 0;

    my $file = $self->{playlist}[$index];
    if (NisseAMP::Core::load_audio($file)) {
        NisseAMP::Core::play();
        return 1;
    }
    return 0;
}

sub play {
    my ($self) = @_;
    my $count = scalar @{ $self->{playlist} };
    if ($count > 0 && NisseAMP::Core::get_status() == 0) {
        $self->play_track($self->{current_index});
    } else {
        NisseAMP::Core::play();
    }
}

sub pause {
    my ($self) = @_;
    NisseAMP::Core::pause();
}

sub stop {
    my ($self) = @_;
    NisseAMP::Core::stop();
}

sub next_track {
    my ($self) = @_;
    my $count = scalar @{ $self->{playlist} };
    if ($count == 0) {
        NisseAMP::Core::play_synth();
        return;
    }

    my $next_idx;
    if (NisseAMP::Core::get_shuffle() && $count > 1) {
        do {
            $next_idx = int(rand($count));
        } while ($next_idx == $self->{current_index});
    } else {
        $next_idx = ($self->{current_index} + 1) % $count;
    }
    $self->play_track($next_idx);
}

sub prev_track {
    my ($self) = @_;
    my $elapsed = NisseAMP::Core::get_elapsed();
    if ($elapsed > 3.0) {
        NisseAMP::Core::seek(0.0);
        return;
    }

    my $count = scalar @{ $self->{playlist} };
    if ($count == 0) {
        NisseAMP::Core::seek(0.0);
        return;
    }

    my $prev_idx = ($self->{current_index} - 1 + $count) % $count;
    $self->play_track($prev_idx);
}

sub is_playing {
    my ($self) = @_;
    return NisseAMP::Core::get_status() == 1;
}

sub is_paused {
    my ($self) = @_;
    return NisseAMP::Core::get_status() == 2;
}

sub is_stopped {
    my ($self) = @_;
    return NisseAMP::Core::get_status() == 0;
}

sub format_time {
    my ($seconds) = @_;
    $seconds = 0 if $seconds < 0;
    my $mins = int($seconds / 60);
    my $secs = int($seconds % 60);
    return sprintf("%02d:%02d", $mins, $secs);
}

1;
