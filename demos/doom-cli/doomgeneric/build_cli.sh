#!/bin/sh

set -e

CFLAGS="$CFLAGS -g"
CFLAGS="$CFLAGS -DDOOMGENERIC_RESX=320 -DDOOMGENERIC_RESY=200 -DDISABLE_ZENITY"
CC="${CC:-cc}"

OBJDIR=build
OUTPUT=doomgeneric

cd "$(dirname "$0")"

SRC_DOOM="
    dummy
    am_map
    doomdef
    doomstat
    dstrings
    d_event
    d_items
    d_iwad
    d_loop
    d_main
    d_mode
    d_net
    f_finale
    f_wipe
    g_game
    hu_lib
    hu_stuff
    info
    i_cdmus
    i_endoom
    i_joystick
    i_scale
    i_sound
    i_system
    i_timer
    memio
    m_argv
    m_bbox
    m_cheat
    m_config
    m_controls
    m_fixed
    m_menu
    m_misc
    m_random
    p_ceilng
    p_doors
    p_enemy
    p_floor
    p_inter
    p_lights
    p_map
    p_maputl
    p_mobj
    p_plats
    p_pspr
    p_saveg
    p_setup
    p_sight
    p_spec
    p_switch
    p_telept
    p_tick
    p_user
    r_bsp
    r_data
    r_draw
    r_main
    r_plane
    r_segs
    r_sky
    r_things
    sha1
    sounds
    statdump
    st_lib
    st_stuff
    s_sound
    tables
    v_video
    wi_stuff
    w_checksum
    w_file
    w_main
    w_wad
    z_zone
    w_file_stdc
    i_input
    i_video
    doomgeneric
    doomgeneric_cli
    cli_data
"

if [ "$1" = "clean" ]; then
    echo "rm -rf $OBJDIR"
    rm -rf $OBJDIR
    echo "rm -f $OUTPUT"
    rm -f $OUTPUT
    exit 0
fi

mkdir -p $OBJDIR
OBJS=
for name in $SRC_DOOM; do
    OBJS="$OBJS $OBJDIR/$name.o"
    # This doesn't detect header file changes, but neither does the Makefile
    if ! [ -e $OBJDIR/$name.o ] || [ $OBJDIR/$name.o -ot $name.c ]; then
        echo "[Compiling $name.c]"
        $CC -c $name.c $CFLAGS -o $OBJDIR/$name.o
    fi
done
echo "[Linking $OUTPUT]"
$CC $CFLAGS $LDFLAGS $OBJS -o $OUTPUT
