#!/usr/bin/perl
#
# Author:
#     Michael J. Walsh
#     based on the original by Sergio Conde <skgsergio@gmail.com>
#
# License:
#     This script is part of omxplayer and it should be
# distributed under the same license.
#

use strict;

my $date = run("date -R 2> /dev/null");
my $hash = "UNKNOWN";
my $branch = "UNKNOWN";
my $repo = "UNKNOWN";

my $ref = run("git symbolic-ref -q HEAD 2> /dev/null");
if($? == 0) {
	my $hash = run("git rev-parse --short $ref 2> /dev/null");
	my $branch = $ref;
	$branch =~ s|^refs/heads/||;

	my $upstream = run("git for-each-ref --format='%(upstream:short)' $ref 2> /dev/null");
	if($upstream ne "") {
		my $short_repo = $upstream;
		$short_repo =~ s|/$branch$||;
		$repo = run("git config remote.$short_repo.url");
	}

	if(no_change()) {
		# no need to update version.h
		exit 0;
	}
}

open(my $w, '>', 'version.h') || exit 1;

print $w <<"OUT";
#ifndef __VERSION_H__
#define __VERSION_H__
#define VERSION_DATE "$date"
#define VERSION_HASH "$hash"
#define VERSION_BRANCH "$branch"
#define VERSION_REPO "$repo"
#endif
OUT
;

exit 0;

sub run {
	my $o = readpipe($_[0]);
	chomp $o;
	return $o;
}

sub no_change {
	open(my $r, '<', 'version.h') || return 0;
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
