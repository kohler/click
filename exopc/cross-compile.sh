#! /bin/sh -a

CROSSBIN=$CROSSDIR
CROSSINC=$CROSSDIR/include

PATH=$CROSSBIN:$PATH
BINFILE=$CROSSBIN/i386-unknown-openbsd2-`basename $0`

#echo using cross compile binary $BINFILE
exec $BINFILE -nostdinc -I$CROSSINC $@

