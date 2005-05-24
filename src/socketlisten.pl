#!/usr/bin/perl

use strict;
use warnings;

use IO::Socket::UNIX;

my $s = IO::Socket::UNIX->new("../test/bbackupd.sock");
while (1) {
	print $s->getline();
}

exit 0;
