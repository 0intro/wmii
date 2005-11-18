#!/bin/sh
rm -f layout.h layout.c layout.mk
echo "#include \"layout.h\"" > layout.c
echo "#include \"wm.h\"" >> layout.c
echo "void init_layouts() {" >> layout.c
for i in `ls layout_*.c`; do
    FUNC="`echo \`basename $i\` | sed 's/\.c//g'`"
    echo "void init_$FUNC();" >> layout.h
    echo "   init_$FUNC();" >> layout.c;
    echo "SRC += $i" >>layout.mk
done
echo "}" >> layout.c
