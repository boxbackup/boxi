#!/usr/bin/perl
use strict;
use Symbol;

# comment string for various endings
my %comment_chars = ('cpp' => '// ', 'h' => '// ', 'pl' => '# ', 'pm' => '# ', '' => '# ');

# other extensions which need text copying, just to remove the private stuff
my %text_files = ('txt' => 1);

# files which don't get the license added
my %no_license = (); # 'filename' => 1

# ----------------------------------------------

# filled in from the manifest file
my %no_license_dir = ();

# distribution name
my $distribution = $ARGV[0];
die "No distribution name specified on the command line" if $distribution eq '';
my $dist_root = "distribution/$distribution";

# check distribution exists
die "Distribution '$distribution' does not exist" unless -d $dist_root;

# get version
open VERSION,"$dist_root/VERSION.txt" or die "Can't open $dist_root/VERSION.txt";
my $version = <VERSION>;
chomp $version;
my $archive_name = <VERSION>;
chomp $archive_name;
close VERSION;

# consistency check
die "Archive name '$archive_name' is not equal to the distribution name '$distribution'"
	unless $archive_name eq $distribution;

# make initial directory
my $base_name = "$archive_name-$version";
system "rm -rf $base_name";
system "rm $base_name.tgz";
mkdir $base_name,0755;

# get license file
open LICENSE,"$dist_root/LICENSE.txt" or die "Can't open $dist_root/LICENSE.txt";
my $license_f;
read LICENSE,$license_f,100000;
close LICENSE;
my @license = ('distribution '.$base_name,'',split(/\n/,$license_f));

# copy files, make a note of all the modules included
my %modules_included;
my $private_sections_removed = 0;
my $non_distribution_sections_removed = 0;
sub copy_from_list
{
	my $list = $_[0];
	open LIST,$list or die "Can't open $list";
	
	while(<LIST>)
	{
		next unless m/\S/;
		chomp;
		my ($src,$dst) = split /\s+/;
		$dst = $src if $dst eq '';
		if($src eq 'MKDIR')
		{
			# actually we just need to make a directory here
			mkdir "$base_name/$dst",0755;
		}
		elsif($src eq 'NO-LICENSE-IN-DIR')
		{
			# record that this directory shouldn't have the license added
			$no_license_dir{$dst} = 1;
		}
		elsif($src eq 'REPLACE-VERSION-IN')
		{
			replace_version_in($dst);
		}
		elsif(-d $src)
		{
			$modules_included{$_} = 1;
			copy_dir($src,$dst);
		}
		else
		{
			copy_file($src,$dst);
		}
	}
	
	close LIST;
}
copy_from_list("distribution/COMMON-MANIFEST.txt");
copy_from_list("$dist_root/DISTRIBUTION-MANIFEST.txt");

# Copy in the root directory and delete the DISTRIBUTION-MANIFEST file
(system("cp $dist_root/*.* $base_name/") == 0)
	or die "Copy of root extra files failed";
unlink "$base_name/DISTRIBUTION-MANIFEST.txt"
	or die "Delete of DISTRIBUTION-MANIFEST.txt file failed";

# produce a new modules file
my $modules = gensym;
open $modules,"modules.txt" or die "Can't open modules.txt for reading";
open MODULES_OUT,">$base_name/modules.txt";

while(<$modules>)
{
	# skip lines for modules which aren't included
	next if m/\A(\w+\/\w+)\s/ && !exists $modules_included{$1};
	
	# skip private sections
	unless(skip_non_applicable_section($_, $modules, 'modules.txt'))
	{
		# copy line to out files
		print MODULES_OUT
	}
}

close MODULES_OUT;
close $modules;

# report on how many private sections were removed
print "Private sections removed: $private_sections_removed\nNon-distribution sections removed: $non_distribution_sections_removed\n";

# tar it up
system "tar cf - $base_name | gzip -9 - > $base_name.tgz";

sub copy_file
{
	my ($fn,$dst_fn) = @_;
	
	my $ext;
	$ext = $1 if $fn =~ m/\.(\w+)\Z/;
	
	# licenses not used in this directory?
	my $license_in_dir = 1;
	$dst_fn =~ m~\A(.+)/[^/]+?\Z~;
	$license_in_dir = 0 if exists $no_license_dir{$1};
	
	# licensed or not?
	if(exists $comment_chars{$ext} && !exists $no_license{$fn} && $license_in_dir)
	{
		my $b = $comment_chars{$ext};

		# copy as text, inserting license
		my $in = gensym;
		open $in,$fn;
		open OUT,">$base_name/$dst_fn";
		
		my $first = <$in>;
		if($first =~ m/\A#!/)
		{
			print OUT $first;
			$first = '';
		}
		
		# write license
		for(@license)
		{
			print OUT $b,$_,"\n"
		}
		
		if($first ne '')
		{
			print OUT $first;
		}
		
		while(<$in>)
		{
			unless(skip_non_applicable_section($_, $in, $fn))
			{
				print OUT
			}
		}	
		
		close OUT;
		close $in;
	}
	else
	{
		if(exists $text_files{$ext})
		{
			# copy this as text, to remove private stuff
			my $in = gensym;
			open $in,$fn;
			open OUT,">$base_name/$dst_fn";

			while(<$in>)
			{
				unless(skip_non_applicable_section($_, $in, $fn))
				{
					print OUT
				}
			}	

			close OUT;
			close $in;
		}
		else
		{
			# copy as binary
			system 'cp',$fn,"$base_name/$dst_fn"
		}
	}
	
	# make sure perl scripts are marked as executable, and other things aren't
	if($ext eq 'pl' || $ext eq '')
	{
		system 'chmod','a+x',"$base_name/$dst_fn"
	}
	else
	{
		system 'chmod','a-x',"$base_name/$dst_fn"
	}
}

sub skip_non_applicable_section
{
	my ($l, $filehandle, $filename) = @_;
	if($l =~ m/BOX_PRIVATE_BEGIN/)
	{
		# skip private section
		print "Removing private section from $filename\n";
		$private_sections_removed++;
		while(<$filehandle>) {last if m/BOX_PRIVATE_END/}
		
		# skipped something
		return 1;
	}
	elsif($l =~ m/IF_DISTRIBUTION\((.+?)\)/)
	{
		# which distributions does this apply to?
		my $applies = 0;
		for(split /,/,$1)
		{
			$applies = 1 if $_ eq $distribution
		}
		unless($applies)
		{
			# skip section?
			print "Removing distribution specific section from $filename\n";
			$non_distribution_sections_removed++;
			while(<$filehandle>) {last if m/END_IF_DISTRIBUTION/}
		}
		# hide this line
		return 1;
	}
	elsif($l =~ m/END_IF_DISTRIBUTION/)
	{
		# hide these lines
		return 1;
	}
	else
	{
		# no skipping, return this line
		return 0;
	}
}

sub copy_dir
{
	my ($dir,$dst_dir) = @_;

	# copy an entire directory... first make sure it exists
	my @n = split /\//,$dst_dir;
	my $d = $base_name;
	for(@n)
	{
		$d .= '/';
		$d .= $_;
		mkdir $d,0755;
	}
	
	# then do each of the files within in
	opendir DIR,$dir;
	my @items = readdir DIR;
	closedir DIR;
	
	for(@items)
	{
		next if m/\A\./;
		next if m/\A_/;
		next if m/\AMakefile\Z/;
		next if m/\Aautogen/;
		next if !-f "$dir/$_";
		
		copy_file("$dir/$_","$dst_dir/$_");
	}
}

sub replace_version_in
{
	my ($file) = @_;
	
	my $fn = $base_name . '/' . $file;
	open IN,$fn or die "Can't open $fn";
	open OUT,'>'.$fn.'.new' or die "Can't open $fn.new for writing";
	
	while(<IN>)
	{
		s/###DISTRIBUTION-VERSION-NUMBER###/$version/g;
		print OUT
	}
	
	close OUT;
	close IN;

	rename($fn.'.new', $fn) or die "Can't rename in place $fn";
}

