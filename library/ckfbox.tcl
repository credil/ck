# ckfbox.tcl --
#
#	Implements the "CK" standard file selection dialog box.
#
# Copyright (c) 1994-1996 Sun Microsystems, Inc.
# Copyright (c) 1999-2000 Christian Werner
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

proc ck_getOpenFile args {
    eval ckFDialog open $args
}

proc ck_getSaveFile args {
    eval ckFDialog save $args
}

# ckFDialog --
#
#	Implements the file selection dialog.

proc ckFDialog {type args} {
    global ckPriv
    set w __ck_filedialog
    upvar #0 $w data
    ckFDialog_Config $w $type $args
    if {![string compare $data(-parent) .]} {
        set w .$w
    } else {
        set w $data(-parent).$w
    }
    # (re)create the dialog box if necessary
    if {![winfo exists $w]} {
	ckFDialog_Create $w
    } elseif {[string compare [winfo class $w] CkFDialog]} {
	destroy $w
	ckFDialog_Create $w
    } else {
	set data(dirMenuBtn) $w.f1.menu
	set data(dirMenu) $w.f1.menu.menu
	set data(upBtn) $w.f1.up
	set data(list) $w.list
	set data(ent) $w.f2.ent
	set data(typeMenuLab) $w.f3.lab
	set data(typeMenuBtn) $w.f3.menu
	set data(typeMenu) $data(typeMenuBtn).m
	set data(okBtn) $w.f2.ok
	set data(cancelBtn) $w.f3.cancel
    }
    # Initialize the file types menu
    if {$data(-filetypes) != {}} {
	$data(typeMenu) delete 0 end
	foreach type $data(-filetypes) {
	    set title  [lindex $type 0]
	    set filter [lindex $type 1]
	    $data(typeMenu) add command -label $title \
		-command [list ckFDialog_SetFilter $w $type]
	}
	ckFDialog_SetFilter $w [lindex $data(-filetypes) 0]
	$data(typeMenuBtn) config -state normal -takefocus 1
	$data(typeMenuLab) config -state normal
    } else {
	set data(filter) "*"
	$data(typeMenuBtn) config -state disabled -takefocus 0
	$data(typeMenuLab) config -state disabled
    }
    ckFDialog_UpdateWhenIdle $w
    place forget $w
    place $w -relx 0.5 -rely 0.5 -anchor center
    set oldFocus [focus]
    focus $data(ent)
    $data(ent) delete 0 end
    $data(ent) insert 0 $data(selectFile)
    $data(ent) select from 0
    $data(ent) select to end
    $data(ent) icursor end
    tkwait variable ckPriv(selectFilePath)
    catch {focus $oldFocus}
    destroy $w
    return $ckPriv(selectFilePath)
}

# ckFDialog_Config --
#
#	Configures the filedialog according to the argument list
#
proc ckFDialog_Config {w type argList} {
    upvar #0 $w data
    set data(type) $type
    # 1. the configuration specs
    set specs {
	{-defaultextension "" "" ""}
	{-filetypes "" "" ""}
	{-initialdir "" "" ""}
	{-initialfile "" "" ""}
	{-parent "" "" "."}
	{-title "" "" ""}
    }
    # 2. default values depending on the type of the dialog
    if {![info exists data(selectPath)]} {
	# first time the dialog has been popped up
	set data(selectPath) [pwd]
	set data(selectFile) ""
    }
    # 3. parse the arguments
    tclParseConfigSpec $w $specs "" $argList
    if {![string compare $data(-title) ""]} {
	if {![string compare $type "open"]} {
	    set data(-title) "Open"
	} else {
	    set data(-title) "Save As"
	}
    }
    # 4. set the default directory and selection according to the -initial
    #    settings
    if {[string compare $data(-initialdir) ""]} {
	if {[file isdirectory $data(-initialdir)]} {
	    set data(selectPath) [glob $data(-initialdir)]
	} else {
	    set data(selectPath) [pwd]
	}
	# Convert the initialdir to an absolute path name.
	set old [pwd]
	cd $data(selectPath)
	set data(selectPath) [pwd]
	cd $old
    }
    set data(selectFile) $data(-initialfile)
    # 5. Parse the -filetypes option
    set data(-filetypes) [ckFDGetFileTypes $data(-filetypes)]
    if {![winfo exists $data(-parent)]} {
	error "bad window path name \"$data(-parent)\""
    }
}

proc ckFDialog_Create {w} {
    set dataName [lindex [split $w .] end]
    upvar #0 $dataName data
    toplevel $w -class CkFDialog -border {
	ulcorner hline urcorner vline lrcorner hline llcorner vline
    }
    # f1: the frame with the directory option menu
    set f1 [frame $w.f1]
    label $f1.lab -text "Directory:" -underline 0
    set data(dirMenuBtn) $f1.menu
    set data(dirMenu) [ck_optionMenu $f1.menu [format %s(selectPath) $dataName] ""]
    set data(upBtn) [button $f1.up -text Up -width 4 -underline 0]
    pack $data(upBtn) -side right -padx 1 -fill both
    pack $f1.lab -side left -padx 4 -fill both
    pack $f1.menu -expand yes -fill both -padx 1
    frame $w.sep0 -border hline -height 1
    set data(list) [listbox $w.list -selectmode browse -height 8]
    bindtags $data(list) [list Listbox $data(list) $w all]
    bind $data(list) <Button-1> [list ckFDialog_ListBrowse $w]
    bind $data(list) <KeyPress> [list ckFDialog_ListBrowse $w]
    bind $data(list) <space> [list ckFDialog_ListBrowse $w]
    bind $data(list) <Return> [list ckFDialog_ListInvoke $w]
    bind $data(list) <Linefeed> [list ckFDialog_ListInvoke $w]
    frame $w.sep1 -border hline -height 1
    # f2: the frame with the OK button and the "file name" field
    set f2 [frame $w.f2]
    label $f2.lab -text "File name:" -anchor e -width 14 -underline 5
    set data(ent) [entry $f2.ent]
    # f3: the frame with the cancel button and the file types field
    set f3 [frame $w.f3]
    # The "File of types:" label needs to be grayed-out when
    # -filetypes are not specified. The label widget does not support
    # grayed-out text on monochrome displays. Therefore, we have to
    # use a button widget to emulate a label widget (by setting its
    # bindtags)
    set data(typeMenuLab) [button $f3.lab -text "Files of type:" \
	-anchor e -width 14 -underline 9 -takefocus 0]
    bindtags $data(typeMenuLab) [list $data(typeMenuLab) Label \
	    [winfo toplevel $data(typeMenuLab)] all]
    set data(typeMenuBtn) [menubutton $f3.menu -menu $f3.menu.m]
    $f3.menu config -takefocus 1 \
	-disabledbackground [$f3.menu cget -background] \
	-disabledforeground [$f3.menu cget -foreground]
    bind $f3.menu <FocusIn> {
	if {[%W cget -state] != "disabled"} {
	    %W configure -state active
	}
    }
    bind $f3.menu <FocusOut> {
	if {[%W cget -state] != "disabled"} {
	    %W configure -state normal
	}
    }
    set data(typeMenu) [menu $data(typeMenuBtn).m -border {
	ulcorner hline urcorner vline lrcorner hline llcorner vline}]
    $data(typeMenuBtn) config -takefocus 1 -anchor w
    # the okBtn is created after the typeMenu so that the keyboard traversal
    # is in the right order
    set data(okBtn) [button $f2.ok -text OK -underline 0 -width 6]
    set data(cancelBtn) [button $f3.cancel -text Cancel -underline 0 -width 6]
    # pack the widgets in f2 and f3
    pack $data(okBtn) -side right -padx 1 -anchor e
    pack $f2.lab -side left -padx 1
    pack $f2.ent -expand 1 -fill x
    pack $data(cancelBtn) -side right -padx 1 -anchor w
    pack $data(typeMenuLab) -side left -padx 1
    pack $data(typeMenuBtn) -expand 1 -fill x -side right
    # Pack all the frames together. We are done with widget construction.
    pack $f1 -side top -fill x
    pack $w.sep0 -side top -fill x
    pack $f3 -side bottom -fill x
    pack $f2 -side bottom -fill x
    pack $w.sep1 -side bottom -fill x
    pack $data(list) -expand 1 -fill both -padx 1
    # Set up the event handlers
    bind $data(ent) <Return> "ckFDialog_ActivateEnt $w"
    bind $data(ent) <Linefeed> "ckFDialog_ActivateEnt $w"
    $data(upBtn) config -command "ckFDialog_UpDirCmd $w"
    $data(okBtn) config -command "ckFDialog_OkCmd $w"
    $data(cancelBtn) config -command "ckFDialog_CancelCmd $w"
    trace variable data(selectPath) w "ckFDialog_SetPath $w"
    bind $w <Control-d> "focus $data(dirMenuBtn) ; break"
    bind $w <Control-t> [format {
	if {"[%s cget -state]" == "normal"} {
	    focus %s
	}
    } $data(typeMenuBtn) $data(typeMenuBtn)]
    bind $w <Control-n> "focus $data(ent) ; break"
    bind $w <Escape> "ckButtonInvoke $data(cancelBtn)"
    bind $w <Control-c> "ckButtonInvoke $data(cancelBtn) ; break"
    bind $w <Control-o> "ckFDialog_InvokeBtn $w Open ; break"
    bind $w <Control-s> "ckFDialog_InvokeBtn $w Save ; break"
    bind $w <Control-u> "ckFDialog_UpDirCmd $w ; break"
}

# ckFDialog_UpdateWhenIdle --
#
#	Creates an idle event handler which updates the dialog in idle
#	time. This is important because loading the directory may take a long
#	time and we don't want to load the same directory for multiple times
#	due to multiple concurrent events.

proc ckFDialog_UpdateWhenIdle {w} {
    upvar #0 [winfo name $w] data
    if {[info exists data(updateId)]} {
	return
    } else {
	set data(updateId) [after idle ckFDialog_Update $w]
    }
}

# ckFDialog_Update --
#
#	Loads the files and directories into listbox. Also
#	sets up the directory option menu for quick access to parent
#	directories.

proc ckFDialog_Update {w} {
    global tcl_version
    # This proc may be called within an idle handler. Make sure that the
    # window has not been destroyed before this proc is called
    if {![winfo exists $w] || [string compare [winfo class $w] CkFDialog]} {
	return
    }
    set dataName [winfo name $w]
    upvar #0 $dataName data
    global ckPriv
    catch {unset data(updateId)}
    set appPWD [pwd]
    if {[catch {
	cd $data(selectPath)
    }]} {
	# We cannot change directory to $data(selectPath). $data(selectPath)
	# should have been checked before ckFDialog_Update is called, so
	# we normally won't come to here. Anyways, give an error and abort
	# action.
	ck_messageBox -type ok -parent $data(-parent) -message \
	    "Cannot change to the directory \"$data(selectPath)\".\nPermission denied."
	cd $appPWD
	return
    }
    update idletasks
    $data(list) delete 0 end
    # Make the dir list
    if {$tcl_version >= 8.0} {
	set sortmode -dictionary
    } else {
	set sortmode -ascii
    }
    foreach f [lsort $sortmode [glob -nocomplain .* *]] {
	if {![string compare $f .]} {
	    continue
	}
	if {![string compare $f ..]} {
	    continue
	}
	if {[file isdir ./$f]} {
	    if {![info exists hasDoneDir($f)]} {
		$data(list) insert end [format "(dir) %s" $f]
		set hasDoneDir($f) 1
	    }
	}
    }
    # Make the file list
    #
    if {![string compare $data(filter) *]} {
	set files [lsort $sortmode \
	    [glob -nocomplain .* *]]
    } else {
	set files [lsort $sortmode \
	    [eval glob -nocomplain $data(filter)]]
    }

    set top 0
    foreach f $files {
	if {![file isdir ./$f]} {
	    if {![info exists hasDoneFile($f)]} {
		$data(list) insert end [format "      %s" $f]
		set hasDoneFile($f) 1
	    }
	}
    }
    $data(list) selection clear 0 end
    $data(list) selection set 0
    $data(list) activate 0
    $data(list) yview 0
    # Update the Directory: option menu
    set list ""
    set dir ""
    foreach subdir [file split $data(selectPath)] {
	set dir [file join $dir $subdir]
	lappend list $dir
    }
    $data(dirMenu) delete 0 end
    set var [format %s(selectPath) $dataName]
    foreach path $list {
	$data(dirMenu) add command -label $path -command [list set $var $path]
    }
    # Restore the PWD to the application's PWD
    cd $appPWD
}

# ckFDialog_SetPathSilently --
#
# 	Sets data(selectPath) without invoking the trace procedure

proc ckFDialog_SetPathSilently {w path} {
    upvar #0 [winfo name $w] data
    trace vdelete  data(selectPath) w "ckFDialog_SetPath $w"
    set data(selectPath) $path
    trace variable data(selectPath) w "ckFDialog_SetPath $w"
}

# This proc gets called whenever data(selectPath) is set

proc ckFDialog_SetPath {w name1 name2 op} {
    if {[winfo exists $w]} {
	upvar #0 [winfo name $w] data
	ckFDialog_UpdateWhenIdle $w
    }
}

# This proc gets called whenever data(filter) is set

proc ckFDialog_SetFilter {w type} {
    upvar #0 [winfo name $w] data
    set data(filter) [lindex $type 1]
    $data(typeMenuBtn) config -text [lindex $type 0] -indicatoron 0
    ckFDialog_UpdateWhenIdle $w
}

# ckFDialogResolveFile --
#
#	Interpret the user's text input in a file selection dialog.
#	Performs:
#
#	(1) ~ substitution
#	(2) resolve all instances of . and ..
#	(3) check for non-existent files/directories
#	(4) check for chdir permissions
#
# Arguments:
#	context:  the current directory you are in
#	text:	  the text entered by the user
#	defaultext: the default extension to add to files with no extension
#
# Return vaue:
#	[list $flag $directory $file]
#
#	 flag = OK	: valid input
#	      = PATTERN	: valid directory/pattern
#	      = PATH	: the directory does not exist
#	      = FILE	: the directory exists by the file doesn't
#			  exist
#	      = CHDIR	: Cannot change to the directory
#	      = ERROR	: Invalid entry
#
#	 directory      : valid only if flag = OK or PATTERN or FILE
#	 file           : valid only if flag = OK or PATTERN
#
#	directory may not be the same as context, because text may contain
#	a subdirectory name

proc ckFDialogResolveFile {context text defaultext} {
    set appPWD [pwd]
    set path [ckFDialog_JoinFile $context $text]
    if {[file ext $path] == ""} {
	set path "$path$defaultext"
    }
    if {[catch {file exists $path}]} {
	# This "if" block can be safely removed if the following code
	# stop generating errors.
	#
	#	file exists ~nonsuchuser
	#
	return [list ERROR $path ""]
    }
    if {[file exists $path]} {
	if {[file isdirectory $path]} {
	    if {[catch {
		cd $path
	    }]} {
		return [list CHDIR $path ""]
	    }
	    set directory [pwd]
	    set file ""
	    set flag OK
	    cd $appPWD
	} else {
	    if {[catch {
		cd [file dirname $path]
	    }]} {
		return [list CHDIR [file dirname $path] ""]
	    }
	    set directory [pwd]
	    set file [file tail $path]
	    set flag OK
	    cd $appPWD
	}
    } else {
	set dirname [file dirname $path]
	if {[file exists $dirname]} {
	    if {[catch {
		cd $dirname
	    }]} {
		return [list CHDIR $dirname ""]
	    }
	    set directory [pwd]
	    set file [file tail $path]
	    if {[regexp {[*]|[?]} $file]} {
		set flag PATTERN
	    } else {
		set flag FILE
	    }
	    cd $appPWD
	} else {
	    set directory $dirname
	    set file [file tail $path]
	    set flag PATH
	}
    }
    return [list $flag $directory $file]
}

# Gets called when the entry box gets keyboard focus. We clear the selection
# from the icon list . This way the user can be certain that the input in the 
# entry box is the selection.

proc ckFDialog_EntFocusIn {w} {
    upvar #0 [winfo name $w] data
    if {[string compare [$data(ent) get] ""]} {
	$data(ent) selection from 0
	$data(ent) selection to end
	$data(ent) icursor end
    } else {
	$data(ent) selection clear
    }
    $data(list) selection clear 0 end
    if {![string compare $data(type) open]} {
	$data(okBtn) config -text "Open"
    } else {
	$data(okBtn) config -text "Save"
    }
}

proc ckFDialog_EntFocusOut {w} {
    upvar #0 [winfo name $w] data
    $data(ent) selection clear
}

# Gets called when user presses Return in the "File name" entry.

proc ckFDialog_ActivateEnt {w} {
    upvar #0 [winfo name $w] data
    set text [string trim [$data(ent) get]]
    set list [ckFDialogResolveFile $data(selectPath) $text \
		  $data(-defaultextension)]
    set flag [lindex $list 0]
    set path [lindex $list 1]
    set file [lindex $list 2]
    switch -- $flag {
	OK {
	    if {![string compare $file ""]} {
		# user has entered an existing (sub)directory
		set data(selectPath) $path
		$data(ent) delete 0 end
	    } else {
		ckFDialog_SetPathSilently $w $path
		set data(selectFile) $file
		ckFDialog_Done $w
	    }
	}
	PATTERN {
	    set data(selectPath) $path
	    set data(filter) $file
	}
	FILE {
	    if {![string compare $data(type) open]} {
		ck_messageBox -type ok -parent $data(-parent) \
		    -message "File \"[file join $path $file]\" does not exist."
		$data(ent) select from 0
		$data(ent) select to end
		$data(ent) icursor end
	    } else {
		ckFDialog_SetPathSilently $w $path
		set data(selectFile) $file
		ckFDialog_Done $w
	    }
	}
	PATH {
	    ck_messageBox -type ok -parent $data(-parent) \
		-message "Directory \"$path\" does not exist."
	    $data(ent) select from 0
	    $data(ent) select to end
	    $data(ent) icursor end
	}
	CHDIR {
	    ck_messageBox -type ok -parent $data(-parent) -message \
	       "Cannot change to the directory \"$path\".\nPermission denied."
	    $data(ent) select from 0
	    $data(ent) select to end
	    $data(ent) icursor end
	}
	ERROR {
	    ck_messageBox -type ok -parent $data(-parent) -message \
	       "Invalid file name \"$path\"."
	    $data(ent) select from 0
	    $data(ent) select to end
	    $data(ent) icursor end
	}
    }
}

# Gets called when user presses the Alt-s or Alt-o keys.

proc ckFDialog_InvokeBtn {w key} {
    upvar #0 [winfo name $w] data
    if {![string compare [$data(okBtn) cget -text] $key]} {
	ckButtonInvoke $data(okBtn)
    }
}

# Gets called when user presses the "parent directory" button

proc ckFDialog_UpDirCmd {w} {
    upvar #0 [winfo name $w] data
    if {[string compare $data(selectPath) "/"]} {
	set data(selectPath) [file dirname $data(selectPath)]
    }
}

# Join a file name to a path name. The "file join" command will break
# if the filename begins with ~

proc ckFDialog_JoinFile {path file} {
    if {[string match {~*} $file] && [file exists $path/$file]} {
	return [file join $path ./$file]
    } else {
	return [file join $path $file]
    }
}

# Gets called when user presses the "OK" button

proc ckFDialog_OkCmd {w} {
    upvar #0 [winfo name $w] data
    set text ""
    set index [$data(list) curselection]
    if {"$index" != ""} {
	set text [string range [$data(list) get $index] 6 end]
    }
    if {[string compare $text ""]} {
	set file [ckFDialog_JoinFile $data(selectPath) $text]
	if {[file isdirectory $file]} {
	    ckFDialog_ListInvoke $w $text
	    return
	}
    }
    ckFDialog_ActivateEnt $w
}

# Gets called when user presses the "Cancel" button

proc ckFDialog_CancelCmd {w} {
    upvar #0 [winfo name $w] data
    global ckPriv
    set ckPriv(selectFilePath) ""
}

# Gets called when user browses the listbox.

proc ckFDialog_ListBrowse w {
    upvar #0 [winfo name $w] data
    set index [$data(list) curselection]
    set text ""
    if {[string length $index]} {
	set text [string range [$data(list) get $index] 6 end]
    }
    if {[string length $text] == 0} {
	return
    }
    set file [ckFDialog_JoinFile $data(selectPath) $text]
    if {![file isdirectory $file]} {
	$data(ent) delete 0 end
	$data(ent) insert 0 $text
	if {![string compare $data(type) open]} {
	    $data(okBtn) config -text "Open"
	} else {
	    $data(okBtn) config -text "Save"
	}
    } else {
	$data(okBtn) config -text "Open"
    }
}

# Gets called when user invokes the lisbox.

proc ckFDialog_ListInvoke {w {text {}}} {
    upvar #0 [winfo name $w] data
    if {[string length $text] == 0} {
	set index [$data(list) curselection]
	if {[string length $index]} {
	    set text [string range [$data(list) get $index] 6 end]
	}
    }
    if {[string length $text] == 0} {
	return
    }
    set file [ckFDialog_JoinFile $data(selectPath) $text]
    if {[file isdirectory $file]} {
	set appPWD [pwd]
	if {[catch {cd $file}]} {
	    ck_messageBox -type ok -parent $data(-parent) -message \
	       "Cannot change to the directory \"$file\".\nPermission denied."
	} else {
	    cd $appPWD
	    set data(selectPath) $file
	}
    } else {
	set data(selectFile) $file
	ckFDialog_Done $w
    }
}

# ckFDialog_Done --
#
#	Gets called when user has input a valid filename. Pops up a
#	dialog box to confirm selection when necessary. Sets the
#	ckPriv(selectFilePath) variable, which will break the "tkwait"
#	loop in ckFDialog and return the selected filename to the
#	script that calls ck_getOpenFile or ck_getSaveFile

proc ckFDialog_Done {w {selectFilePath ""}} {
    upvar #0 [winfo name $w] data
    global ckPriv
    if {![string compare $selectFilePath ""]} {
	set selectFilePath [ckFDialog_JoinFile $data(selectPath) \
		$data(selectFile)]
	set ckPriv(selectFile) $data(selectFile)
	set ckPriv(selectPath) $data(selectPath)
	if {[file exists $selectFilePath] && 
	    ![string compare $data(type) save]} {
		set reply [ck_messageBox -icon warning -type yesno\
			-parent $data(-parent) -message "File\
			\"$selectFilePath\" already exists.\nDo\
			you want to overwrite it?"]
		if {![string compare $reply "no"]} {
		    return
		}
	}
    }
    set ckPriv(selectFilePath) $selectFilePath
}

