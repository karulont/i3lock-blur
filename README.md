i3lock - improved screen locker
===============================
i3lock is a simple screen locker like slock. After starting it, you will
see a white screen (you can configure the color/an image). You can return
to your screen by entering your password.

Many little improvements have been made to i3lock over time:

- Fuzzy option added. With that there is no image or solid color displayed
  when screen is locked. A blurring filter is applied to the screen.

- i3lock forks, so you can combine it with an alias to suspend to RAM
  (run "i3lock && echo mem > /sys/power/state" to get a locked screen
   after waking up your computer from suspend to RAM)

- You can specify either a background color or a PNG image which will be
  displayed while your screen is locked.

- You can specify whether i3lock should bell upon a wrong password.

- i3lock uses PAM and therefore is compatible with LDAP etc.

Requirements
------------
- pkg-config
- libxcb
- libxcb-util
- libpam-dev
- libcairo-dev
- libxcb-xinerama
- libev
- libx11-dev
- libx11-xcb-dev
- libxkbfile-dev
- libxkbcommon >= 0.4.0
- libxkbcommon-x11 >= 0.4.0
- libGL

Install packages in Ubuntu

  sudo apt-get install pkg-config libxcb1-dev libxcb1 libgl2ps-dev libx11-dev libglc0 libglc-dev libcairo2-dev libcairo-gobject2 libcairo2-dev libxkbfile-dev libxkbfile1 libxkbcommon-dev libxkbcommon-x11-dev libxcb-xkb-dev libxcb-dpms0-dev libxcb-damage0-dev libpam0g-dev libev-dev libxcb-image0-dev libxcb-util0-dev libxcb-composite0-dev libxcb-xinerama0-dev

Running i3lock
-------------
Simply invoke the 'i3lock' command. To get out of it, enter your password and
press enter.

Upstream
--------
Please submit pull requests to https://github.com/i3/i3lock
