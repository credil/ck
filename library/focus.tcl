# focus.tcl --
#
# This file defines several procedures for managing the input
# focus.
#
# Copyright (c) 1994-1995 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

set ckPriv(restrictWindow) {}

# ck_focusNext --
# This procedure returns the name of the next window after "w" in
# "focus order" (the window that should receive the focus next if
# Tab is typed in w).  "Next" is defined by a pre-order search
# of the current window and its descendants, with the stacking
# order determining the order of siblings.  The "-takefocus" options
# on windows determine whether or not they should be skipped.
#
# Arguments:
# w -		Name of a window.

proc ck_focusNext w {
    global ckPriv
    set cur $w
    while 1 {

	# Descend to just before the first child of the current widget.

	set parent $cur
	set children [winfo children $cur]
	set i -1

	# Look for the next sibling that isn't a top-level.

	while 1 {
	    incr i
	    if {$i < [llength $children]} {
		set cur [lindex $children $i]
		if {[winfo toplevel $cur] == $cur} {
		    continue
		} else {
		    break
		}
	    }

	    # No more siblings, so go to the current widget's parent.
	    # If it's a top-level, break out of the loop, otherwise
	    # look for its next sibling.

	    set cur $parent
            if {$parent == $ckPriv(restrictWindow)} break
	    if {[winfo toplevel $cur] == $cur} {
		break
	    }
	    set parent [winfo parent $parent]
	    set children [winfo children $parent]
	    set i [lsearch -exact $children $cur]
	}
	if {($cur == $w) || [ckFocusOK $cur]} {
	    return $cur
	}
    }
}

# ck_focusPrev --
# This procedure returns the name of the previous window before "w" in
# "focus order" (the window that should receive the focus next if
# Shift-Tab is typed in w).  "Next" is defined by a pre-order search
# of the current and its descendants, with the stacking
# order determining the order of siblings.  The "-takefocus" options
# on windows determine whether or not they should be skipped.
#
# Arguments:
# w    -	Name of a window.

proc ck_focusPrev w {
    global ckPriv
    set cur $w
    while 1 {

	# Collect information about the current window's position
	# among its siblings.

	# Collect information about the current window's position
	# among its siblings.  Also, if the window is a top-level,
	# then reposition to just after the last child of the window.
    
	if {[winfo toplevel $cur] == $cur || \
	    $cur == $ckPriv(restrictWindow)}  {
	    set parent $cur
	    set children [winfo children $cur]
	    set i [llength $children]
	} else {
	    set parent [winfo parent $cur]
	    set children [winfo children $parent]
	    set i [lsearch -exact $children $cur]
	}
    
	# Go to the previous sibling, then descend to its last descendant
	# (highest in stacking order.  While doing this, ignore top-levels
	# and their descendants.  When we run out of descendants, go up
	# one level to the parent.

	while {$i > 0} {
	    incr i -1
	    set cur [lindex $children $i]
	    if {[winfo toplevel $cur] == $cur} {
		continue
	    }
	    set parent $cur
	    set children [winfo children $parent]
	    set i [llength $children]
	}
	set cur $parent
	if {($cur == $w) || [ckFocusOK $cur]} {
	    return $cur
	}
    }
}

# ckFocusOK --
#
# This procedure is invoked to decide whether or not to focus on
# a given window.  It returns 1 if it's OK to focus on the window,
# 0 if it's not OK.  The code first checks whether the window is
# viewable.  If not, then it never focuses on the window.  Then it
# checks the -takefocus option for the window and uses it if it's
# set.  If there's no -takefocus option, the procedure checks to
# see if (a) the widget isn't disabled, and (b) it has some key
# bindings.  If all of these are true, then 1 is returned.
#
# Arguments:
# w -		Name of a window.

proc ckFocusOK w {
    global ckPriv
    if {![winfo ismapped $w]} {return 0}
    if {$ckPriv(restrictWindow) != ""} {
        if {[winfo toplevel $w] == [winfo toplevel $ckPriv(restrictWindow)]} {
	    if {[string first $ckPriv(restrictWindow) $w] != 0} {return 0}
	}
    }
    set code [catch {$w cget -takefocus} value]
    if {($code == 0) && ($value != "")} {
	if {$value == 0} {
	    return 0
	} elseif {$value == 1} {
	    # For listboxes: don't take focus if nothing selectable
	    if {[winfo class $w] == "Listbox" && [$w size] == 0} {
		return 0
	    }
	    return 1
	} else {
	    set value [uplevel #0 $value $w]
	    if {$value != ""} {
		return $value
	    }
	}
    }
    set code [catch {$w cget -state} value]
    if {($code == 0) && ($value == "disabled")} {
	return 0
    }
    regexp Key|Focus "[bind $w] [bind [winfo class $w]]"
}

# ck_RestrictFocus --
#
# This procedure implements restriction of keyboard focus on a
# subtree of the widget hierarchy not including toplevels within
# that subtree.
#
# Argument formats:
#
# w         -       Name of a window on which the restriction is placed.
# current   -       Returns the current restrict window or an empty
#                   string, if there's no restriction active.
# release w -       If w is the current restrict window, the restriction is
#                   released.

proc ck_RestrictFocus args {
    global ckPriv
    set len [llength $args]
    set opt [lindex $args 0]
    switch -glob -- $opt {
        .* {
            if {$len != 1} {
                error "bad # arguments: must be \"ck_RestrictFocus window\""
            }
            if {![winfo exists $opt]} {
                error "bad window pathname \"$opt\""
            }
            if {![winfo ismapped $opt]} {
                error "window \"$opt\" not viewable"
            }
            set ckPriv(restrictWindow) $opt
            bind $opt <Destroy> {ck_Unrestrict %W}
            bind $opt <Unmap> {ck_Unrestrict %W}
        }
        c* {
            if {$len != 1} {
                error "bad # arguments: must be \"ck_RestrictFocus current\""
            }
            return $ckPriv(restrictWindow)
        }
        r* {
            if {$len != 2} {
                error \
	"bad # arguments: must be \"ck_RestrictFocus release window\""
            }
            set w [lindex $args 1]
            ck_Unrestrict $w
        }
        default {
            error "bad option \"$opt\": must be current or release"
        }
    }
}

# ck_Unrestrict --
#
# This procedure is invoked on Destroy or Unmap events in order
# to release a restriction on a window.
#
# Arguments:
# w -		Name of a window.

proc ck_Unrestrict w {
    global ckPriv
    if {$w == $ckPriv(restrictWindow)} {
        set ckPriv(restrictWindow) {}
    }
    bind $w <Destroy> {# nothing}
    bind $w <Unmap> {# nothing}
}
