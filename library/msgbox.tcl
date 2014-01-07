# msgbox.tcl --
#
#	Implements messageboxes.
#
# Copyright (c) 1994-1997 Sun Microsystems, Inc.
# Copyright (c) 1999 Christian Werner
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.


# ck_messageBox --
#
#	Pops up a messagebox with an application-supplied message with
#	an icon and a list of buttons.
#	See the user documentation for details on what ck_messageBox does.

proc ck_messageBox args {
    global ckPriv
    set w ckPrivMsgBox
    upvar #0 $w data
    set specs {
	{-default "" "" ""}
        {-icon "" "" "info"}
        {-message "" "" ""}
        {-parent "" "" .}
        {-title "" "" ""}
        {-type "" "" "ok"}
    }
    tclParseConfigSpec $w $specs "" $args
    if {[lsearch {info warning error question} $data(-icon)] == -1} {
	error "invalid icon \"$data(-icon)\", must be error, info, question or warning"
    }
    if {![winfo exists $data(-parent)]} {
	error "bad window path name \"$data(-parent)\""
    }
    switch -- $data(-type) {
	abortretryignore {
	    set buttons {
		{abort  -width 6 -text Abort -underline 0}
		{retry  -width 6 -text Retry -underline 0}
		{ignore -width 6 -text Ignore -underline 0}
	    }
	}
	ok {
	    set buttons {
		{ok -width 6 -text OK -underline 0}
	    }
	    if {$data(-default) == ""} {
		set data(-default) "ok"
	    }
	}
	okcancel {
	    set buttons {
		{ok     -width 6 -text OK     -underline 0}
		{cancel -width 6 -text Cancel -underline 0}
	    }
	}
	retrycancel {
	    set buttons {
		{retry  -width 6 -text Retry  -underline 0}
		{cancel -width 6 -text Cancel -underline 0}
	    }
	}
	yesno {
	    set buttons {
		{yes    -width 6 -text Yes -underline 0}
		{no     -width 6 -text No  -underline 0}
	    }
	}
	yesnocancel {
	    set buttons {
		{yes    -width 6 -text Yes -underline 0}
		{no     -width 6 -text No  -underline 0}
		{cancel -width 6 -text Cancel -underline 0}
	    }
	}
	default {
	    error "invalid message box type \"$data(-type)\", must be abortretryignore, ok, okcancel, retrycancel, yesno or yesnocancel"
	}
    }
    if {[string compare $data(-default) ""]} {
	set valid 0
	foreach btn $buttons {
	    if {![string compare [lindex $btn 0] $data(-default)]} {
		set valid 1
		break
	    }
	}
	if {!$valid} {
	    error "invalid default button \"$data(-default)\""
	}
    }
    # 2. Set the dialog to be a child window of $parent
    if {[string compare $data(-parent) .]} {
	set w $data(-parent).__ck__messagebox
    } else {
	set w .__ck__messagebox
    }

    # 3. Create the top-level window and divide it into top
    # and bottom parts.
    catch {destroy $w}
    toplevel $w -class Dialog \
        -border { ulcorner hline urcorner vline lrcorner hline llcorner vline }
    place $w -relx 0.5 -rely 0.5 -anchor center
    label $w.title -text $data(-title)
    pack $w.title -side top -fill x
    frame $w.bot
    pack $w.bot -side bottom -fill both
    frame $w.top
    pack $w.top -side top -fill both -expand 1
    # 4. Fill the top part with bitmap and message (use the option
    # database for -wraplength so that it can be overridden by
    # the caller).
    message $w.top.msg -text $data(-message) -aspect 1000
    pack $w.top.msg -side right -expand 1 -fill both -padx 1 -pady 1
    # 5. Create a row of buttons at the bottom of the dialog.
    set i 0
    foreach but $buttons {
	set name [lindex $but 0]
	set opts [lrange $but 1 end]
	if {![string compare $opts {}]} {
	    # Capitalize the first letter of $name
	    set capName \
		[string toupper \
		    [string index $name 0]][string range $name 1 end]
	    set opts [list -text $capName]
	}
	eval button $w.bot.$name $opts \
	    -command [list [list set ckPriv(button) $name]]
	pack $w.bot.$name -side left -expand 1 -padx 1 -pady 1
	# create the binding for the key accelerator, based on the underline
	set underIdx [$w.bot.$name cget -underline]
	if {$underIdx >= 0} {
	    set key [string index [$w.bot.$name cget -text] $underIdx]
	    bind $w [string tolower $key] [list $w.bot.$name invoke]
	    bind $w [string toupper $key] [list $w.bot.$name invoke]
	}
	incr i
    }
    # 6. Create a binding for <Return> on the dialog if there is a
    # default button.
    if {[string compare $data(-default) ""]} {
	bind $w <Return> "ckButtonInvoke $w.bot.$data(-default) ; break"
	bind $w <Linefeed> "ckButtonInvoke $w.bot.$data(-default) ; break"
    }
    # 7. Claim the focus.
    set oldFocus [focus]
    if {[string compare $data(-default) ""]} {
	focus $w.bot.$data(-default)
    } else {
	focus [lindex [winfo children $w.bot] 0]
    }
    # 8. Wait for the user to respond, then restore the focus and
    # return the index of the selected button.  Restore the focus
    # before deleting the window, since otherwise the window manager
    # may take the focus away so we can't redirect it.  Finally,
    # restore any grab that was in effect.
    tkwait variable ckPriv(button)
    catch {focus $oldFocus}
    destroy $w
    return $ckPriv(button)
}
