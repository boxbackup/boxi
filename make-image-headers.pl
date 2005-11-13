#!/usr/bin/perl

use warnings;
use strict;

use IO::File;
use Fcntl qw(SEEK_SET SEEK_END);

my $linelength = 8;

foreach my $filename ("empty16.png", "tick16.png", "tickgrey16.png", 
	"cross16.png", "crossgrey16.png", "plus16.png", "plusgrey16.png",
	"sametime16.png", "hourglass16.png", "equal16.png", "notequal16.png", 
	"unknown16.png", "oldfile16.png", "exclam16.png", "folder16.png", 
	"partial16.png", "alien16.png", "tick16a.png", "cross16a.png") 
{
	my $ifh = IO::File->new("< images/$filename") 
		or die "src/$filename: $!";

	$ifh->seek(0, SEEK_END);
	my $length = $ifh->tell();
	$ifh->seek(0, SEEK_SET);

	my $data;
	$ifh->read($data, $length);
	$ifh->close();

	my $ofh = IO::File->new("> src/$filename.c") or die "$!";
	my $c_name = $filename;
	$c_name =~ tr|.|_|;

	$ofh->write("#include <StaticImage.h>\n\n");

	$ofh->write("static const unsigned char data[] = {\n");
	my @values = unpack("C*", $data);

	while (@values) {
		my @line = splice(@values, 0, $linelength);
		my $output = join("", map { sprintf "0x%02x, ", $_ } @line);
		$ofh->write("\t$output\n");
	}

	$ofh->write("};\n");

	$ofh->write("const struct StaticImage ${c_name} = {\n");
	$ofh->write("\t.size = $length,\n");
	$ofh->write("\t.data = data\n");
	$ofh->write("};\n");
	$ofh->close();
}

exit 0;
