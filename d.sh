#
# Write out list of default widget options
#

defscript1() {
    cat >/tmp/d$$.tcl <<'EOD'
set count 0
foreach c [lsort -ascii [info commands]] {
    if {$c == "puts" || $c == "vwait"} {
        continue
    }
    if {[catch {$c .w$count} tmp]} {
        continue
    }
    if {"$tmp" != ".w$count"} {
        continue
    }
    if {[catch {.w$count configure} clist]} {
        incr count
        continue
    }
    puts stderr [format "\n\t\t\t%s\n" $c]
    foreach i [lsort -ascii $clist] {
        if {[llength $i] > 2} {
            puts stderr [format "%-35s\t%s" [lindex $i 0] [lindex $i 3]]
        }
    }
    incr count
}
exit 0
EOD
echo /tmp/d$$.tcl
}

#
# Write out list of class bindings
#

defscript2() {
    cat >/tmp/d$$.tcl <<'EOD'
proc all w { return $w }
set count 0
foreach c [lsort -ascii [info commands]] {
    if {$c == "puts" || $c == "vwait"} {
        continue
    }
    if {[catch {$c .w$count} tmp]} {
        continue
    }
    if {"$tmp" != ".w$count"} {
        continue
    }
    set class $c
    if {$class != "all" && [catch {winfo class .w$count} class]} {
        incr count
        continue
    }
    puts stderr [format "\n\t\t\t%s\n" $class]
    set out ""
    set icnt 0
    foreach i [lsort -ascii [bind $class]] {
        append out [format "%-19.19s" $i]
        incr icnt
        if {$icnt % 4 == 0} {
            append out "\n"
        } else {
            append out " "
        }
    }
    if {$out == ""} {
        set out "*** no events bound to class ***"
    }
    puts stderr [string trimright $out "\n"]
    incr count
}
exit 0
EOD
echo /tmp/d$$.tcl
}


SCRIPT=`defscript1`

rm -f def.list
exec 2>def.list
echo "Terminals w/ color" >&2
echo "------------------" >&2
TERM=color_xterm ./cwsh $SCRIPT
echo -e "\f" >&2
echo "Terminals w/o color" >&2
echo "-------------------" >&2
TERM=vt100 ./cwsh $SCRIPT
rm -f $SCRIPT

SCRIPT=`defscript2`

echo -e "\f" >&2
echo "Events bound to classes" >&2
echo "-----------------------" >&2
TERM=vt100 ./cwsh $SCRIPT
rm -f $SCRIPT

