package NisseAMP::Skin;

use strict;
use warnings;
use File::Spec;
use NisseAMP::Core qw(load_skin);

sub new {
    my ($class, %args) = @_;
    my $self = {
        skin_dir => $args{skin_dir} || 'assets/skin',
        loaded   => 0,
    };
    bless $self, $class;
    return $self;
}

sub load {
    my ($self, $dir) = @_;
    $dir ||= $self->{skin_dir};
    
    if (!-d $dir) {
        warn "[NisseAMP::Skin] Skin directory not found: $dir\n";
        return 0;
    }

    my $res = NisseAMP::Core::load_skin($dir);
    if ($res) {
        $self->{skin_dir} = $dir;
        $self->{loaded}   = 1;
        print "[NisseAMP::Skin] Loaded skin from $dir\n";
        return 1;
    } else {
        warn "[NisseAMP::Skin] Failed to load skin textures from $dir\n";
        return 0;
    }
}

sub is_loaded {
    my ($self) = @_;
    return $self->{loaded};
}

1;
