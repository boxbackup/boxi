#!/usr/bin/perl
use strict;

use lib "../../infrastructure";
use BoxPlatform;

# Make protocol C++ classes from a protocol description file

# built in type info (values are is basic type, C++ typename)
# may get stuff added to it later if protocol uses extra types
my %translate_type_info =
(
	'int64' => [1, 'int64_t'],
	'int32' => [1, 'int32_t'],
	'int16' => [1, 'int16_t'],
	'int8' => [1, 'int8_t'],
	'bool' => [1, 'bool'],
	'string' => [0, 'std::string']
);

# built in instructions for logging various types
# may be added to
my %log_display_types = 
(
	'int64' => ['0x%llx', 'VAR'],
	'int32' => ['0x%x', 'VAR'],
	'int16' => ['0x%x', 'VAR'],
	'int8' => ['0x%x', 'VAR'],
	'bool' => ['%s', '((VAR)?"true":"false")'],
	'string' => ['%s', 'VAR.c_str()']
);



my ($type, $file) = @ARGV;

if($type ne 'Server' && $type ne 'Client')
{
	die "Neither Server or Client is specified on command line\n";
}

open IN, $file or die "Can't open input file $file\n";

print "Making $type protocol classes from $file...\n";

my @extra_header_files;

my $implement_syslog = 0;
my $implement_filelog = 0;

# read attributes
my %attr;
while(<IN>)
{
	# get and clean line
	my $l = $_; $l =~ s/#.*\Z//; $l =~ s/\A\s+//; $l =~ s/\s+\Z//; next unless $l =~ m/\S/;
	
	last if $l eq 'BEGIN_OBJECTS';
	
	my ($k,$v) = split /\s+/,$l,2;
	
	if($k eq 'ClientType')
	{
		add_type($v) if $type eq 'Client';
	}
	elsif($k eq 'ServerType')
	{
		add_type($v) if $type eq 'Server';
	}
	elsif($k eq 'ImplementLog')
	{
		my ($log_if_type,$log_type) = split /\s+/,$v;
		if($type eq $log_if_type)
		{
			if($log_type eq 'syslog')
			{
				$implement_syslog = 1;
			}
			elsif($log_type eq 'file')
			{
				$implement_filelog = 1;
			}
			else
			{
				printf("ERROR: Unknown log type for implementation: $log_type\n");
				exit(1);
			}
		}
	}
	elsif($k eq 'LogTypeToText')
	{
		my ($log_if_type,$type_name,$printf_format,$arg_template) = split /\s+/,$v;
		if($type eq $log_if_type)
		{
			$log_display_types{$type_name} = [$printf_format,$arg_template]
		}
	}
	else
	{
		$attr{$k} = $v;
	}
}

sub add_type
{
	my ($protocol_name, $cpp_name, $header_file) = split /\s+/,$_[0];
	
	$translate_type_info{$protocol_name} = [0, $cpp_name];
	push @extra_header_files, $header_file;
}

# check attributes
for(qw/Name ServerContextClass IdentString/)
{
	if(!exists $attr{$_})
	{
		die "Attribute $_ is required, but not specified\n";
	}
}

my $protocol_name = $attr{'Name'};
my ($context_class, $context_class_inc) = split /\s+/,$attr{'ServerContextClass'};
my $ident_string = $attr{'IdentString'};

my $current_cmd = '';
my %cmd_contents;
my %cmd_attributes;
my %cmd_constants;
my %cmd_id;
my @cmd_list;

# read in the command definitions
while(<IN>)
{
	# get and clean line
	my $l = $_; $l =~ s/#.*\Z//; $l =~ s/\s+\Z//; next unless $l =~ m/\S/;
	
	# definitions or new command thing?
	if($l =~ m/\A\s+/)
	{
		die "No command defined yet" if $current_cmd eq '';
	
		# definition of component
		$l =~ s/\A\s+//;
		
		my ($type,$name,$value) = split /\s+/,$l;
		if($type eq 'CONSTANT')
		{
			push @{$cmd_constants{$current_cmd}},"$name = $value"
		}
		else
		{
			push @{$cmd_contents{$current_cmd}},$type,$name;
		}
	}
	else
	{
		# new command
		my ($name,$id,@attributes) = split /\s+/,$l;
		$cmd_attributes{$name} = [@attributes];
		$cmd_id{$name} = int($id);
		$current_cmd = $name;
		push @cmd_list,$name;
	}
}

close IN;



# open files
my $h_filename = 'autogen_'.$protocol_name.'Protocol'.$type.'.h';
open CPP,'>autogen_'.$protocol_name.'Protocol'.$type.'.cpp';
open H,">$h_filename";

print CPP <<__E;

// Auto-generated file -- do not edit

#include "Box.h"
#include "$h_filename"
#include "IOStream.h"

__E

if($implement_syslog)
{
	print H <<EOF;
#ifndef WIN32
#include <syslog.h>
#endif
EOF
}


my $guardname = uc 'AUTOGEN_'.$protocol_name.'Protocol'.$type.'_H';
print H <<__E;

// Auto-generated file -- do not edit

#ifndef $guardname
#define $guardname

#include "Protocol.h"
#include "ProtocolObject.h"
#include "ServerException.h"

class IOStream;

__E

if($implement_filelog)
{
	print H qq~#include <stdio.h>\n~;
}

# extra headers
for(@extra_header_files)
{
	print H qq~#include "$_"\n~
}
print H "\n";

if($type eq 'Server')
{
	# need utils file for the server
	print H '#include "Utils.h"',"\n\n"
}


my $derive_objects_from = 'ProtocolObject';
my $objects_extra_h = '';
my $objects_extra_cpp = '';
if($type eq 'Server')
{
	# define the context
	print H "class $context_class;\n\n";
	print CPP "#include \"$context_class_inc\"\n\n";
	
	# change class we derive the objects from
	$derive_objects_from = $protocol_name.'ProtocolObject';
	
	$objects_extra_h = <<__E;
	virtual std::auto_ptr<ProtocolObject> DoCommand(${protocol_name}ProtocolServer &rProtocol, $context_class &rContext);
__E
	$objects_extra_cpp = <<__E;
std::auto_ptr<ProtocolObject> ${derive_objects_from}::DoCommand(${protocol_name}ProtocolServer &rProtocol, $context_class &rContext)
{
	THROW_EXCEPTION(ConnectionException, Conn_Protocol_TriedToExecuteReplyCommand)
}
__E
}

print CPP qq~#include "MemLeakFindOn.h"\n~;

if($type eq 'Client' && ($implement_syslog || $implement_filelog))
{
	# change class we derive the objects from
	$derive_objects_from = $protocol_name.'ProtocolObjectCl';
}
if($implement_syslog)
{
	$objects_extra_h .= <<__E;
	virtual void LogSysLog(const char *Action) const = 0;
__E
}
if($implement_filelog)
{
	$objects_extra_h .= <<__E;
	virtual void LogFile(const char *Action, FILE *file) const = 0;
__E
}

if($derive_objects_from ne 'ProtocolObject')
{	
	# output a definition for the protocol object derviced class
	print H <<__E;
class ${protocol_name}ProtocolServer;
	
class $derive_objects_from : public ProtocolObject
{
public:
	$derive_objects_from();
	virtual ~$derive_objects_from();
	$derive_objects_from(const $derive_objects_from &rToCopy);
	
$objects_extra_h
};
__E

	# and some cpp definitions
	print CPP <<__E;
${derive_objects_from}::${derive_objects_from}()
{
}
${derive_objects_from}::~${derive_objects_from}()
{
}
${derive_objects_from}::${derive_objects_from}(const $derive_objects_from &rToCopy)
{
}
$objects_extra_cpp
__E
}



my $classname_base = $protocol_name.'Protocol'.$type;

# output the classes
for my $cmd (@cmd_list)
{
	print H <<__E;
class $classname_base$cmd : public $derive_objects_from
{
public:
	$classname_base$cmd();
	$classname_base$cmd(const $classname_base$cmd &rToCopy);
	~$classname_base$cmd();
	int GetType() const;
	enum
	{
		TypeID = $cmd_id{$cmd}
	};
__E
	# constants
	if(exists $cmd_constants{$cmd})
	{
		print H "\tenum\n\t{\n\t\t";
		print H join(",\n\t\t",@{$cmd_constants{$cmd}});
		print H "\n\t};\n";
	}
	# flags
	if(obj_is_type($cmd,'EndsConversation'))
	{
		print H "\tbool IsConversationEnd() const;\n";
	}
	if(obj_is_type($cmd,'IsError'))
	{
		print H "\tbool IsError(int &rTypeOut, int &rSubTypeOut) const;\n";
	}
	if($type eq 'Server' && obj_is_type($cmd, 'Command'))
	{
		print H "\tstd::auto_ptr<ProtocolObject> DoCommand(${protocol_name}ProtocolServer &rProtocol, $context_class &rContext); // IMPLEMENT THIS\n"
	}

	# want to be able to read from streams?
	my $read_from_streams = (obj_is_type($cmd,'Command') && $type eq 'Server') || (obj_is_type($cmd,'Reply') && $type eq 'Client');
	my $write_to_streams = (obj_is_type($cmd,'Command') && $type eq 'Client') || (obj_is_type($cmd,'Reply') && $type eq 'Server');
	
	if($read_from_streams)
	{
		print H "\tvoid SetPropertiesFromStreamData(Protocol &rProtocol);\n";
		
		# write Get functions
		for(my $x = 0; $x < $#{$cmd_contents{$cmd}}; $x+=2)
		{
			my ($ty,$nm) = (${$cmd_contents{$cmd}}[$x], ${$cmd_contents{$cmd}}[$x+1]);
			
			print H "\t".translate_type_to_arg_type($ty)." Get$nm() {return m$nm;}\n";
		}
	}
	my $param_con_args = '';
	if($write_to_streams)
	{
		# extra constructor?
		if($#{$cmd_contents{$cmd}} >= 0)
		{
			my @a;
			for(my $x = 0; $x < $#{$cmd_contents{$cmd}}; $x+=2)
			{
				my ($ty,$nm) = (${$cmd_contents{$cmd}}[$x], ${$cmd_contents{$cmd}}[$x+1]);

				push @a,translate_type_to_arg_type($ty)." $nm";
			}		
			$param_con_args = join(', ',@a);
			print H "\t$classname_base$cmd(".$param_con_args.");\n";
		}
		print H "\tvoid WritePropertiesToStreamData(Protocol &rProtocol) const;\n";
		# set functions
		for(my $x = 0; $x < $#{$cmd_contents{$cmd}}; $x+=2)
		{
			my ($ty,$nm) = (${$cmd_contents{$cmd}}[$x], ${$cmd_contents{$cmd}}[$x+1]);
			
			print H "\tvoid Set$nm(".translate_type_to_arg_type($ty)." $nm) {m$nm = $nm;}\n";
		}
	}
	
	if($implement_syslog)
	{
		print H "\tvirtual void LogSysLog(const char *Action) const;\n";
	}
	if($implement_filelog)
	{
		print H "\tvirtual void LogFile(const char *Action, FILE *file) const;\n";
	}

	
	# write member variables and setup for cpp file
	my @def_constructor_list;
	my @copy_constructor_list;
	my @param_constructor_list;
	
	print H "private:\n";
	for(my $x = 0; $x < $#{$cmd_contents{$cmd}}; $x+=2)
	{
		my ($ty,$nm) = (${$cmd_contents{$cmd}}[$x], ${$cmd_contents{$cmd}}[$x+1]);

		print H "\t".translate_type_to_member_type($ty)." m$nm;\n";
		
		my ($basic,$typename) = translate_type($ty);
		if($basic)
		{
			push @def_constructor_list, "m$nm(0)";
		}
		push @copy_constructor_list, "m$nm(rToCopy.m$nm)";
		push @param_constructor_list, "m$nm($nm)";
	}
	
	# finish off
	print H "};\n\n";
	
	# now the cpp file...
	my $def_con_vars = join(",\n\t  ",@def_constructor_list);
	$def_con_vars = "\n\t: ".$def_con_vars if $def_con_vars ne '';
	my $copy_con_vars = join(",\n\t  ",@copy_constructor_list);
	$copy_con_vars = "\n\t: ".$copy_con_vars if $copy_con_vars ne '';
	my $param_con_vars = join(",\n\t  ",@param_constructor_list);
	$param_con_vars = "\n\t: ".$param_con_vars if $param_con_vars ne '';

	my $class = "$classname_base$cmd".'::';
	print CPP <<__E;
$class$classname_base$cmd()$def_con_vars
{
}
$class$classname_base$cmd(const $classname_base$cmd &rToCopy)$copy_con_vars
{
}
$class~$classname_base$cmd()
{
}
int ${class}GetType() const
{
	return $cmd_id{$cmd};
}
__E
	if($read_from_streams)
	{
		print CPP "void ${class}SetPropertiesFromStreamData(Protocol &rProtocol)\n{\n";
		for(my $x = 0; $x < $#{$cmd_contents{$cmd}}; $x+=2)
		{
			my ($ty,$nm) = (${$cmd_contents{$cmd}}[$x], ${$cmd_contents{$cmd}}[$x+1]);
			if($ty =~ m/\Avector/)
			{
				print CPP "\trProtocol.ReadVector(m$nm);\n";
			}
			else
			{
				print CPP "\trProtocol.Read(m$nm);\n";
			}
		}
		print CPP "}\n";
	}
	if($write_to_streams)
	{
		# implement extra constructor?
		if($param_con_vars ne '')
		{
			print CPP "$class$classname_base$cmd($param_con_args)$param_con_vars\n{\n}\n";
		}
		print CPP "void ${class}WritePropertiesToStreamData(Protocol &rProtocol) const\n{\n";
		for(my $x = 0; $x < $#{$cmd_contents{$cmd}}; $x+=2)
		{
			my ($ty,$nm) = (${$cmd_contents{$cmd}}[$x], ${$cmd_contents{$cmd}}[$x+1]);
			if($ty =~ m/\Avector/)
			{
				print CPP "\trProtocol.WriteVector(m$nm);\n";
			}
			else
			{
				print CPP "\trProtocol.Write(m$nm);\n";
			}
		}
		print CPP "}\n";
	}
	if(obj_is_type($cmd,'EndsConversation'))
	{
		print CPP "bool ${class}IsConversationEnd() const\n{\n\treturn true;\n}\n";
	}
	if(obj_is_type($cmd,'IsError'))
	{
		# get parameters
		my ($mem_type,$mem_subtype) = split /,/,obj_get_type_params($cmd,'IsError');
		print CPP <<__E;
bool ${class}IsError(int &rTypeOut, int &rSubTypeOut) const
{
	rTypeOut = m$mem_type;
	rSubTypeOut = m$mem_subtype;
	return true;
}
__E
	}

	if($implement_syslog)
	{
		my ($format,$args) = make_log_strings($cmd);
		print CPP <<__E;
void ${class}LogSysLog(const char *Action) const
{
	::syslog(LOG_INFO,"%s $format",Action$args);
}
__E
	}
	if($implement_filelog)
	{
		my ($format,$args) = make_log_strings($cmd);
		print CPP <<__E;
void ${class}LogFile(const char *Action, FILE *File) const
{
	::fprintf(File,"%s $format\\n",Action$args);
	::fflush(File);
}
__E
	}
}

# finally, the protocol object itself
print H <<__E;
class $classname_base : public Protocol
{
public:
	$classname_base(IOStream &rStream);
	virtual ~$classname_base();

	std::auto_ptr<$derive_objects_from> Receive();
	void Send(const ${derive_objects_from} &rObject);
__E
if($implement_syslog)
{
	print H "\tvoid SetLogToSysLog(bool Log = false) {mLogToSysLog = Log;}\n";
}
if($implement_filelog)
{
	print H "\tvoid SetLogToFile(FILE *File = 0) {mLogToFile = File;}\n";
}
if($type eq 'Server')
{
	# need to put in the conversation function
	print H "\tvoid DoServer($context_class &rContext);\n\n";
	# and the send vector thing
	print H "\tvoid SendStreamAfterCommand(IOStream *pStream);\n\n";
}
if($type eq 'Client')
{
	# add plain object taking query functions
	my $with_params;
	for my $cmd (@cmd_list)
	{
		if(obj_is_type($cmd,'Command'))
		{
			my $has_stream = obj_is_type($cmd,'StreamWithCommand');
			my $argextra = $has_stream?', IOStream &rStream':'';
			my $queryextra = $has_stream?', rStream':'';
			my $reply = obj_get_type_params($cmd,'Command');
			print H "\tstd::auto_ptr<$classname_base$reply> Query(const $classname_base$cmd &rQuery$argextra);\n";
			my @a;
			my @na;
			for(my $x = 0; $x < $#{$cmd_contents{$cmd}}; $x+=2)
			{
				my ($ty,$nm) = (${$cmd_contents{$cmd}}[$x], ${$cmd_contents{$cmd}}[$x+1]);
				push @a,translate_type_to_arg_type($ty)." $nm";
				push @na,"$nm";
			}
			my $ar = join(', ',@a);
			my $nar = join(', ',@na);
			$nar = "($nar)" if $nar ne '';

			$with_params .= "\tinline std::auto_ptr<$classname_base$reply> Query$cmd($ar$argextra)\n\t{\n";
			$with_params .= "\t\t$classname_base$cmd send$nar;\n";
			$with_params .= "\t\treturn Query(send$queryextra);\n";
			$with_params .= "\t}\n";
		}
	}
	# quick hack to correct bad argument lists for commands with zero paramters but with streams
	$with_params =~ s/\(, /(/g;
	print H "\n",$with_params,"\n";
}
print H <<__E;
private:
	$classname_base(const $classname_base &rToCopy);
__E
if($type eq 'Server')
{
	# need to put the streams to send vector
	print H "\tstd::vector<IOStream*> mStreamsToSend;\n\tvoid DeleteStreamsToSend();\n";
}

if($implement_filelog || $implement_syslog)
{
	print H <<__E;
	virtual void InformStreamReceiving(u_int32_t Size);
	virtual void InformStreamSending(u_int32_t Size);
__E
}

if($implement_syslog)
{
	print H "private:\n\tbool mLogToSysLog;\n";
}
if($implement_filelog)
{
	print H "private:\n\tFILE *mLogToFile;\n";
}
print H <<__E;

protected:	
	virtual std::auto_ptr<ProtocolObject> MakeProtocolObject(int ObjType);
	virtual const char *GetIdentString();
};

__E

my $construtor_extra = '';
$construtor_extra .= ', mLogToSysLog(false)' if $implement_syslog;
$construtor_extra .= ', mLogToFile(0)' if $implement_filelog;

my $destructor_extra = ($type eq 'Server')?"\n\tDeleteStreamsToSend();":'';

my $prefix = $classname_base.'::';
print CPP <<__E;
$prefix$classname_base(IOStream &rStream)
	: Protocol(rStream)$construtor_extra
{
}
$prefix~$classname_base()
{$destructor_extra
}
const char *${prefix}GetIdentString()
{
	return "$ident_string";
}
std::auto_ptr<ProtocolObject> ${prefix}MakeProtocolObject(int ObjType)
{
	switch(ObjType)
	{
__E

# do objects within this
for my $cmd (@cmd_list)
{
	print CPP <<__E;
	case $cmd_id{$cmd}:
		return std::auto_ptr<ProtocolObject>(new $classname_base$cmd);
		break;
__E
}

print CPP <<__E;
	default:
		THROW_EXCEPTION(ConnectionException, Conn_Protocol_UnknownCommandRecieved)
	}
}
__E
# write receieve and send functions
print CPP <<__E;
std::auto_ptr<$derive_objects_from> ${prefix}Receive()
{
	std::auto_ptr<${derive_objects_from}> preply((${derive_objects_from}*)(Protocol::Receive().release()));

__E
	if($implement_syslog)
	{
		print CPP <<__E;
	if(mLogToSysLog)
	{
		preply->LogSysLog("Receive");
	}
__E
	}
	if($implement_filelog)
	{
		print CPP <<__E;
	if(mLogToFile != 0)
	{
		preply->LogFile("Receive", mLogToFile);
	}
__E
	}
print CPP <<__E;

	return preply;
}

void ${prefix}Send(const ${derive_objects_from} &rObject)
{
__E
	if($implement_syslog)
	{
		print CPP <<__E;
	if(mLogToSysLog)
	{
		rObject.LogSysLog("Send");
	}
__E
	}
	if($implement_filelog)
	{
		print CPP <<__E;
	if(mLogToFile != 0)
	{
		rObject.LogFile("Send", mLogToFile);
	}
__E
	}

print CPP <<__E;
	Protocol::Send(rObject);
}

__E
# write server function?
if($type eq 'Server')
{
	print CPP <<__E;
void ${prefix}DoServer($context_class &rContext)
{
	// Handshake with client
	Handshake();

	// Command processing loop
	bool inProgress = true;
	while(inProgress)
	{
		// Get an object from the conversation
		std::auto_ptr<${derive_objects_from}> pobj(Receive());

__E
	if($implement_syslog)
	{
		print CPP <<__E;
		if(mLogToSysLog)
		{
			pobj->LogSysLog("Receive");
		}
__E
	}
	if($implement_filelog)
	{
		print CPP <<__E;
		if(mLogToFile != 0)
		{
			pobj->LogFile("Receive", mLogToFile);
		}
__E
	}
	print CPP <<__E;

		// Run the command
		std::auto_ptr<${derive_objects_from}> preply((${derive_objects_from}*)(pobj->DoCommand(*this, rContext).release()));
		
__E
	if($implement_syslog)
	{
		print CPP <<__E;
		if(mLogToSysLog)
		{
			preply->LogSysLog("Send");
		}
__E
	}
	if($implement_filelog)
	{
		print CPP <<__E;
		if(mLogToFile != 0)
		{
			preply->LogFile("Send", mLogToFile);
		}
__E
	}
	print CPP <<__E;

		// Send the reply
		Send(*(preply.get()));

		// Send any streams
		for(unsigned int s = 0; s < mStreamsToSend.size(); s++)
		{
			// Send the streams
			SendStream(*mStreamsToSend[s]);
		}
		// Delete these streams
		DeleteStreamsToSend();

		// Does this end the conversation?
		if(pobj->IsConversationEnd())
		{
			inProgress = false;
		}
	}	
}

void ${prefix}SendStreamAfterCommand(IOStream *pStream)
{
	ASSERT(pStream != NULL);
	mStreamsToSend.push_back(pStream);
}

void ${prefix}DeleteStreamsToSend()
{
	for(std::vector<IOStream*>::iterator i(mStreamsToSend.begin()); i != mStreamsToSend.end(); ++i)
	{
		delete (*i);
	}
	mStreamsToSend.clear();
}

__E
}

# write logging functions?
if($implement_filelog || $implement_syslog)
{
	my ($fR,$fS);

	if($implement_syslog)
	{
		$fR .= qq~\tif(mLogToSysLog) { ::syslog(LOG_INFO, (Size==Protocol::ProtocolStream_SizeUncertain)?"Receiving stream, size uncertain":"Receiving stream, size %d", Size); }\n~;
		$fS .= qq~\tif(mLogToSysLog) { ::syslog(LOG_INFO, (Size==Protocol::ProtocolStream_SizeUncertain)?"Sending stream, size uncertain":"Sending stream, size %d", Size); }\n~;
	}
	if($implement_filelog)
	{
		$fR .= qq~\tif(mLogToFile) { ::fprintf(mLogToFile, (Size==Protocol::ProtocolStream_SizeUncertain)?"Receiving stream, size uncertain":"Receiving stream, size %d\\n", Size); ::fflush(mLogToFile); }\n~;
		$fS .= qq~\tif(mLogToFile) { ::fprintf(mLogToFile, (Size==Protocol::ProtocolStream_SizeUncertain)?"Sending stream, size uncertain":"Sending stream, size %d\\n", Size); ::fflush(mLogToFile); }\n~;
	}

	print CPP <<__E;

void ${prefix}InformStreamReceiving(u_int32_t Size)
{
$fR}

void ${prefix}InformStreamSending(u_int32_t Size)
{
$fS}

__E
}


# write client Query functions?
if($type eq 'Client')
{
	for my $cmd (@cmd_list)
	{
		if(obj_is_type($cmd,'Command'))
		{
			my $reply = obj_get_type_params($cmd,'Command');
			my $reply_id = $cmd_id{$reply};
			my $has_stream = obj_is_type($cmd,'StreamWithCommand');
			my $argextra = $has_stream?', IOStream &rStream':'';
			my $send_stream_extra = '';
			if($has_stream)
			{
				$send_stream_extra = <<__E;
	
	// Send stream after the command
	SendStream(rStream);
__E
			}
			print CPP <<__E;
std::auto_ptr<$classname_base$reply> ${classname_base}::Query(const $classname_base$cmd &rQuery$argextra)
{
	// Send query
	Send(rQuery);
	$send_stream_extra
	// Wait for the reply
	std::auto_ptr<${derive_objects_from}> preply(Receive().release());

	if(preply->GetType() == $reply_id)
	{
		// Correct response
		return std::auto_ptr<$classname_base$reply>(($classname_base$reply*)preply.release());
	}
	else
	{
		// Set protocol error
		int type, subType;
		if(preply->IsError(type, subType))
		{
			SetError(type, subType);
			TRACE2("Protocol: Error received %d/%d\\n", type, subType);
		}
		else
		{
			SetError(Protocol::UnknownError, Protocol::UnknownError);
		}
		
		// Throw an exception
		THROW_EXCEPTION(ConnectionException, Conn_Protocol_UnexpectedReply)
	}
}
__E
		}
	}
}



print H <<__E;
#endif // $guardname

__E

# close files
close H;
close CPP;


sub obj_is_type
{
	my ($c,$ty) = @_;
	for(@{$cmd_attributes{$c}})
	{
		return 1 if $_ =~ m/\A$ty/;
	}
	
	return 0;
}

sub obj_get_type_params
{
	my ($c,$ty) = @_;
	for(@{$cmd_attributes{$c}})
	{
		return $1 if $_ =~ m/\A$ty\((.+?)\)\Z/;
	}
	die "Can't find attribute $ty\n"
}

# returns (is basic type, typename)
sub translate_type
{
	my $ty = $_[0];
	
	if($ty =~ m/\Avector\<(.+?)\>\Z/)
	{
		my $v_type = $1;
		my (undef,$v_ty) = translate_type($v_type);
		return (0, 'std::vector<'.$v_ty.'>')
	}
	else
	{
		if(!exists $translate_type_info{$ty})
		{
			die "Don't know about type name $ty\n";
		}
		return @{$translate_type_info{$ty}}
	}
}

sub translate_type_to_arg_type
{
	my ($basic,$typename) = translate_type(@_);
	return $basic?$typename:'const '.$typename.'&'
}

sub translate_type_to_member_type
{
	my ($basic,$typename) = translate_type(@_);
	return $typename
}

sub make_log_strings
{
	my ($cmd) = @_;

	my @str;
	my @arg;
	for(my $x = 0; $x < $#{$cmd_contents{$cmd}}; $x+=2)
	{
		my ($ty,$nm) = (${$cmd_contents{$cmd}}[$x], ${$cmd_contents{$cmd}}[$x+1]);
		
		if(exists $log_display_types{$ty})
		{
			# need to translate it
			my ($format,$arg) = @{$log_display_types{$ty}};
			$arg =~ s/VAR/m$nm/g;

			if ($format eq "0x%llx" and $target_windows)
			{
				$format = "0x%I64x";
				$arg = "(uint64_t)$arg";
			}

			push @str,$format;
			push @arg,$arg;
		}
		else
		{
			# is opaque
			push @str,'OPAQUE';
		}
	}
	return ($cmd.'('.join(',',@str).')', join(',','',@arg));
}


