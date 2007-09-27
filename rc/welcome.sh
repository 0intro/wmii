#!/bin/sh
# display a welcome message that contains the wmii tutorial

xmessage -file - <<'EOF'
Welcome to wmii, the non-wimp environment of the Suckless Project.

This is a small step by step tutorial, intended to make you a
little bit familiar with wmii.

From here on, keypresses will be described such that M-a refers to
pressing $MODKEY and a at the same time. $MODKEY refers to a
configuration variable which contains the Alt key by default.

Let's go!

- Start two xterms by pressing M-Return twice.
- Switch between the three windows: M-j, M-k,
  M-h, M-l
  If you prefer to use the mouse, then just move the pointer to
  the desired window.
- Try the other column modes: M-s for stack mode,
  M-m for max mode Press M-d to return to default
  mode.
- Create a new column with: M-Shift-l
  This moves the client rightwards.
- Tag the selected client with another tag: M-Shift-2
  IMPORTANT: before you do the next step, note that you
    can select the current tag with M-1.
- Select the new tag: M-2
- Select the floating area: M-Space
- Open the programs menu: M-p
  Type 'xclock' and press Enter.
- Move the xclock window: Hold $MODKEY, left-click on the
  window and move the cursor around.
- Resize the xclock window: Hold $MODKEY, right-click the
  window and move the cursor around.
- Kill the selected client (the xclock window) with: M-Shift-c
- Open the actions menu: M-a
  Rerun wmiirc by selecting 'wmiirc'.
- We'll now have a look at the internal filesystem used by
  wmii.  Executing
  	wmiir ls /
  in the shell of the terminal will list all the files in the
  root directory.
  Entries ending with / are directories.
  If you are curious, you can now dig deeper into the
  directory trees. For instance,
  	wmiir ls /rbar/
  will show you the content of the right half of the bar.

We hope that these steps gave you an idea of how wmii works.
You can reread them at any time by pressing $MODKEY-a and
selecting 'welcome'.

You should now take a look at the wmii(1) man page.  A FAQ is
available at <http://wmii.suckless.org>.
EOF
