# clrpick.tcl --
#
#	Color selection dialog.
#	standard color selection dialog.
#
# Copyright (c) 1996 Sun Microsystems, Inc.
# Copyright (c) 1999 Christian Werner
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

# ck_chooseColor --
#
#	Create a color dialog and let the user choose a color. This function
#	should not be called directly. It is called by the tk_chooseColor
#	function when a native color selector widget does not exist

proc ck_chooseColor args {
    global ckPriv
    set w .__ck__color
    upvar #0 $w data
    if {[winfo depth .] == 1} {
	set data(colors) {black white}
	set data(rcolors) {white black}
    } else {
	set data(colors) {black blue cyan green magenta red white yellow}
	set data(rcolors) {white white white white white white black black}
    }
    ckColorDialog_Config $w $args

    if {![winfo exists $w]} {
	toplevel $w -class CkColorDialog -border {
	    ulcorner hline urcorner vline lrcorner hline llcorner vline
	}
	ckColorDialog_BuildDialog $w
    }
    place $w -relx 0.5 -rely 0.5 -anchor center

    # Set the focus.

    set oldFocus [focus]
    focus $w.bot.ok

    # Wait for the user to respond, then restore the focus and
    # return the index of the selected button.  Restore the focus
    # before deleting the window, since otherwise the window manager
    # may take the focus away so we can't redirect it.  Finally,
    # restore any grab that was in effect.

    tkwait variable ckPriv(selectColor)
    catch {focus $oldFocus}
    destroy $w
    unset data
    return $ckPriv(selectColor)
}

# ckColorDialog_Config  --
#
#	Parses the command line arguments to tk_chooseColor
#
proc ckColorDialog_Config {w argList} {
    global ckPriv
    upvar #0 $w data
    set specs {
	{-initialcolor "" "" ""}
	{-parent "" "" "."}
	{-title "" "" "Color"}
    }
    tclParseConfigSpec $w $specs "" $argList
    if {![string compare $data(-initialcolor) ""]} {
	if {[info exists ckPriv(selectColor)] && \
		[string compare $ckPriv(selectColor) ""]} {
	    set data(-initialcolor) $ckPriv(selectColor)
	} else {
	    set data(-initialcolor) [. cget -background]
	}
    } elseif {[lsearch -exact $data(colors) $data(-initialcolor)] <= 0} {
	error "illegal -initialcolor"
    }
    if {![winfo exists $data(-parent)]} {
	error "bad window path name \"$data(-parent)\""
    }
}

# ckColorDialog_BuildDialog --
#
#	Build the dialog.
#
proc ckColorDialog_BuildDialog w {
    upvar #0 $w data
    label $w.title -text "Select Color"
    pack $w.title -side top -fill x -pady 1
    frame $w.top
    pack $w.top -side top -fill x -padx 1
    set count 0
    foreach i $data(colors) {
	radiobutton $w.top.$i -background $i -text $i -value $i \
	    -variable ${w}(finalColor) \
	    -foreground [lindex $data(rcolors) $count] \
	    -selectcolor [lindex $data(rcolors) $count]
	if {[winfo depth .] > 1} {
	    $w.top.$i configure -activeforeground \
		[$w.top.$i cget -background] -activeattributes bold \
		-activebackground [$w.top.$i cget -foreground]
	}
	pack $w.top.$i -side top -fill x
	incr count
    }
    frame $w.bot
    pack $w.bot -side top -fill x -padx 1 -pady 1
    button $w.bot.ok -text OK -width 8 -underline 0 \
	-command [list ckColorDialog_OkCmd $w]
    button $w.bot.cancel -text Cancel -width 8 -underline 0 \
	-command [list ckColorDialog_CancelCmd $w]
    pack $w.bot.ok $w.bot.cancel -side left -expand 1
    # Accelerator bindings
    bind $w <Escape> [list ckButtonInvoke $w.bot.cancel]
    bind $w <c> [list ckButtonInvoke $w.bot.cancel]
    bind $w <C> [list ckButtonInvoke $w.bot.cancel]
    bind $w <o> [list ckButtonInvoke $w.bot.ok]
    bind $w <O> [list ckButtonInvoke $w.bot.ok]
    set data(finalColor) $data(-initialcolor)
}

proc ckColorDialog_OkCmd {w} {
    global ckPriv
    upvar #0 $w data
    set ckPriv(selectColor) $data(finalColor)
}

proc ckColorDialog_CancelCmd {w} {
    global ckPriv
    set ckPriv(selectColor) ""
}

