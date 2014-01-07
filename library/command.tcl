#
# command.tcl --
#
# This file defines the command dialog procedure.
#
# Copyright (c) 1995-1996 Christian Werner
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

# ckCommand --
#    Create command window e.g. for interactive use of cwsh

proc ckCommand {{w .ckCommand}} {
    global ckPriv
    if [winfo exists $w] {
	raise $w
        return
    }
    toplevel $w -class CommandDialog \
        -border { ulcorner hline urcorner vline lrcorner hline llcorner vline }
    place $w -relx 0.5 -rely 0.5 -relwidth 0.5 -relheight 0.5 -anchor center

    label $w.title -text "Command dialog"
    place $w.title -y 0 -relx 0.5 -bordermode ignore -anchor center

    entry $w.entry
    frame $w.sep0 -border hline -height 1
    scrollbar $w.scroll -command "$w.output yview" -takefocus 0
    text $w.output -yscrollcommand "$w.scroll set"
    frame $w.sep1 -border hline -height 1
    button $w.close -command "lower $w" -text Dismiss

    pack $w.entry -side top -fill x
    pack $w.sep0 -side top -fill x
    pack $w.close -side bottom -ipadx 1
    pack $w.sep1 -side bottom -fill x
    pack $w.scroll -side right -fill y
    pack $w.output -side left -fill both -expand 1

    bind $w.entry <Return> "ckCommandRun $w"
    bind $w.entry <Linefeed> "ckCommandRun $w"
    bind $w.entry <Up> "ckCmdHist $w 1"
    bind $w.entry <Down> "ckCmdHist $w -1"
    bind $w.output <Tab> {focus [ck_focusNext %W] ; break}
    bind $w.output <Control-X> "ckCommandRun $w \[$w.output get 1.0 end\]"
    bind $w <Escape> "lower $w ; break"
    bind $w <Control-U> "ckCmdToggleSize $w"    
    bind $w <Control-L> {update screen}

    focus $w.entry

    set ckPriv(cmdHistory) {}
    set ckPriv(cmdHistCnt) -1
    set ckPriv(cmdHistMax) 32
}

proc ckCmdToggleSize w {
    if {[string first "-relwidth 1" [place info $w]] >= 0} {
        place $w -relx 0.5 -rely 0.5 -relwidth 0.5 -relheight 0.5 \
            -anchor center
    } else {
        place $w -relx 0.5 -rely 0.5 -relwidth 1.0 -relheight 1.0 \
            -anchor center
    }
}

proc ckCmdHist {w dir} {
    global ckPriv
    incr ckPriv(cmdHistCnt) $dir
    if {$ckPriv(cmdHistCnt) < 0} {
        set cmd ""
        set ckPriv(cmdHistCnt) -1
    } else {
        if {$ckPriv(cmdHistCnt) >= [llength $ckPriv(cmdHistory)]} {
            set ckPriv(cmdHistCnt) [expr [llength $ckPriv(cmdHistory)] - 1]
            return
        }
        set cmd [lindex $ckPriv(cmdHistory) $ckPriv(cmdHistCnt)]
    }
    $w.entry delete 0 end
    $w.entry insert end $cmd
}

proc ckCommandRun {w {cmd {}}} {
    global errorInfo ckPriv
    if {$cmd == ""} {
        set cmd [string trim [$w.entry get]]
        if {$cmd == ""} {
            return
        }
    }
    set code [catch {uplevel #0 $cmd} result]
    if {$code == 0} {
        set ckPriv(cmdHistory) [lrange [concat [list $cmd] \
            $ckPriv(cmdHistory)] 0 $ckPriv(cmdHistMax)]
        set ckPriv(cmdHistCnt) -1
    }
    $w.output delete 1.0 end
    $w.output insert 1.0 $result
    if $code { $w.output insert end "\n----\n$errorInfo" }
    $w.output mark set insert 1.0
    if {$code == 0} {
        $w.entry delete 0 end
    }
}
