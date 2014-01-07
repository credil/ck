/* 
 * ks_names.h --
 *
 *	Key symbols, associated values and terminfo names.
 *
 * Copyright (c) 1995 Christian Werner.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef KEY_BACKSPACE
{ "BackSpace", KEY_BACKSPACE, "kbs" },
#else
{ "BackSpace", 0x008, NULL },
#endif
#ifdef KEY_DC
{ "Delete", KEY_DC, "kdch1" },
#else
{ "Delete", 0x07f, NULL },
#endif
{ "Tab", 0x009, NULL },
{ "Linefeed", 0x00a, NULL },
{ "Return", 0x00d, NULL },
{ "Escape", 0x01b, NULL },
{ "ASCIIDelete", 0x07f, NULL },
#ifdef KEY_HOME
{ "Home", KEY_HOME, "khome" },
#endif
#ifdef KEY_LEFT
{ "Left", KEY_LEFT, "kcub1" },
#endif
#ifdef KEY_UP
{ "Up", KEY_UP, "kcuu1" },
#endif
#ifdef KEY_RIGHT
{ "Right", KEY_RIGHT, "kcuf1" },
#endif
#ifdef KEY_DOWN
{ "Down", KEY_DOWN, "kcud1" },
#endif
#ifdef KEY_PPAGE
{ "Prior", KEY_PPAGE, "kpp" },
#endif
#ifdef KEY_NPAGE
{ "Next", KEY_NPAGE, "knp" },
#endif
#ifdef KEY_END
{ "End", KEY_END, "kend" },
#endif
#ifdef KEY_BEG
{ "Begin", KEY_BEG, "kbeg" },
#endif
#ifdef KEY_SELECT
{ "Select", KEY_SELECT, "kslt" },
#endif
#ifdef KEY_PRINT
{ "Print", KEY_PRINT, "kprt" },
#endif
#ifdef KEY_COMMAND
{ "Execute", KEY_COMMAND, "kcmd" },
#endif
#ifdef KEY_IC
{ "Insert", KEY_IC, "kich1" },
#endif
#ifdef KEY_UNDO
{ "Undo", KEY_UNDO, "kund" },
#endif
#ifdef KEY_REDO
{ "Redo", KEY_REDO, "krdo" },
#endif
#ifdef KEY_OPTIONS
{ "Menu", KEY_OPTIONS, "kopt" },
#endif
#ifdef KEY_REFERENCE
{ "Find", KEY_REFERENCE, "kref" },
#endif
#ifdef KEY_BTAB
{ "BackTab", KEY_BTAB, "kcbt" },
#endif
#ifdef KEY_CANCEL
{ "Cancel", KEY_CANCEL, "kcan" },
#endif
#ifdef KEY_HELP
{ "Help", KEY_HELP, "khlp" },
#endif
#ifdef KEY_F
{ "F1", KEY_F(1), "kf1" },
{ "F2", KEY_F(2), "kf2" },
{ "F3", KEY_F(3), "kf3" },
{ "F4", KEY_F(4), "kf4" },
{ "F5", KEY_F(5), "kf5" },
{ "F6", KEY_F(6), "kf6" },
{ "F7", KEY_F(7), "kf7" },
{ "F8", KEY_F(8), "kf8" },
{ "F9", KEY_F(9), "kf9" },
{ "F10", KEY_F(10), "kf10" },
{ "L1", KEY_F(11), "kf11" },
{ "F11", KEY_F(11), "kf11" },
{ "L2", KEY_F(12), "kf12" },
{ "F12", KEY_F(12), "kf12" },
{ "L3", KEY_F(13), "kf13" },
{ "F13", KEY_F(13), "kf13" },
{ "L4", KEY_F(14), "kf14" },
{ "F14", KEY_F(14), "kf14" },
{ "L5", KEY_F(15), "kf15" },
{ "F15", KEY_F(15), "kf15" },
{ "L6", KEY_F(16), "kf16" },
{ "F16", KEY_F(16), "kf16" },
{ "L7", KEY_F(17), "kf17" },
{ "F17", KEY_F(17), "kf17" },
{ "L8", KEY_F(18), "kf18" },
{ "F18", KEY_F(18), "kf18" },
{ "L9", KEY_F(19), "kf19" },
{ "F19", KEY_F(19), "kf19" },
{ "L10", KEY_F(20), "kf20" },
{ "F20", KEY_F(20), "kf20" },
{ "R1", KEY_F(21), "kf21" },
{ "F21", KEY_F(21), "kf21" },
{ "R2", KEY_F(22), "kf22" },
{ "F22", KEY_F(22), "kf22" },
{ "R3", KEY_F(23), "kf23" },
{ "F23", KEY_F(23), "kf23" },
{ "R4", KEY_F(24), "kf24" },
{ "F24", KEY_F(24), "kf24" },
{ "R5", KEY_F(25), "kf25" },
{ "F25", KEY_F(25), "kf25" },
{ "R6", KEY_F(26), "kf26" },
{ "F26", KEY_F(26), "kf26" },
{ "R7", KEY_F(27), "kf27" },
{ "F27", KEY_F(27), "kf27" },
{ "R8", KEY_F(28), "kf28" },
{ "F28", KEY_F(28), "kf28" },
{ "R9", KEY_F(29), "kf29" },
{ "F29", KEY_F(29), "kf29" },
{ "R10", KEY_F(30), "kf30" },
{ "F30", KEY_F(30), "kf30" },
{ "R11", KEY_F(31), "kf31" },
{ "F31", KEY_F(31), "kf31" },
{ "R12", KEY_F(32), "kf32" },
{ "F32", KEY_F(32), "kf32" },
{ "R13", KEY_F(33), "kf33" },
{ "F33", KEY_F(33), "kf33" },
{ "R14", KEY_F(34), "kf34" },
{ "F34", KEY_F(34), "kf34" },
{ "R15", KEY_F(35), "kf35" },
{ "F35", KEY_F(35), "kf35" },
#endif
#ifdef KEY_SUSPEND
{ "Suspend", KEY_SUSPEND, "kspd" },
#endif
{ "space", 0x020, NULL },
{ "exclam", 0x021, NULL },
{ "quotedbl", 0x022, NULL },
{ "numbersign", 0x023, NULL },
{ "dollar", 0x024, NULL },
{ "percent", 0x025, NULL },
{ "ampersand", 0x026, NULL },
{ "quoteright", 0x027, NULL },
{ "parenleft", 0x028, NULL },
{ "parenright", 0x029, NULL },
{ "asterisk", 0x02a, NULL },
{ "plus", 0x02b, NULL },
{ "comma", 0x02c, NULL },
{ "minus", 0x02d, NULL },
{ "period", 0x02e, NULL },
{ "slash", 0x02f, NULL },
{ "0", 0x030, NULL },
{ "1", 0x031, NULL },
{ "2", 0x032, NULL },
{ "3", 0x033, NULL },
{ "4", 0x034, NULL },
{ "5", 0x035, NULL },
{ "6", 0x036, NULL },
{ "7", 0x037, NULL },
{ "8", 0x038, NULL },
{ "9", 0x039, NULL },
{ "colon", 0x03a, NULL },
{ "semicolon", 0x03b, NULL },
{ "less", 0x03c, NULL },
{ "equal", 0x03d, NULL },
{ "greater", 0x03e, NULL },
{ "question", 0x03f, NULL },
{ "at", 0x040, NULL },
{ "A", 0x041, NULL },
{ "B", 0x042, NULL },
{ "C", 0x043, NULL },
{ "D", 0x044, NULL },
{ "E", 0x045, NULL },
{ "F", 0x046, NULL },
{ "G", 0x047, NULL },
{ "H", 0x048, NULL },
{ "I", 0x049, NULL },
{ "J", 0x04a, NULL },
{ "K", 0x04b, NULL },
{ "L", 0x04c, NULL },
{ "M", 0x04d, NULL },
{ "N", 0x04e, NULL },
{ "O", 0x04f, NULL },
{ "P", 0x050, NULL },
{ "Q", 0x051, NULL },
{ "R", 0x052, NULL },
{ "S", 0x053, NULL },
{ "T", 0x054, NULL },
{ "U", 0x055, NULL },
{ "V", 0x056, NULL },
{ "W", 0x057, NULL },
{ "X", 0x058, NULL },
{ "Y", 0x059, NULL },
{ "Z", 0x05a, NULL },
{ "bracketleft", 0x05b, NULL },
{ "backslash", 0x05c, NULL },
{ "bracketright", 0x05d, NULL },
{ "asciicircum", 0x05e, NULL },
{ "underscore", 0x05f, NULL },
{ "quoteleft", 0x060, NULL },
{ "a", 0x061, NULL },
{ "b", 0x062, NULL },
{ "c", 0x063, NULL },
{ "d", 0x064, NULL },
{ "e", 0x065, NULL },
{ "f", 0x066, NULL },
{ "g", 0x067, NULL },
{ "h", 0x068, NULL },
{ "i", 0x069, NULL },
{ "j", 0x06a, NULL },
{ "k", 0x06b, NULL },
{ "l", 0x06c, NULL },
{ "m", 0x06d, NULL },
{ "n", 0x06e, NULL },
{ "o", 0x06f, NULL },
{ "p", 0x070, NULL },
{ "q", 0x071, NULL },
{ "r", 0x072, NULL },
{ "s", 0x073, NULL },
{ "t", 0x074, NULL },
{ "u", 0x075, NULL },
{ "v", 0x076, NULL },
{ "w", 0x077, NULL },
{ "x", 0x078, NULL },
{ "y", 0x079, NULL },
{ "z", 0x07a, NULL },
{ "braceleft", 0x07b, NULL },
{ "bar", 0x07c, NULL },
{ "braceright", 0x07d, NULL },
{ "asciitilde", 0x07e, NULL },

