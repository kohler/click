#!/bin/sh

# findelements.sh -- script analyzes element source code during build process
# Eddie Kohler
#
# Copyright (c) 1999 Massachusetts Institute of Technology.
#
# This software is being provided by the copyright holders under the GNU
# General Public License, either version 2 or, at your discretion, any later
# version. For more information, see the `COPYRIGHT' file in the source
# distribution.

# determine mode
expand=0
prefix=""
all=0
while [ x"$1" != x ]; do
case $1 in
  -m|--m|--ma|--mak|--make|--makef|--makefi|--makefil|--makefile)
     makefile=1; shift 1;;
  -c|--c|--cx|--cxx)
     makefile=0; shift 1;;
  -x|--e|--ex|--exp|--expa|--expan|--expand)
     expand=1; shift 1;;
  -p|--p|--pr|--pre|--pref|--prefi|--prefix)
     shift 1; prefix="$1/"; shift 1;;
  -a|--a|--al|--all)
     all=1; shift 1;;
  *)
     echo "Usage: ./findelements.sh [-m|-c|-x] < [FILES AND DIRECTORIES]" 1>&2
     exit 1;;
esac
done

# expand list of files
if test $all = 1; then
  first_files=`cd ${prefix}elements; ls`
else
  first_files=`cat`
fi

files=""
for i in $first_files; do
  if test -d "${prefix}elements/$i"; then
    if echo "$i" | grep '/'; then
      :
    else
      i="${prefix}elements/$i"
    fi
  fi
  if test -d $i; then
    files="$files
"`find $i \( -name \[^.,]\*.cc -o -name \[^.,]\*.c \) -print`
  else
    files="$files
$i"
  fi
done
files=`echo "$files" | sort | uniq`

# exit if expanded
if test $expand = 1; then
  echo "$files" | grep .; exit 0
fi

# find a good version of awk
if test -x /usr/bin/gawk; then
  awk=gawk
elif test -x /usr/bin/nawk; then
  awk=nawk
else
  awk=awk
fi

# check dependencies: generate a list of bad files, then remove those files
# from the list of good files
bad_files=''
while true; do
  exports1=`grep '^EXPORT_ELEMENT' $files | sed 's/.*(\(.*\)).*/\1/'`
  exports2=`grep '^ELEMENT_PROVIDES' $files | sed 's/.*(\(.*\)).*/\1/'`
  exports3=`echo "$files" | sed 's/^elements\/\([^\/]*\)\/.*/\1/'`
  awk_exports=`echo "$exports1$exports2$exports3" | sed 's/\(.*\)/dep["\1"]=1;/'`
  new_bad_files=`grep '^ELEMENT_REQUIRES' $files | $awk -F: 'BEGIN {OFS="";'"$awk_exports"'}
{
  sub(/ELEMENT_REQUIRES\(/, "", $2);
  sub(/\)/, "", $2);
  split($2, deps, / +/);
  for (j in deps) {
    i = deps[j]
    if (i ~ /^!/) {
      sub(/^!/, "", i);
      if (dep[i]) {
        print $1;
        break;
      }
    } else if (!dep[i]) {
      print $1;
      break;
    }
  }
}' | sort | uniq`
  if test -z "$new_bad_files"; then
    break
  else
    files=`echo "$files
$new_bad_files" | sort | uniq -u`
    bad_files="$new_bad_files
$bad_files"
  fi
done

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
