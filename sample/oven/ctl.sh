#!/bin/sh -e

PREFIX='@@__PREFIX__@@'
SBINDIR="$PREFIX/sbin"

ECHOE="/bin/echo -e"

error()
{
	$ECHOE "$arg0: $*" >&2
}

show()
{
	if [ "$fmt" = "json" ]; then
		# Perform operation using JSON command
		exec $SBINDIR/sysrepocfg --datastore running \
		                         --export \
		                         --xpath '/oven:oven' \
		                         --format json
	elif [ "$fmt" = "xml" ]; then
		# Perform operation using XML command
		exec $SBINDIR/sysrepocfg --datastore running \
		                         --export \
		                         --xpath '/oven:oven'
		                         --format xml
	fi
}

state()
{
	if [ "$fmt" = "json" ]; then
		# Perform operation using JSON command
		exec $SBINDIR/sysrepocfg --datastore operational \
		                         --export \
		                         --xpath '/oven:oven-state' \
		                         --format json
	elif [ "$fmt" = "xml" ]; then
		# Perform operation using XML command
		exec $SBINDIR/sysrepocfg --datastore operation \
		                         --export \
		                         --xpath '/oven:oven-state'
		                         --format xml
	fi
}

settemp()
{
	local temp="$1"
	
	if [ "$fmt" = "json" ]; then
		# Perform operation using JSON command
		exec $SBINDIR/sysrepocfg --datastore running \
		                         --module oven \
		                         --edit \
		                         --format json <<_EOF
{
	"oven:oven": {
		"temperature": $temp
	}
}
_EOF
	elif [ "$fmt" = "xml" ]; then
		# Perform operation using XML command
		exec $SBINDIR/sysrepocfg --datastore running \
		                         --module oven \
		                         --edit \
		                         --format xml <<_EOF
<oven xmlns="urn:sysrepo:oven">
	<temperature>$temp</temperature>
</oven>
_EOF
	fi
}

turnonoff()
{
	local val
	
	if [ "$1" = "on" ]; then
		val="true"
	else
		val="false"
	fi
	
	if [ "$fmt" = "json" ]; then
		# Perform operation using JSON command
		exec $SBINDIR/sysrepocfg --datastore running \
		                         --module oven \
		                         --edit \
		                         --format json <<_EOF
{
	"oven:oven": {
		"turned-on": $val
	}
}
_EOF
	elif [ "$fmt" = "xml" ]; then
		# Perform operation using XML command
		exec $SBINDIR/sysrepocfg --datastore running \
		                         --module oven \
		                         --edit \
		                         --format xml <<_EOF
<oven xmlns="urn:sysrepo:oven">
	<turned-on>$val</turned-on>
</oven>
_EOF
	fi
}

insert()
{
	local when="$1"
	
	if [ "$fmt" = "json" ]; then
		# Perform operation using JSON command
		exec $SBINDIR/sysrepocfg --module oven \
		                         --rpc \
		                         --format json <<_EOF
{
	"oven:insert-food": {
		"time": "$when"
	}
}
_EOF
	elif [ "$fmt" = "xml" ]; then
		# Perform operation using XML command
		exec $SBINDIR/sysrepocfg --module oven \
		                         --rpc \
		                         --format xml <<_EOF
<insert-food xmlns="urn:sysrepo:oven">
	<time>$when</time>
</oven>
_EOF
	fi
}

remove()
{
	if [ "$fmt" = "json" ]; then
		# Perform operation using JSON command
		exec $SBINDIR/sysrepocfg --module oven \
		                         --rpc \
		                         --format json <<_EOF
{
	"oven:remove-food": { }
}
_EOF
	elif [ "$fmt" = "xml" ]; then
		# Perform operation using XML command
		exec $SBINDIR/sysrepocfg --module oven \
		                         --rpc \
		                         --format xml <<_EOF
<remove-food xmlns="urn:sysrepo:oven">
</remove-food>
_EOF
	fi
}

usage()
{
	cat >&2 <<_EOF
Usage: $arg0 [OPTIONS] <COMMAND>
Sysrepo sample oven tool.

Where COMMAND:
    settemp <TEMPERATURE>      -- configure oven temperature
    turnon                     -- configure oven on
    turnoff                    -- configure oven off
    insert <WHEN>              -- insert food into the oven according to <WHEN>
    remove                     -- remove food from the oven
    show                       -- show oven configuration
    state                      -- show oven state

Where OPTIONS:
    -f | --format <FORMAT>     -- serialization format used to perform requested
                                  operation (defaults to "json")
    -h | --help                -- this help message

With:
    TEMPERATURE                -- temperature in degree Celsius
    FORMAT                     -- either "xml" or "json"
    WHEN                       -- either "now" or "on-oven-ready"
_EOF
}

arg0="$(basename $0)"
cmdln=$(getopt --options f:h \
               --longoptions format:,help \
               --name "$arg0" \
               -- "$@")
if [ $? -gt 0 ]; then
	usage
	exit 1
fi
eval set -- "$cmdln"

fmt='json'
while true; do
	case "$1" in
	-f|--format)
		fmt="$2"
		if [ "$fmt" != "json" ] && [ "$fmt" != "xml" ]; then
			error "invalid '$fmt' format.\n"
			usage
			exit 1
		fi
		shift 2;;
	-h|--help)
		usage
		exit 0;;
	--)
		shift;
		break;;
	*)
		break;;
	esac
done

if [ $# -lt 1 ]; then
	error "missing argument.\n"
	usage
	exit 1
fi

cmd="$1"
if [ "$cmd" = "settemp" ]; then
	if [ $# -ne 2 ]; then
		error "missing temperature argument.\n"
		usage
		exit 1
	fi
	settemp "$2"
elif [ "$cmd" = "turnon" ]; then
	turnonoff "on"
elif [ "$cmd" = "turnoff" ]; then
	turnonoff "off"
elif [ "$cmd" = "insert" ]; then
	if [ $# -ne 2 ]; then
		error "missing when argument.\n"
		usage
		exit 1
	fi
	insert "$2"
elif [ "$cmd" = "remove" ]; then
	remove
elif [ "$cmd" = "show" ]; then
	show
elif [ "$cmd" = "state" ]; then
	state
else
	error "invalid '$cmd' command.\n"
	usage
	exit 1
fi
