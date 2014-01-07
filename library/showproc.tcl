#
# showproc.tcl --
#
# This file defines a command for retrieving Tcl procedures.
#
# Copyright (c) 1996 Christian Werner
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

proc showproc args {
    set result {}
    if {[llength $args] == 0} {
	set args *
    }
    foreach i $args {
	foreach k [info procs $i] {
	    set procs($k) {}
	}
    }
    foreach i [lsort -ascii [array names procs]] {
	set proc proc
	lappend proc $i
	set args {}
	foreach k [info args $i] {
	    if [info default $i $k value] {
		lappend args [list $k $value]
	    } else {
		lappend args $k
	    }
	}
	lappend proc $args
	lappend proc [info body $i]
	lappend result $proc
    }
    return [join $result "\n\n"]
}


