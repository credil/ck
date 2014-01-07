# keylpr --
# Pretty print the contents of a keyed list
#
# Copyright (c) 1996 Christian Werner
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.


# Internal function which recursively collects the keys in a keyed list.
# Don't remove the blank before "proc", it prevents "auto_mkindex" from
# creating an entry for this procedure.

 proc keylpr__ {list keynames prefix} {
    upvar 1 $keynames names
    upvar 1 $list l
    set m [keylkeys l $prefix]
    set x [string length $prefix]
    foreach k $m {
        if $x {
            set p ${prefix}.$k
        } else {
            set p $k
        }
        if ![keylpr__ l names $p] {
            lappend names $p
        }
    }
    return [llength $m]
}

# Keyed list pretty printer, returns string with pretty printed keyed
# list. Parameters
#
#    keylname       name of keyed list variable to be formatted
#    prefix (opt)   component of keyed list to be formatted or empty
#                   to format entire keyed list

proc keylpr {keylname {prefix {}}} {
    upvar 1 $keylname keylist
    set keys {}
    keylpr__ keylist keys $prefix
    set maxlength 0
    set keys [lsort -ascii $keys]
    foreach k $keys {
        set l [string length $k]
        if {$l > $maxlength} {
            set maxlength $l
        }
    }
    set fmt "%-${maxlength}s    %s"
    set r {}
    set q {}
    foreach k $keys {
        if ![catch {format $fmt $k [keylget keylist $k]} l] {
            append r $q $l
            set q "\n" 
        }
    }
    return $r
}
