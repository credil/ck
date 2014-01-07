# comdlg.tcl --
#
#	Some functions needed for the common dialog boxes. Probably need to go
#	in a different file.
#
# RCS: @(#) $Id: comdlg.tcl,v 1.1 1999/09/02 08:54:01 chw Exp chw $
#
# Copyright (c) 1996 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

# tclParseConfigSpec --
#
#	Parses a list of "-option value" pairs. If all options and
#	values are legal, the values are stored in
#	$data($option). Otherwise an error message is returned. When
#	an error happens, the data() array may have been partially
#	modified, but all the modified members of the data(0 array are
#	guaranteed to have valid values. This is different than
#	Tk_ConfigureWidget() which does not modify the value of a
#	widget record if any error occurs.
#
# Arguments:
#
# w = widget record to modify. Must be the pathname of a widget.
#
# specs = {
#    {-commandlineswitch resourceName ResourceClass defaultValue verifier}
#    {....}
# }
#
# flags = currently unused.
#
# argList = The list of  "-option value" pairs.
#
proc tclParseConfigSpec {w specs flags argList} {
    upvar #0 $w data

    # 1: Put the specs in associative arrays for faster access
    #
    foreach spec $specs {
	if {[llength $spec] < 4} {
	    error "\"spec\" should contain 5 or 4 elements"
	}
	set cmdsw [lindex $spec 0]
	set cmd($cmdsw) ""
	set rname($cmdsw)   [lindex $spec 1]
	set rclass($cmdsw)  [lindex $spec 2]
	set def($cmdsw)     [lindex $spec 3]
	set verproc($cmdsw) [lindex $spec 4]
    }

    if {([llength $argList]%2) != 0} {
	foreach {cmdsw value} $argList {
	    if {![info exists cmd($cmdsw)]} {
	        error "unknown option \"$cmdsw\", must be [tclListValidFlags cmd]"
	    }
	}
	error "value for \"[lindex $argList end]\" missing"
    }

    # 2: set the default values
    #
    foreach cmdsw [array names cmd] {
	set data($cmdsw) $def($cmdsw)
    }

    # 3: parse the argument list
    #
    foreach {cmdsw value} $argList {
	if {![info exists cmd($cmdsw)]} {
	    error "unknown option \"$cmdsw\", must be [tclListValidFlags cmd]"
	}
	set data($cmdsw) $value
    }

    # Done!
}

proc tclListValidFlags {v} {
    upvar $v cmd

    set len [llength [array names cmd]]
    set i 1
    set separator ""
    set errormsg ""
    foreach cmdsw [lsort [array names cmd]] {
	append errormsg "$separator$cmdsw"
	incr i
	if {$i == $len} {
	    set separator " or "
	} else {
	    set separator ", "
	}
    }
    return $errormsg
}

# This procedure is used to sort strings in a case-insenstive mode.
#
proc tclSortNoCase {str1 str2} {
    return [string compare [string toupper $str1] [string toupper $str2]]
}


# Gives an error if the string does not contain a valid integer
# number
#
proc tclVerifyInteger {string} {
    lindex {1 2 3} $string
}

# ckFDGetFileTypes --
#
#	Process the string given by the -filetypes option of the file
#	dialogs. Similar to the C function TkGetFileFilters() on the Mac
#	and Windows platform.
#
proc ckFDGetFileTypes {string} {
    foreach t $string {
	if {[llength $t] < 2 || [llength $t] > 3} {
	    error "bad file type \"$t\", should be \"typeName {extension ?extensions ...?} ?{macType ?macTypes ...?}?\""
	}
	eval lappend [list fileTypes([lindex $t 0])] [lindex $t 1]
    }

    set types {}
    foreach t $string {
	set label [lindex $t 0]
	set exts {}

	if {[info exists hasDoneType($label)]} {
	    continue
	}

	set name "$label ("
	set sep ""
	foreach ext $fileTypes($label) {
	    if {![string compare $ext ""]} {
		continue
	    }
	    regsub {^[.]} $ext "*." ext
	    if {![info exists hasGotExt($label,$ext)]} {
		append name $sep$ext
		lappend exts $ext
		set hasGotExt($label,$ext) 1
	    }
	    set sep ,
	}
	append name ")"
	lappend types [list $name $exts]

	set hasDoneType($label) 1
    }

    return $types
}
