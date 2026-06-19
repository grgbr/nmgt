#!/bin/bash -e

PREFIX='@@__PREFIX__@@'
SBINDIR="$PREFIX/sbin"
BINDIR="$PREFIX/bin"
WWWDIR="$PREFIX/www"

ECHOE="/bin/echo -e"

error()
{
	$ECHOE "$arg0: $*" >&2
}

child=()
trap 'kill -9 ${child[*]} 2>/dev/null; exit 0' SIGCHLD EXIT INT TERM

$SBINDIR/restconfd webmin &
child+=($!)
$BINDIR/nghttpd 10081 --address=127.0.0.1 --no-tls -d $WWWDIR &
child+=($!)
$BINDIR/nghttpx --add-forwarded=for --frontend='0.0.0.0,8080;no-tls' \
	--backend='127.0.0.1,10000;/restconf/:/yang/:/streams/:/.well-known/;proto=h2' \
	--backend='127.0.0.1,10081;;proto=h2' &
child+=($!)
wait
