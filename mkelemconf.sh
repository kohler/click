#!/bin/sh

# mkelemconf.sh -- script takes list of elements and generates Makefile or
# C++ source code
# Eddie Kohler
#
# Copyright (c) 2000 Massachusetts Institute of Technology.
#
# This software is being provided by the copyright holders under the GNU
# General Public License, either version 2 or, at your discretion, any later
# version. For more information, see the `COPYRIGHT' file in the source
# distribution.

# determine mode
makefile=1
expand=0
prefix=""
all=0
verbose=""
while [ x"$1" != x ]; do
case $1 in
  -m|--m|--ma|--mak|--make|--makef|--makefi|--makefil|--makefile)
     makefile=1; shift 1;;
  -c|--c|--cx|--cxx)
     makefile=0; shift 1;;
  -p|--p|--pr|--pre|--pref|--prefi|--prefix)
     shift 1; prefix="$1/"; shift 1;;
  -p*)
     prefix=`echo "$1" | sed 's/^-p//'`; shift 1;;
  --p=*|--pr=*|--pre=*|--pref=*|--prefi=*|--prefix=*)
     prefix=`echo "$1" | sed 's/^[^=]*=//'`; shift 1;;
  -v|--v|--ve|--ver|--verb|--verbo|--verbos|--verbose)
     verbose=1; shift 1;;
  -a|--a|--al|--all)
     all=1; shift 1;;
  *)
     echo "Usage: ./findelements.sh [-m|-c] [-v] [-pPREFIX] < elements.conf" 1>&2
     exit 1;;
esac
done

if test -n "$verbose" -a -n "$prefix"; then
  echo "Prefix: $verbose" 1>&2
fi

# expand list of files
if test -n "$prefix"; then
  prefix=`echo "$prefix" | sed 's/\//\\\//'`
  echo "$prefix"
  files=`cat | sed 's/^/'"$prefix"'/'`
else
  files=`cat`
fi

# find a good version of awk
if test -x /usr/bin/gawk; then
  awk=gawk
elif test -x /usr/bin/nawk; then
  awk=nawk
else
  awk=awk
fi

# output files!
if test $makefile = 1; then
  echo "ELEMENT_OBJS = \\"
  echo "$files" | sed -e 's/\.cc*$/.o \\/;s/^.*\///' | grep .
  echo
  # for i in `echo "$bad_files" | sort | uniq`; do
  #   echo "*** warning: dependency check failed for $i" 1>&2
  # done
else
  grep '^EXPORT_ELEMENT' $files | $awk -F: 'BEGIN {
   OFS = "";
}
{
  sub(/\.cc/, ".hh", $1);
  INCLUDES[$1] = 1;
  sub(/EXPORT_ELEMENT\(/, "", $2);
  sub(/\)/, "", $2);
  B = B "  lexer->add_element_type(new " $2 ");\n";
}
END {
  print "#ifdef HAVE_CONFIG_H\n# include <config.h>\n#endif\n#include \"lexer.hh\"";
  for (file in INCLUDES) {
    print "#include \"", file, "\"";
  }
  print "void\nexport_elements(Lexer *lexer)\n{";
  print B, "}";
}
'
fi
