#!/bin/sh
rm -f layoutdef.h layoutdef.c layout.mk
echo "#include \"layoutdef.h\"" > layoutdef.c
echo "#include \"wm.h\"" >> layoutdef.c
echo "void init_layouts() {" >> layoutdef.c
for i in `ls layout_*.c`; do
    FUNC="`echo \`basename $i\` | sed 's/\.c//g'`"
    echo "void init_$FUNC();" >> layoutdef.h
    echo "   init_$FUNC();" >> layoutdef.c;
    echo "SRC += $i" >>layoutdef.mk
done
echo "}" >> layoutdef.c
