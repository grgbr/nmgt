#!/bin/bash -e

PREFIX='@@__PREFIX__@@'
SBINDIR="$PREFIX/sbin"
ETCDIR="$PREFIX/etc"

ECHOE="/bin/echo -e"

error()
{
	$ECHOE "$arg0: $*" >&2
}

exec $SBINDIR/netopeer2-server -d -v 2
