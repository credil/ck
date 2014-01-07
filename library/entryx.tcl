# entryx.tcl --
#
# This file defines the additional bindings for entry widgets and provides
# procedures that help in implementing those bindings.
#
# Copyright (c) 1992-1994 The Regents of the University of California.
# Copyright (c) 1994-1995 Sun Microsystems, Inc.
# Copyright (c) 1995 Christian Werner
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

#-------------------------------------------------------------------------
# Create extended entry widget.
#-------------------------------------------------------------------------

proc entryx {w args} {
    global ckPriv errorInfo

    set mode EntryNormal
    set index [lsearch -glob $args -mod*]
    set regexp ".*"
    if {$index >= 0} {
        set mode [lindex $args [expr $index + 1]]
        switch -glob -- $mode {
            in* {
                set mode EntryInteger
                set regexp {^[-+]?[0-9]*$}
            }
            un* {
                set mode EntryUnsigned
                set regexp {^[0-9]*$}
            }
            fl* {
                set mode EntryFloat
                set regexp {^[-+]?(([0-9]*(\.)[0-9]*)|([0-9]*))$}
            }
            re* {
                set mode EntryRegexp
            }
            no* {
                set mode EntryNormal
            }
            bo* {
                set mode EntryBoolean
	    }
            default {
                return -code error -errorinfo \
"bad mode \"$mode\": should be boolean, integer, unsigned, float, regexp or normal"
            }
	}
        set args [lreplace $args $index [expr $index + 1]]
    }


    set index [lsearch -glob $args -reg*]
    if {$index >= 0} {
        set regexp [lindex $args [expr $index + 1]]
        if [catch {regexp -- $regexp foo}] {
            return -code error -errorinfo "bad regexp \"$regexp\""
	}
        set args [lreplace $args $index [expr $index + 1]]
    }

    set initial ""
    set index [lsearch -glob $args -ini*]
    if {$index >= 0} {
        set initial [lindex $args [expr $index + 1]]
        set args [lreplace $args $index [expr $index + 1]]
    }

    set touchvar ""
    set index [lsearch -glob $args -to*]
    if {$index >= 0} {
        set touchvar [lindex $args [expr $index + 1]]
        set args [lreplace $args $index [expr $index + 1]]
    }

    set default ""
    set index [lsearch -glob $args -de*]
    if {$index >= 0} {
        set default [lindex $args [expr $index + 1]]
        set args [lreplace $args $index [expr $index + 1]]
    }

    if {$mode == "EntryBoolean"} {
        set onval 1
        set offval 0

        set index [lsearch -glob $args -onv*]
        if {$index >= 0} {
            set onval [lindex $args [expr $index + 1]]
            set args [lreplace $args $index [expr $index + 1]]
        }
        set index [lsearch -glob $args -offv*]
        if {$index >= 0} {
            set offval [lindex $args [expr $index + 1]]
            set args [lreplace $args $index [expr $index + 1]]
        }
    }

    set fieldwidth 100000
    set index [lsearch -glob $args -fie*]
    if {$index >= 0} {
        set value [lindex $args [expr $index + 1]]
        if {[catch {expr int($value)} fieldwidth] || $fieldwidth <= 0} {
            return -code error -errorinfo "bad fieldwidth \"$value\""
        }
        set args [lreplace $args $index [expr $index + 1]]
    }
 
    if [catch {eval entry $w $args} ret] {
        return -code error -errorinfo $errorInfo
    }

    set ckPriv(entryx$ret,fw) $fieldwidth
    if {$touchvar != ""} {
	upvar #0 $touchvar tv
	if ![catch {set tv 0}] {
	    set ckPriv(entryx$ret,tv) $touchvar
	}
    }
    set ckPriv(entryx$ret,re) $regexp
    set ckPriv(entryx$ret,de) $default

    if {$mode == "EntryBoolean"} {
        set ckPriv(entryx$ret,t) [string toupper [string index $onval 0]]
        set ckPriv(entryx$ret,f) [string toupper [string index $offval 0]]
    }

    bindtags $ret [list $ret $mode [winfo toplevel $ret] all]

    if {$initial != ""} {
        $ret delete 0 end
        $ret insert end $initial
    }

    return $ret
}

#-------------------------------------------------------------------------
# The code below creates the class bindings for extended entries.
#-------------------------------------------------------------------------

bind EntryInteger  <Destroy> {ckEntryDestroy %W}
bind EntryUnsigned <Destroy> {ckEntryDestroy %W}
bind EntryFloat    <Destroy> {ckEntryDestroy %W}
bind EntryRegexp   <Destroy> {ckEntryDestroy %W}
bind EntryNormal   <Destroy> {ckEntryDestroy %W}
bind EntryBoolean  <Destroy> {ckEntryDestroy %W}

bind EntryInteger  <FocusIn> {ckEntryFocus %W 1 Integer}
bind EntryUnsigned <FocusIn> {ckEntryFocus %W 1 Unsigned}
bind EntryFloat    <FocusIn> {ckEntryFocus %W 1 Float}
bind EntryRegexp   <FocusIn> {ckEntryFocus %W 1 Regexp}
bind EntryNormal   <FocusIn> {ckEntryFocus %W 1 Normal}
bind EntryBoolean  <FocusIn> {ckEntryFocus %W 1 Boolean}

bind EntryInteger  <FocusOut> {ckEntryFocus %W 0 Integer}
bind EntryUnsigned <FocusOut> {ckEntryFocus %W 0 Unsigned}
bind EntryFloat    <FocusOut> {ckEntryFocus %W 0 Float}
bind EntryRegexp   <FocusOut> {ckEntryFocus %W 0 Regexp}
bind EntryNormal   <FocusOut> {ckEntryFocus %W 0 Normal}
bind EntryBoolean  <FocusOut> {ckEntryFocus %W 0 Boolean}

bind EntryInteger  <Left> {ckEntryXSetCursor %W [expr [%W index insert] - 1]}
bind EntryUnsigned <Left> {ckEntryXSetCursor %W [expr [%W index insert] - 1]}
bind EntryFloat    <Left> {ckEntryXSetCursor %W [expr [%W index insert] - 1]}
bind EntryRegexp   <Left> {ckEntryXSetCursor %W [expr [%W index insert] - 1]}
bind EntryNormal   <Left> {ckEntryXSetCursor %W [expr [%W index insert] - 1]}
bind EntryBoolean  <Left> {focus [ck_focusPrev %W]}

bind EntryInteger  <Right> {ckEntryXSetCursor %W [expr [%W index insert] + 1]}
bind EntryUnsigned <Right> {ckEntryXSetCursor %W [expr [%W index insert] + 1]}
bind EntryFloat    <Right> {ckEntryXSetCursor %W [expr [%W index insert] + 1]}
bind EntryRegexp   <Right> {ckEntryXSetCursor %W [expr [%W index insert] + 1]}
bind EntryNormal   <Right> {ckEntryXSetCursor %W [expr [%W index insert] + 1]}
bind EntryBoolean  <Right> {focus [ck_focusNext %W]}

bind EntryInteger  <BackSpace> {ckEntryXBackspace %W}
bind EntryUnsigned <BackSpace> {ckEntryXBackspace %W}
bind EntryFloat    <BackSpace> {ckEntryXBackspace %W}
bind EntryRegexp   <BackSpace> {ckEntryXBackspace %W}
bind EntryNormal   <BackSpace> {ckEntryXBackspace %W}
bind EntryBoolean  <BackSpace> {# nothing}

bind EntryInteger  <Control-h> {ckEntryXBackspace %W}
bind EntryUnsigned <Control-h> {ckEntryXBackspace %W}
bind EntryFloat    <Control-h> {ckEntryXBackspace %W}
bind EntryRegexp   <Control-h> {ckEntryXBackspace %W}
bind EntryNormal   <Control-h> {ckEntryXBackspace %W}
bind EntryBoolean  <Control-h> {# nothing}

bind EntryInteger  <Delete> {ckEntryXDelete %W}
bind EntryUnsigned <Delete> {ckEntryXDelete %W}
bind EntryFloat    <Delete> {ckEntryXDelete %W}
bind EntryRegexp   <Delete> {ckEntryXDelete %W}
bind EntryNormal   <Delete> {ckEntryXDelete %W}
bind EntryBoolean  <Delete> {# nothing}

bind EntryInteger  <ASCIIDelete> {ckEntryXDelete %W}
bind EntryUnsigned <ASCIIDelete> {ckEntryXDelete %W}
bind EntryFloat    <ASCIIDelete> {ckEntryXDelete %W}
bind EntryRegexp   <ASCIIDelete> {ckEntryXDelete %W}
bind EntryNormal   <ASCIIDelete> {ckEntryXDelete %W}
bind EntryBoolean  <ASCIIDelete> {# nothing}

bind EntryInteger  <Home> {ckEntryXSetCursor %W 0}
bind EntryUnsigned <Home> {ckEntryXSetCursor %W 0}
bind EntryFloat    <Home> {ckEntryXSetCursor %W 0}
bind EntryRegexp   <Home> {ckEntryXSetCursor %W 0}
bind EntryNormal   <Home> {ckEntryXSetCursor %W 0}
bind EntryBoolean  <Home> {# nothing}

bind EntryInteger  <End> {ckEntryXSetCursor %W [expr [%W index end] - 1]}
bind EntryUnsigned <End> {ckEntryXSetCursor %W [expr [%W index end] - 1]}
bind EntryFloat    <End> {ckEntryXSetCursor %W [expr [%W index end] - 1]}
bind EntryRegexp   <End> {ckEntryXSetCursor %W [expr [%W index end] - 1]}
bind EntryNormal   <End> {ckEntryXSetCursor %W [expr [%W index end] - 1]}
bind EntryBoolean  <End> {# nothing}

bind EntryInteger  <Return> {focus [ck_focusNext %W]}
bind EntryUnsigned <Return> {focus [ck_focusNext %W]}
bind EntryFloat    <Return> {focus [ck_focusNext %W]}
bind EntryRegexp   <Return> {focus [ck_focusNext %W]}
bind EntryNormal   <Return> {focus [ck_focusNext %W]}
bind EntryBoolean  <Return> {focus [ck_focusNext %W]}

bind EntryInteger  <Linefeed> {focus [ck_focusNext %W]}
bind EntryUnsigned <Linefeed> {focus [ck_focusNext %W]}
bind EntryFloat    <Linefeed> {focus [ck_focusNext %W]}
bind EntryRegexp   <Linefeed> {focus [ck_focusNext %W]}
bind EntryNormal   <Linefeed> {focus [ck_focusNext %W]}
bind EntryBoolean  <Linefeed> {focus [ck_focusNext %W]}

bind EntryInteger  <Tab> {# nothing}
bind EntryUnsigned <Tab> {# nothing}
bind EntryFloat    <Tab> {# nothing}
bind EntryRegexp   <Tab> {# nothing}
bind EntryNormal   <Tab> {# nothing}
bind EntryBoolean  <Tab> {# nothing}

bind EntryInteger  <BackTab> {# nothing}
bind EntryUnsigned <BackTab> {# nothing}
bind EntryFloat    <BackTab> {# nothing}
bind EntryRegexp   <BackTab> {# nothing}
bind EntryNormal   <BackTab> {# nothing}
bind EntryBoolean  <BackTab> {# nothing}

bind EntryInteger  <Escape> {# nothing}
bind EntryUnsigned <Escape> {# nothing}
bind EntryFloat    <Escape> {# nothing}
bind EntryRegexp   <Escape> {# nothing}
bind EntryNormal   <Escape> {# nothing}
bind EntryBoolean  <Escape> {# nothing}

bind EntryInteger  <Control> {# nothing}
bind EntryUnsigned <Control> {# nothing}
bind EntryFloat    <Control> {# nothing}
bind EntryRegexp   <Control> {# nothing}
bind EntryNormal   <Control> {# nothing}
bind EntryBoolean  <Control> {# nothing}

bind EntryInteger  <KeyPress> {ckEntryInput %W %A}
bind EntryUnsigned <KeyPress> {ckEntryInput %W %A}
bind EntryFloat    <KeyPress> {ckEntryInput %W %A}
bind EntryRegexp   <KeyPress> {ckEntryInput %W %A}
bind EntryNormal   <KeyPress> {ckEntryInput %W %A}
bind EntryBoolean  <KeyPress> {ckEntryBooleanInput %W %A}

bind EntryInteger <Button-1> {
    if [ckFocusOK %W] {%W icursor @%x ; focus %W}
}
bind EntryUnsigned <Button-1> {
    if [ckFocusOK %W] {%W icursor @%x ; focus %W}
}
bind EntryFloat <Button-1> {
    if [ckFocusOK %W] {%W icursor @%x ; focus %W}
}
bind EntryRegexp <Button-1> {
    if [ckFocusOK %W] {%W icursor @%x ; focus %W}
}
bind EntryNormal <Button-1> {
    if [ckFocusOK %W] {%W icursor @%x ; focus %W}
}
bind EntryBoolean <Button-1> {
    if [ckFocusOK %W] {%W icursor @%x ; focus %W}
}

# ckEntryDestroy --
# If entry has been destroyed, cleanup parts of global ckPriv array
#
# Arguments:
# w -		The entry window

proc ckEntryDestroy w {
    global ckPriv
    unset ckPriv(entryx$w,fw)
    unset ckPriv(entryx$w,re)
    unset ckPriv(entryx$w,de)
    catch {unset ckPriv(entryx%W,tv)}
    catch {unset ckPriv(entryx%W,t)}
    catch {unset ckPriv(entryx%W,f)}
}

# ckEntryTouched --
# If entry has a touch variable assigned, this variable is asserted here.
#
# Arguments:
# w -		The entry window

proc ckEntryTouched w {
    global ckPriv
    if [info exists ckPriv(entryx$w,tv)] {
	upvar #0 $ckPriv(entryx$w,tv) var
	set var 1
    }
}

# ckEntryFocus --
# For FocusIn set reverse on mono screens or swap foreground/background
# on color screens and position insertion cursor in first column of entry.
# For FocusOut restore attributes or colors.
#
# Arguments:
# w -		The entry window
# focus -	1=FocusIn or 0=FocusOut
# mode -        Type of entryx, e.g. Integer etc.

proc ckEntryFocus {w focus mode} {
    global ckPriv
    if {[winfo depth $w] == 1} {
        if $focus {
            set ckPriv(entryxAttr) [$w cget -attributes]
            $w configure -attributes reverse
        } else {
            $w configure -attributes $ckPriv(entryxAttr)
	}
    } else {
        if $focus {
            set ckPriv(entryxFg) [$w cget -foreground]
            set ckPriv(entryxBg) [$w cget -background]
            $w configure -foreground $ckPriv(entryxBg) \
                -background $ckPriv(entryxFg)
        } else {
            $w configure -foreground $ckPriv(entryxFg) \
                -background $ckPriv(entryxBg)
	}
    }
    if $focus {
        $w icursor 0
        ckEntryXSeeInsert $w
    } else {
        switch -glob -- $mode {
            Integer - Unsigned {
                set val [$w get]
                $w delete 0 end
                if [scan $val %d val] {
                    $w insert end $val
                } else {
                    $w insert end $ckPriv(entryx$w,de)
                }
            }
            Float {
                set val [$w get]
                $w delete 0 end
                if [scan $val %f val] {
                    $w insert end $val
                } else {
                    $w insert end $ckPriv(entryx$w,de)
                }
            }
        }
    }
}

# ckEntryXSetCursor -
# Move the insertion cursor to a given position in an entry.
# Makes sure that the insertion cursor is visible.
#
# Arguments:
# w -           The entry window.
# pos -         The desired new position for the cursor in the window.

proc ckEntryXSetCursor {w pos} {
    $w icursor $pos
    ckEntryXSeeInsert $w
}

# ckEntryXSeeInsert --
# Make sure that the insertion cursor is visible in the entry window.
# If not, adjust the view so that it is. If the cursor is at the very
# end of the fieldwidth, advance focus to next window.
#
# Arguments:
# w -           The entry window.

proc ckEntryXSeeInsert w {
    global ckPriv
    set c [$w index insert]
    set left [$w index @0]
    if {$left > $c} {
        $w xview $c
        return
    }
    set x [winfo width $w]
    while {([$w index @$x] <= $c) && ($left < $c)} {
        incr left
        $w xview $left
    }
    if {$c >= $ckPriv(entryx$w,fw)} {
	$w icursor [expr $ckPriv(entryx$w,fw) - 1]
    }
    if {$c >= $ckPriv(entryx$w,fw)} {
        set c [expr $ckPriv(entryx$w,fw) - 1]
	$w icursor $c
        $w xview [expr $c - $x]
    }
}

# ckEntryXBackspace --
# Backspace over the character just before the insertion cursor.
# If backspacing would move the cursor off the left edge of the
# window, reposition the cursor at about the middle of the window.
#
# Arguments:
# w -           The entry window in which to backspace.

proc ckEntryXBackspace w {
    set x [expr {[$w index insert] - 1}]
    if {$x >= 0} {
	$w delete $x
	ckEntryTouched $w
    }
    if {[$w index @0] >= [$w index insert]} {
        set range [$w xview]
        set left [lindex $range 0]
        set right [lindex $range 1]
        $w xview moveto [expr $left - ($right - $left)/2.0]
    }
}

# ckEntryXDelete --
# Delete the character at the insertion cursor.
#
# Arguments:
# w -           The entry window in which to backspace.

proc ckEntryXDelete w {
    set a [$w index insert]
    set b [$w index end]
    if {$a != $b && $b != 0} {
	$w delete $a
	ckEntryTouched $w
    }
}

# ckEntryBooleanInput --
#
# Arguments:
# w -		The entry window in which to insert the string
# s -		The string to insert

proc ckEntryBooleanInput {w s} {
    global ckPriv
    set old [$w get]
    set s [string toupper $s]
    if {[string compare " " $s] == 0} {
        if {[string compare $ckPriv(entryx$w,t) $old] == 0} {
            set s $ckPriv(entryx$w,f)
        } else {
            set s $ckPriv(entryx$w,t)
        }
    } elseif {[string compare $ckPriv(entryx$w,t) $s] != 0 && \
        [string compare $ckPriv(entryx$w,f) $s] != 0} {
        return
    }
    if {[string compare $s $old] != 0} {
    	$w delete 0 end
    	$w insert 0 $s
	$w icursor 0
    	ckEntryTouched $w
    }
}

# ckEntryInput --
#
#    Input string into entry (types Integer, Unsigned, Float, Regexp, Normal).
#    Handling of blanks is as follows:
#      1. try to enter blank into the string, if result is a valid regexp
#      2. otherwise try to delete rest of field, if result is a valid regexp
#      3. ignore the blank input
#    Lower case characters are converted to upper case, if regexp denies
#    lower case characters.
#
# Arguments:
# w -		The entry window in which to insert the string
# s -		The string to insert

proc ckEntryInput {w s} {
    global ckPriv
    if {$s == ""} return
    set insert [$w index insert]
    if {$insert >= $ckPriv(entryx$w,fw)} return
    set save [$w get]
    $w insert insert $s
    $w delete insert
    if {![regexp $ckPriv(entryx$w,re) [$w get]]} {
        set ok 1
        if {$s == " "} {
            $w icursor [expr [$w index insert] - 1]
            $w delete insert end
            ckEntryTouched $w
            ckEntryXSeeInsert $w
            return
        } elseif {[string match {[a-z]} $s]} {
            $w icursor $insert
            $w insert insert [string toupper $s]
	    $w delete insert
        } else {
            set ok 0
        }
        if {$ok} {
	    set ok [regexp $ckPriv(entryx$w,re) [$w get]]
        }
        if {!$ok} {
	    $w delete 0 end
	    $w insert end $save
	    $w icursor $insert
	    ckEntryXSeeInsert $w
	    bell
	    return
	}
    }
    ckEntryTouched $w
    ckEntryXSeeInsert $w
}
