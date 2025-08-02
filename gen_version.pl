#!/usr/bin/perl
#
# Author:
#     Michael J. Walsh
#     based on the original by Sergio Conde <skgsergio@gmail.com>
#
# License:
#     This script is part of omxplayer and it should be
#     distributed under the same license.
#

use strict;
use warnings;

my $date = run("date -R");
my $hash = "UNKNOWN";
my $branch = "UNKNOWN";
my $repo = "UNKNOWN";

sub main {
    my $ref = run("git symbolic-ref -q HEAD");

    $hash = run("git rev-parse --short $ref");
    $branch = $ref;
    $branch =~ s|^refs/heads/||;

    my $upstream = run("git for-each-ref --format='%(upstream:short)' $ref");
    if($upstream ne "") {
        my $short_repo = $upstream;
        $short_repo =~ s|/$branch$||;
        $repo = run("git config remote.$short_repo.url");
    }

    if(no_change()) {
        # no need to update version_info.h
        exit 0;
    } else {
        generate_version_info_header();

        # if there are command line args execute then as a command
        if(@ARGV > 0) {
            compile_version_object();
            exit 1;
        } else {
            exit 0;
        }
    }
}

main();

sub generate_version_info_header {
    open(my $w, '>', 'version_info.h') || exit 1;

    print $w <<"OUT";
#pragma once
#define VERSION_DATE "$date"
#define VERSION_HASH "$hash"
#define VERSION_BRANCH "$branch"
#define VERSION_REPO "$repo"
OUT
;

}

sub compile_version_object {
    # compile a new version.o since the version has changed
    print join(" ", @ARGV) , "\n";
    exec @ARGV;
}

sub run {
    my $o = readpipe($_[0] . " 2> /dev/null");
    die "command failed: $_[0]" if $? != 0;
    chomp $o;
    return $o;
}

sub no_change {
    open(my $r, '<', 'version_info.h') || return 0;
    my $content = join '', <$r>;
    close $r;

    if($content =~ m/VERSION_HASH "([^"]+)"/) {
        return 0 if $1 ne $hash;
    }

    if($content =~ m/VERSION_BRANCH "([^"]+)"/) {
        return 0 if $1 ne $branch;
    }

    if($content =~ m/VERSION_REPO "([^"]+)"/) {
        return 0 if $1 ne $repo;
    }

    return 1;
}
