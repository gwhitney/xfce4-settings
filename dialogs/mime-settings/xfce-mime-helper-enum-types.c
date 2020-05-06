
/* This file is generated by glib-mkenums, do not modify it. This code is licensed under the same license as the containing project. Note that it links to GLib, so must comply with the LGPL linking clauses. */

#undef GTK_DISABLE_DEPRECATED
#define GTK_ENABLE_BROKEN
#include "exo-helper.h"

#include "xfce-mime-helper-enum-types.h"

/* enumerations from "exo-helper.h" */
GType
exo_helper_category_get_type (void)
{
	static GType type = 0;
	if (type == 0) {
	static const GEnumValue values[] = {
	{ EXO_HELPER_WEBBROWSER, "EXO_HELPER_WEBBROWSER", "WebBrowser" },
	{ EXO_HELPER_MAILREADER, "EXO_HELPER_MAILREADER", "MailReader" },
	{ EXO_HELPER_FILEMANAGER, "EXO_HELPER_FILEMANAGER", "FileManager" },
	{ EXO_HELPER_TERMINALEMULATOR, "EXO_HELPER_TERMINALEMULATOR", "TerminalEmulator" },
	{ 0, NULL, NULL }
	};
	type = g_enum_register_static ("ExoHelperCategory", values);
  }
	return type;
}

/* Generated data ends here */