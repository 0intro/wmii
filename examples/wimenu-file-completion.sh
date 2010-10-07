#!/bin/sh
# This script will launch wimenu and provide command
# completion for the first argument and filename completion
# for each following argument, and execute the result.
# Program name completion requires that a program list already
# exist in $(wmiir namespace)/.proglist

fifo="$HOME/.wmii/menu_fifo"
mkfifo $fifo 2>/dev/null

script=$(cat <<'!'
    BEGIN {
        progs = "cat $(wmiir namespace)/.proglist"

        # Print the first set of completions to wimenu’s fifo
        print read(progs) >fifo
        fflush(fifo)
    }

    # Process the input and provide the completions
    {
        # Skip the trailing part of the command.
        # If there is none, this is the result.
        if (!getline rest) {
            print
            exit
        }

        if (!match($0, /.*[ \t]/))
            # First argument, provide the program list
            update(0, progs)
        else {
            # Set the offset to the location of the last
            # space, and save that part of the completion
            offset = RLENGTH
            str = substr($0, offset + 1)

            # If we're completing a sub-directory, adjust
            # the offset to the position of the last /
            if (match(str, ".*/"))
                offset += RLENGTH

            # If the last component of the path begins with
            # a ., include hidden files
            arg = ""
            if(match(str, "(^|/)\\.[^/]*$"))
                    arg = "-A"

            # Substitute ~/ for $HOME/
            sub("^~/", ENVIRON["HOME"] "/", str)

            # Strip the trailing filename
            sub("[^/]+$", "", str)

            update(offset, "ls " arg quote(str))
        }
    }

    # Push out a new set of completions
    function update(offset, cmd) {
        # Only push out the completion if the offset or the
        # option of ls has changed. The behavior will be the
        # same regardless, but this is a minor optimization
        if (offset != loffset || cmd != lcmd) {
            loffset = offset
            lcmd = cmd

            cmpl = read(cmd)
            print offset >fifo
            print cmpl >fifo
            fflush(fifo)
        }
    }

    # Quote a string. This should work in any Bourne
    # or POSIX compatible shell.
    function quote(str) {
        if (!match(str, /[\[\](){}$'"^#~!&;*?|<>]/))
            return str
        gsub(/\\/, "'\\\\'", str)
        gsub(/'/, "'\\''", str)
        return "'" str "'"
    }

    # Read the output of a command and return it
    function read(cmd) {
        if (cmd in cache)
            return cache[cmd]
        res = ""
        while (cmd | getline)
            res = res quote($0) "\n"
        close(cmd)
        return cache[cmd] = res
    }
!
)
res="$(wimenu -c "$@" <$fifo | awk -v "fifo=$fifo" "$script")"
exec ${SHELL:-sh} -c "exec $res"
