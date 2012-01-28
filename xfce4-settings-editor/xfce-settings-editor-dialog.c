/*
 *  xfce4-settings-editor
 *
 *  Copyright (c) 2008      Brian Tarricone <bjt23@cornell.edu>
 *  Copyright (c) 2008      Stephan Arts <stephan@xfce.org>
 *  Copyright (c) 2009-2010 Jérôme Guelfucci <jeromeg@xfce.org>
 *  Copyright (c) 2012      Nick Schermer <nick@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <xfconf/xfconf.h>

#include "xfce-settings-editor-dialog.h"
#include "xfce-settings-prop-dialog.h"
#include "xfce-settings-cell-renderer.h"



struct _XfceSettingsEditorDialogClass
{
    XfceTitledDialogClass __parent__;
};

struct _XfceSettingsEditorDialog
{
    XfceTitledDialog __parent__;

    XfconfChannel     *channel;

    GtkWidget         *paned;

    GtkListStore      *channels_store;
    GtkWidget         *channels_treeview;

    GtkTreeStore      *props_store;
    XfconfChannel     *props_channel;
    GtkWidget         *props_treeview;

    GtkWidget         *button_new;
    GtkWidget         *button_edit;
    GtkWidget         *button_reset;
};



enum
{
    CHANNEL_COLUMN_NAME,
    N_CHANNEL_COLUMNS
};

enum
{
    PROP_COLUMN_FULL,
    PROP_COLUMN_NAME,
    PROP_COLUMN_TYPE_NAME,
    PROP_COLUMN_TYPE,
    PROP_COLUMN_LOCKED,
    PROP_COLUMN_VALUE,
    N_PROP_COLUMNS
};



static GSList         *monitor_dialogs = NULL;
static GtkWindowGroup *monitor_group = NULL;



static void     xfce_settings_editor_dialog_finalize             (GObject                   *object);
static void     xfce_settings_editor_dialog_response             (GtkDialog                 *widget,
                                                                  gint                       response_id);
static void     xfce_settings_editor_dialog_load_channels        (XfceSettingsEditorDialog  *dialog);
static void     xfce_settings_editor_dialog_channel_changed      (GtkTreeSelection          *selection,
                                                                  XfceSettingsEditorDialog  *dialog);
static gboolean xfce_settings_editor_dialog_channel_menu         (XfceSettingsEditorDialog  *dialog);
static gboolean xfce_settings_editor_dialog_channel_button_press (GtkWidget                 *treeview,
                                                                  GdkEventButton            *event,
                                                                  XfceSettingsEditorDialog  *dialog);
static void     xfce_settings_editor_dialog_value_changed        (GtkCellRenderer           *renderer,
                                                                  const gchar               *path,
                                                                  const GValue              *new_value,
                                                                  XfceSettingsEditorDialog  *dialog);
static void     xfce_settings_editor_dialog_selection_changed    (GtkTreeSelection          *selection,
                                                                  XfceSettingsEditorDialog  *dialog);
static gboolean xfce_settings_editor_dialog_query_tooltip        (GtkWidget                 *treeview,
                                                                  gint                       x,
                                                                  gint                       y,
                                                                  gboolean                   keyboard_mode,
                                                                  GtkTooltip                *tooltip,
                                                                  XfceSettingsEditorDialog  *dialog);
static void     xfce_settings_editor_dialog_row_activated        (GtkTreeView               *treeview,
                                                                  GtkTreePath               *path,
                                                                  GtkTreeViewColumn         *column,
                                                                  XfceSettingsEditorDialog  *dialog);
static gboolean xfce_settings_editor_dialog_key_press_event      (GtkTreeView               *treeview,
                                                                  GdkEventKey               *event,
                                                                  XfceSettingsEditorDialog  *dialog);
static void     xfce_settings_editor_dialog_property_new         (XfceSettingsEditorDialog  *dialog);
static void     xfce_settings_editor_dialog_property_edit        (XfceSettingsEditorDialog  *dialog);
static void     xfce_settings_editor_dialog_property_reset       (XfceSettingsEditorDialog  *dialog);



G_DEFINE_TYPE (XfceSettingsEditorDialog, xfce_settings_editor_dialog, XFCE_TYPE_TITLED_DIALOG)



static void
xfce_settings_editor_dialog_class_init (XfceSettingsEditorDialogClass *klass)
{
    GObjectClass   *gobject_class;
    GtkDialogClass *gtkdialog_class;

    gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = xfce_settings_editor_dialog_finalize;

    gtkdialog_class = GTK_DIALOG_CLASS (klass);
    gtkdialog_class->response = xfce_settings_editor_dialog_response;
}



static void
xfce_settings_editor_dialog_init (XfceSettingsEditorDialog *dialog)
{
    GtkWidget         *paned;
    GtkWidget         *content_area;
    GtkWidget         *scroll;
    GtkWidget         *treeview;
    GtkCellRenderer   *render;
    GtkTreeViewColumn *column;
    GtkTreeSelection  *selection;
    GtkWidget         *vbox;
    GtkWidget         *bbox;
    GtkWidget         *button;

    dialog->channel = xfconf_channel_new ("xfce4-settings-editor");

    dialog->channels_store = gtk_list_store_new (N_CHANNEL_COLUMNS,
                                                 G_TYPE_STRING);
    xfce_settings_editor_dialog_load_channels (dialog);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (dialog->channels_store),
                                          CHANNEL_COLUMN_NAME, GTK_SORT_ASCENDING);

    dialog->props_store = gtk_tree_store_new (N_PROP_COLUMNS,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING,
                                              G_TYPE_BOOLEAN,
                                              G_TYPE_VALUE);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (dialog->props_store),
                                          PROP_COLUMN_NAME, GTK_SORT_ASCENDING);

    gtk_window_set_title (GTK_WINDOW (dialog), _("Settings Editor"));
    xfce_titled_dialog_set_subtitle (XFCE_TITLED_DIALOG (dialog), _("Customize settings stored by Xfconf"));
    gtk_window_set_icon_name (GTK_WINDOW (dialog), "preferences-system");
    gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_NORMAL);
    gtk_window_set_default_size (GTK_WINDOW (dialog),
        xfconf_channel_get_int (dialog->channel, "/last/window-width", 640),
        xfconf_channel_get_int (dialog->channel, "/last/window-height", 500));
    gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                            GTK_STOCK_HELP, GTK_RESPONSE_HELP,
                            GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);

    dialog->paned = paned = gtk_hpaned_new ();
    content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    gtk_box_pack_start (GTK_BOX (content_area), paned, TRUE, TRUE, 0);
    gtk_paned_set_position (GTK_PANED (paned),
        xfconf_channel_get_int (dialog->channel, "/last/paned-position", 180));
    gtk_container_set_border_width (GTK_CONTAINER (paned), 6);
    gtk_widget_show (paned);

    scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_ETCHED_IN);
    gtk_paned_add1 (GTK_PANED (paned), scroll);
    gtk_widget_show (scroll);

    treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (dialog->channels_store));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), TRUE);
    gtk_tree_view_set_headers_clickable (GTK_TREE_VIEW (treeview), FALSE);
    gtk_tree_view_set_fixed_height_mode (GTK_TREE_VIEW (treeview), TRUE);
    gtk_tree_view_set_enable_search (GTK_TREE_VIEW (treeview), FALSE);
    gtk_container_add (GTK_CONTAINER (scroll), treeview);
    dialog->channels_treeview = treeview;
    gtk_widget_show (treeview);

    g_signal_connect_swapped (G_OBJECT (treeview), "popup-menu",
        G_CALLBACK (xfce_settings_editor_dialog_channel_menu), dialog);
    g_signal_connect (G_OBJECT (treeview), "button-press-event",
        G_CALLBACK (xfce_settings_editor_dialog_channel_button_press), dialog);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
    g_signal_connect (G_OBJECT (selection), "changed",
        G_CALLBACK (xfce_settings_editor_dialog_channel_changed), dialog);

    render = gtk_cell_renderer_text_new ();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview), 0,
                                                 _("Channel"), render,
                                                 "text", CHANNEL_COLUMN_NAME, NULL);

    vbox = gtk_vbox_new (FALSE, 6);
    gtk_paned_add2 (GTK_PANED (paned), vbox);
    gtk_widget_show (vbox);

    scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_ETCHED_IN);
    gtk_box_pack_start (GTK_BOX (vbox), scroll, TRUE, TRUE, 0);
    gtk_widget_show (scroll);

    treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (dialog->props_store));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), TRUE);
    gtk_tree_view_set_headers_clickable (GTK_TREE_VIEW (treeview), FALSE);
    gtk_tree_view_set_enable_search (GTK_TREE_VIEW (treeview), FALSE);
    gtk_container_add (GTK_CONTAINER (scroll), treeview);
    dialog->props_treeview = treeview;
    gtk_widget_show (treeview);

    gtk_widget_set_has_tooltip (treeview, TRUE);
    g_signal_connect (G_OBJECT (treeview), "query-tooltip",
        G_CALLBACK (xfce_settings_editor_dialog_query_tooltip), dialog);
    g_signal_connect (G_OBJECT (treeview), "row-activated",
        G_CALLBACK (xfce_settings_editor_dialog_row_activated), dialog);
    g_signal_connect (G_OBJECT (treeview), "key-press-event",
        G_CALLBACK (xfce_settings_editor_dialog_key_press_event), dialog);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    g_signal_connect (G_OBJECT (selection), "changed",
        G_CALLBACK (xfce_settings_editor_dialog_selection_changed), dialog);

    render = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (_("Property"), render,
                                                       "text", PROP_COLUMN_NAME,
                                                       NULL);
    gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    render = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (_("Type"), render,
                                                       "text", PROP_COLUMN_TYPE_NAME,
                                                       NULL);
    gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    render = gtk_cell_renderer_toggle_new ();
    column = gtk_tree_view_column_new_with_attributes (_("Locked"), render,
                                                       "active", PROP_COLUMN_LOCKED,
                                                       NULL);
    gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    render = xfce_settings_cell_renderer_new ();
    column = gtk_tree_view_column_new_with_attributes (_("Value"), render,
                                                       "value", PROP_COLUMN_VALUE,
                                                       "locked", PROP_COLUMN_LOCKED,
                                                       NULL);
    gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
    g_signal_connect (G_OBJECT (render), "value-changed",
        G_CALLBACK (xfce_settings_editor_dialog_value_changed), dialog);

    bbox = gtk_hbutton_box_new ();
    gtk_box_pack_start (GTK_BOX (vbox), bbox, FALSE, TRUE, 0);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_START);
    gtk_widget_show (bbox);

    button = gtk_button_new_from_stock (GTK_STOCK_NEW);
    gtk_container_add (GTK_CONTAINER (bbox), button);
    gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text (button, _("New property"));
    gtk_widget_set_sensitive (button, FALSE);
    gtk_widget_set_can_focus (button, FALSE);
    dialog->button_new = button;
    gtk_widget_show (button);
    g_signal_connect_swapped (G_OBJECT (button), "clicked",
        G_CALLBACK (xfce_settings_editor_dialog_property_new), dialog);

    button = gtk_button_new_from_stock (GTK_STOCK_EDIT);
    gtk_container_add (GTK_CONTAINER (bbox), button);
    gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text (button, _("Edit selected property"));
    gtk_widget_set_sensitive (button, FALSE);
    gtk_widget_set_can_focus (button, FALSE);
    dialog->button_edit = button;
    gtk_widget_show (button);
    g_signal_connect_swapped (G_OBJECT (button), "clicked",
        G_CALLBACK (xfce_settings_editor_dialog_property_edit), dialog);

    button = xfce_gtk_button_new_mixed (GTK_STOCK_REVERT_TO_SAVED, _("_Reset"));
    gtk_container_add (GTK_CONTAINER (bbox), button);
    gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text (button, _("Reset selected property"));
    gtk_widget_set_sensitive (button, FALSE);
    gtk_widget_set_can_focus (button, FALSE);
    dialog->button_reset = button;
    gtk_widget_show (button);
    g_signal_connect_swapped (G_OBJECT (button), "clicked",
        G_CALLBACK (xfce_settings_editor_dialog_property_reset), dialog);
}



static void
xfce_settings_editor_dialog_finalize (GObject *object)
{
    GSList *li, *lnext;

    XfceSettingsEditorDialog *dialog = XFCE_SETTINGS_EDITOR_DIALOG (object);

    for (li = monitor_dialogs; li != NULL; li = lnext)
    {
        lnext = li->next;
        gtk_dialog_response (GTK_DIALOG (li->data), GTK_RESPONSE_CLOSE);
    }

    if (monitor_group != NULL)
       g_object_unref (G_OBJECT (monitor_group));

    g_object_unref (G_OBJECT (dialog->channels_store));

    g_object_unref (G_OBJECT (dialog->props_store));
    if (dialog->props_channel != NULL)
        g_object_unref (G_OBJECT (dialog->props_channel));

    g_object_unref (G_OBJECT (dialog->channel));

    G_OBJECT_CLASS (xfce_settings_editor_dialog_parent_class)->finalize (object);
}



static void
xfce_settings_editor_dialog_response (GtkDialog *widget,
                                      gint       response_id)
{
    XfceSettingsEditorDialog *dialog = XFCE_SETTINGS_EDITOR_DIALOG (widget);
    gint                      width, height;
    GdkWindowState            state;

    if (response_id == GTK_RESPONSE_HELP)
    {
        xfce_dialog_show_help (GTK_WINDOW (widget),
                               "xfce4-settings",
                               "settings-editor", NULL);
    }
    else
    {
        /* don't save the state for full-screen windows */
        state = gdk_window_get_state (GTK_WIDGET (widget)->window);
        if ((state & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN)) == 0)
        {
            /* save window size */
            gtk_window_get_size (GTK_WINDOW (widget), &width, &height);
            xfconf_channel_set_int (dialog->channel, "/last/window-width", width),
            xfconf_channel_set_int (dialog->channel, "/last/window-height", height);

            xfconf_channel_set_int (dialog->channel, "/last/paned-position",
                gtk_paned_get_position (GTK_PANED (dialog->paned)));
        }

        gtk_widget_destroy (GTK_WIDGET (widget));
        gtk_main_quit ();
    }
}



static void
xfce_settings_editor_dialog_load_channels (XfceSettingsEditorDialog *dialog)
{
    gchar       **channel_names;
    guint         i;
    gchar        *channel_name = NULL;
    GtkTreePath  *path;
    GtkTreeIter   iter;

    g_return_if_fail (GTK_IS_LIST_STORE (dialog->channels_store));

    /* try to restore the selected name (for reload) */
    if (dialog->props_channel != NULL)
      {
        g_object_get (G_OBJECT (dialog->props_channel),
                      "channel-name", &channel_name, NULL);
      }

    gtk_list_store_clear (dialog->channels_store);

    channel_names = xfconf_list_channels ();
    if (G_LIKELY (channel_names != NULL))
    {
        for (i = 0; channel_names[i] != NULL; i++)
        {
            gtk_list_store_insert_with_values (dialog->channels_store, &iter, i,
                                               CHANNEL_COLUMN_NAME, channel_names[i],
                                               -1);

            if (g_strcmp0 (channel_name, channel_names[i]) == 0)
              {
                path = gtk_tree_model_get_path (GTK_TREE_MODEL (dialog->channels_store), &iter);
                gtk_tree_view_set_cursor (GTK_TREE_VIEW (dialog->channels_treeview), path, NULL, FALSE);
                gtk_tree_path_free (path);
              }
        }
        g_strfreev (channel_names);
    }

    g_free (channel_name);
}



static const gchar *
xfce_settings_editor_dialog_type_name (const GValue *value)
{
    if (G_UNLIKELY (value == NULL))
        return _("Empty");

    if (G_UNLIKELY (G_VALUE_TYPE (value) == xfce_settings_array_type ()))
        return _("Array");

    switch (G_VALUE_TYPE (value))
    {
        case G_TYPE_STRING:
            return _("String");

        /* show non-technical name here, the tooltip
         * contains the full type name */
        case G_TYPE_INT:
        case G_TYPE_UINT:
        case G_TYPE_INT64:
        case G_TYPE_UINT64:
            return _("Integer");

        case G_TYPE_BOOLEAN:
            return _("Boolean");

        case G_TYPE_DOUBLE:
            return _("Double");

        default:
            return G_VALUE_TYPE_NAME (value);
    }
}



static void
xfce_settings_editor_dialog_property_load (const gchar               *property,
                                           const GValue              *value,
                                           XfceSettingsEditorDialog  *dialog,
                                           GtkTreePath              **expand_path)
{
    gchar       **paths;
    guint         i;
    GtkTreeIter   child_iter;
    GtkTreeIter   parent_iter;
    GValue        parent_val = { 0,};
    gboolean      found_parent;
    GValue        string_value = { 0, };
    GtkTreeModel *model = GTK_TREE_MODEL (dialog->props_store);

    g_return_if_fail (GTK_IS_TREE_STORE (dialog->props_store));
    g_return_if_fail (G_IS_VALUE (value));
    g_return_if_fail (property != NULL && *property == '/');

    paths = g_strsplit (property, "/", -1);
    if (paths == NULL)
        return;

    for (i = 1; paths[i] != NULL; i++)
    {
        found_parent = FALSE;

        if (gtk_tree_model_iter_children (model, &child_iter, i == 1 ? NULL : &parent_iter))
        {
            for (;;)
            {
                /* look if one of the paths already exists */
                gtk_tree_model_get_value (model, &child_iter, PROP_COLUMN_NAME, &parent_val);
                found_parent = g_strcmp0 (g_value_get_string (&parent_val), paths[i]) == 0;
                g_value_unset (&parent_val);

                /* maybe we still need to set the value */
                if (found_parent)
                {
                    /* set this property in case it is the last value */
                    if (paths[i + 1] == NULL)
                        goto set_child_values;
                    break;
                }

                /* append at the end of this parent */
                if (!gtk_tree_model_iter_next (model, &child_iter))
                   break;
            }
        }

        if (!found_parent)
        {
            gtk_tree_store_append (GTK_TREE_STORE (model), &child_iter,
                                   i == 1 ? NULL : &parent_iter);

            if (G_LIKELY (paths[i + 1] != NULL))
            {
                gtk_tree_store_set (GTK_TREE_STORE (model), &child_iter,
                                    PROP_COLUMN_NAME, paths[i],
                                    PROP_COLUMN_TYPE_NAME, _("Empty"), -1);
            }
            else
            {
                set_child_values:

                g_value_init (&string_value, G_TYPE_STRING);
                if (!g_value_transform (value, &string_value))
                    g_value_set_string (&string_value, "Unknown");

                gtk_tree_store_set (GTK_TREE_STORE (model), &child_iter,
                                    PROP_COLUMN_NAME, paths[i],
                                    PROP_COLUMN_FULL, property,
                                    PROP_COLUMN_TYPE, G_VALUE_TYPE_NAME (value),
                                    PROP_COLUMN_TYPE_NAME, xfce_settings_editor_dialog_type_name (value),
                                    PROP_COLUMN_LOCKED, xfconf_channel_is_property_locked (dialog->props_channel, property),
                                    PROP_COLUMN_VALUE, value,
                                    -1);

                if (expand_path != NULL)
                    *expand_path = gtk_tree_model_get_path (model, &child_iter);

                g_value_unset (&string_value);
            }
        }

        parent_iter = child_iter;
    }

    g_strfreev (paths);
}



typedef struct
{
    const gchar *prop;
    GtkTreePath *path;
}
DeleteContext;



static gboolean
xfce_settings_editor_dialog_property_find (GtkTreeModel *model,
                                           GtkTreePath  *path,
                                           GtkTreeIter  *iter,
                                           gpointer      data)
{
    GValue         prop = { 0, };
    DeleteContext *context = data;
    gboolean       found = FALSE;

    gtk_tree_model_get_value (model, iter, PROP_COLUMN_FULL, &prop);

    if (g_strcmp0 (g_value_get_string (&prop), context->prop) == 0)
    {
        found = TRUE;
        context->path = gtk_tree_path_copy (path);
    }

    g_value_unset (&prop);

    return found;
}



static void
xfce_settings_editor_dialog_property_changed (XfconfChannel            *channel,
                                              const gchar              *property,
                                              const GValue             *value,
                                              XfceSettingsEditorDialog *dialog)
{
    GtkTreePath      *path = NULL;
    DeleteContext    *context;
    GtkTreeIter       child_iter;
    GtkTreeModel     *model;
    GValue            parent_val = { 0, };
    GtkTreeIter       parent_iter;
    gboolean          empty_prop;
    gboolean          has_parent;
    GtkTreeSelection *selection;

    g_return_if_fail (GTK_IS_TREE_STORE (dialog->props_store));
    g_return_if_fail (XFCONF_IS_CHANNEL (channel));
    g_return_if_fail (dialog->props_channel == channel);

    if (value != NULL && G_IS_VALUE (value))
    {
        xfce_settings_editor_dialog_property_load (property, value, dialog, &path);

        if (path != NULL)
        {
            /* show the new value */
            gtk_tree_view_expand_to_path (GTK_TREE_VIEW (dialog->props_treeview), path);
            gtk_tree_path_free (path);
        }
    }
    else
    {
        /* we only get here when the property must be deleted, this means there
         * is also no reset value in one of the xdg channels */
        context = g_slice_new0 (DeleteContext);
        context->prop = property;

        model = GTK_TREE_MODEL (dialog->props_store);
        gtk_tree_model_foreach (model, xfce_settings_editor_dialog_property_find, context);

        if (context->path != NULL)
        {
            if (gtk_tree_model_get_iter (model, &child_iter, context->path))
            {
                if (gtk_tree_model_iter_has_child (model, &child_iter))
                {
                    /* the node has children, so only unset it */
                    gtk_tree_store_set (GTK_TREE_STORE (model), &child_iter,
                                        PROP_COLUMN_FULL, NULL,
                                        PROP_COLUMN_TYPE, NULL,
                                        PROP_COLUMN_TYPE_NAME, _("Empty"),
                                        PROP_COLUMN_LOCKED, FALSE,
                                        PROP_COLUMN_VALUE, NULL,
                                        -1);
                }
                else
                {
                    /* delete the node */
                    has_parent = gtk_tree_model_iter_parent (model, &parent_iter, &child_iter);
                    gtk_tree_store_remove (GTK_TREE_STORE (model), &child_iter);

                    /* remove the parent nodes if they are empty */
                    while (has_parent)
                    {
                        /* if the parent still has children, stop cleaning */
                        if (gtk_tree_model_iter_has_child (model, &parent_iter))
                            break;

                        /* maybe the parent has a value */
                        gtk_tree_model_get_value (model, &parent_iter, PROP_COLUMN_FULL, &parent_val);
                        empty_prop = g_value_get_string (&parent_val) == NULL;
                        g_value_unset (&parent_val);

                        /* nope it points to a real xfconf property */
                        if (!empty_prop)
                            break;

                        /* get the parent and remove the empty row */
                        child_iter = parent_iter;
                        has_parent = gtk_tree_model_iter_parent (model, &parent_iter, &child_iter);
                        gtk_tree_store_remove (GTK_TREE_STORE (model), &child_iter);
                    }
                }
            }

            gtk_tree_path_free (context->path);
        }

        g_slice_free (DeleteContext, context);
    }

    /* update button sensitivity */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->props_treeview));
    xfce_settings_editor_dialog_selection_changed (selection, dialog);
}



static void
xfce_settings_editor_dialog_property_load_hash (gpointer key,
                                                gpointer value,
                                                gpointer data)
{
    xfce_settings_editor_dialog_property_load (key, value, data, NULL);
}



static void
xfce_settings_editor_dialog_properties_load (XfceSettingsEditorDialog *dialog,
                                             XfconfChannel            *channel)
{
    GHashTable *props;

    g_return_if_fail (GTK_IS_TREE_STORE (dialog->props_store));
    g_return_if_fail (XFCONF_IS_CHANNEL (channel));

    if (dialog->props_channel != NULL)
    {
        g_signal_handlers_block_by_func (G_OBJECT (dialog->props_channel),
            G_CALLBACK (xfce_settings_editor_dialog_property_changed), dialog);
        g_object_unref (G_OBJECT (dialog->props_channel));
        dialog->props_channel = NULL;
    }

    gtk_tree_store_clear (dialog->props_store);

    dialog->props_channel = g_object_ref (G_OBJECT (channel));

    props = xfconf_channel_get_properties (channel, NULL);
    if (G_LIKELY (props != NULL))
    {
        g_hash_table_foreach (props, xfce_settings_editor_dialog_property_load_hash, dialog);
        g_hash_table_destroy (props);
    }

    gtk_tree_view_expand_all (GTK_TREE_VIEW (dialog->props_treeview));

    g_signal_connect (G_OBJECT (dialog->props_channel), "property-changed",
        G_CALLBACK (xfce_settings_editor_dialog_property_changed), dialog);
}



static void
xfce_settings_editor_dialog_channel_changed (GtkTreeSelection         *selection,
                                             XfceSettingsEditorDialog *dialog)
{
    GtkTreeIter    iter;
    GValue         value = { 0, };
    XfconfChannel *channel;
    gboolean       locked;

    if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
        gtk_tree_model_get_value (GTK_TREE_MODEL (dialog->channels_store), &iter,
                                  CHANNEL_COLUMN_NAME, &value);

        channel = xfconf_channel_new (g_value_get_string (&value));

        locked = xfconf_channel_is_property_locked (channel, "/");
        gtk_widget_set_sensitive (dialog->button_new, !locked);

        xfce_settings_editor_dialog_properties_load (dialog, channel);

        g_object_unref (G_OBJECT (channel));

        g_value_unset (&value);
    }
    else
    {
        gtk_widget_set_sensitive (dialog->button_new, FALSE);
        gtk_tree_store_clear (dialog->props_store);
    }
}



static void
xfce_settings_editor_dialog_channel_reset (XfceSettingsEditorDialog *dialog)
{
    gchar             *channel_name;
    GtkTreeSelection  *selection;
    GtkTreeIter        iter;
    GtkTreePath       *path;
    gchar            **channels;
    gboolean           found;
    guint              i;

    if (dialog->props_channel == NULL)
        return;

    g_object_get (dialog->props_channel, "channel-name", &channel_name, NULL);

    if (xfce_dialog_confirm (GTK_WINDOW (dialog),
                             GTK_STOCK_REVERT_TO_SAVED,
                             _("_Reset Channel"),
                             _("Resetting a channel will permanently remove those custom settings."),
                             _("Are you sure you want to reset channel \"%s\" and all its properties?"),
                             channel_name))
    {
        /* reset all channel properties */
        xfconf_channel_reset_property (dialog->props_channel, "/", TRUE);

        /* check if the channel still exists (channel reset, not remove) */
        channels = xfconf_list_channels ();
        if (G_LIKELY (channels != NULL))
        {
            for (i = 0, found = FALSE; !found && channels[i] != NULL; i++)
                found = g_strcmp0 (channels[i], channel_name) == 0;
            g_strfreev (channels);
        }

        if (!found)
        {
            selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->channels_treeview));
            if (gtk_tree_selection_get_selected (selection, NULL, &iter))
            {
                if (gtk_list_store_remove (dialog->channels_store, &iter))
                    path = gtk_tree_model_get_path (GTK_TREE_MODEL (dialog->channels_store), &iter);
                else
                    path = gtk_tree_path_new_first ();

                gtk_tree_view_set_cursor (GTK_TREE_VIEW (dialog->channels_treeview), path, NULL, FALSE);
                gtk_tree_path_free (path);
            }
        }
    }

    g_free (channel_name);
}



static void
xfce_settings_editor_dialog_channel_monitor_changed (XfconfChannel *channel,
                                                     const gchar   *property,
                                                     const GValue  *value,
                                                     GtkWidget     *window)
{
    GtkTextBuffer *buffer;
    GTimeVal       timeval;
    gchar         *str;
    GValue         str_value = { 0, };
    GtkTextIter    iter;

    buffer = g_object_get_data (G_OBJECT (window), "buffer");
    g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

    g_get_current_time (&timeval);

    if (value != NULL && G_IS_VALUE (value))
    {
        g_value_init (&str_value, G_TYPE_STRING);
        if (g_value_transform (value, &str_value))
        {
            str = g_strdup_printf ("%ld: %s (%s: %s)\n",
                                   timeval.tv_sec, property,
                                   G_VALUE_TYPE_NAME (value),
                                   g_value_get_string (&str_value));
        }
        else
        {
            str = g_strdup_printf ("%ld: %s (%s)\n",
                                   timeval.tv_sec, property,
                                   G_VALUE_TYPE_NAME (value));
        }
        g_value_unset (&str_value);
    }
    else
    {
        /* I18N: if a property is removed from the channel */
        str = g_strdup_printf ("%ld: %s (%s)\n", timeval.tv_sec,
                               property, _("reset"));
    }

    /* prepend to the buffer */
    gtk_text_buffer_get_start_iter (buffer, &iter);
    gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, str, -1, "monospace", NULL);
    g_free (str);
}



static void
xfce_settings_editor_dialog_channel_monitor_response (GtkWidget     *window,
                                                      gint           response_id,
                                                      XfconfChannel *channel)
{
    GtkTextBuffer *buffer;

    if (response_id == GTK_RESPONSE_REJECT)
    {
        buffer = g_object_get_data (G_OBJECT (window), "buffer");
        g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
        gtk_text_buffer_set_text (buffer, "", 0);
    }
    else
    {
        g_signal_handlers_disconnect_by_func (G_OBJECT (channel),
            G_CALLBACK (xfce_settings_editor_dialog_channel_monitor_changed), window);

        g_object_unref (G_OBJECT (channel));

        monitor_dialogs = g_slist_remove (monitor_dialogs, window);

        gtk_widget_destroy (window);
    }
}



static void
xfce_settings_editor_dialog_channel_monitor (XfceSettingsEditorDialog *dialog)
{
    GtkWidget     *window;
    gchar         *channel_name;
    gchar         *title;
    GtkWidget     *scroll;
    GtkWidget     *textview;
    GtkWidget     *content_area;
    GtkTextBuffer *buffer;
    GTimeVal       timeval;
    gchar         *str;
    GtkTextIter    iter;

    if (dialog->props_channel == NULL)
        return;

    g_object_get (dialog->props_channel, "channel-name", &channel_name, NULL);
    title = g_strdup_printf (_("Monitor %s"), channel_name);

    window = xfce_titled_dialog_new ();
    gtk_window_set_title (GTK_WINDOW (window), title);
    gtk_window_set_icon_name (GTK_WINDOW (window), "utilities-system-monitor");
    gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
    gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_NORMAL);
    xfce_titled_dialog_set_subtitle (XFCE_TITLED_DIALOG (window),
        _("Watch an Xfconf channel for property changes"));
    gtk_dialog_add_buttons (GTK_DIALOG (window),
                            GTK_STOCK_CLEAR, GTK_RESPONSE_REJECT,
                            GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
    g_signal_connect (G_OBJECT (window), "response",
        G_CALLBACK (xfce_settings_editor_dialog_channel_monitor_response),
        g_object_ref (G_OBJECT (dialog->props_channel)));
    gtk_dialog_set_default_response (GTK_DIALOG (window), GTK_RESPONSE_CLOSE);
    g_free (title);

    monitor_dialogs = g_slist_prepend (monitor_dialogs, window);

    if (monitor_group == NULL)
        monitor_group = gtk_window_group_new ();
    gtk_window_group_add_window (monitor_group, GTK_WINDOW (window));

    scroll = gtk_scrolled_window_new (NULL, NULL);
    content_area = gtk_dialog_get_content_area (GTK_DIALOG (window));
    gtk_box_pack_start (GTK_BOX (content_area), scroll, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (scroll), 6);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_ETCHED_IN);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_show (scroll);

    buffer = gtk_text_buffer_new (NULL);
    g_object_set_data_full (G_OBJECT (window), "buffer", buffer, g_object_unref);
    gtk_text_buffer_create_tag (buffer, "monospace", "font", "monospace", NULL);
    g_signal_connect (G_OBJECT (dialog->props_channel), "property-changed",
        G_CALLBACK (xfce_settings_editor_dialog_channel_monitor_changed), window);

    g_get_current_time (&timeval);
    gtk_text_buffer_get_start_iter (buffer, &iter);
    str = g_strdup_printf ("%ld: ", timeval.tv_sec);
    gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, str, -1, "monospace", NULL);
    g_free (str);

    str = g_strdup_printf (_("start monitoring channel \"%s\""), channel_name);
    gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, str, -1, "monospace", NULL);
    g_free (str);

    textview = gtk_text_view_new_with_buffer (buffer);
    gtk_container_add (GTK_CONTAINER (scroll), textview);
    gtk_text_view_set_editable (GTK_TEXT_VIEW (textview), FALSE);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (textview), FALSE);
    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (textview), GTK_WRAP_NONE);
    gtk_widget_show (textview);

    gtk_window_present_with_time (GTK_WINDOW (window), gtk_get_current_event_time ());

    g_free (channel_name);
}



static gboolean
xfce_settings_editor_dialog_channel_menu (XfceSettingsEditorDialog *dialog)
{
    GtkWidget *menu;
    GtkWidget *mi;
    gchar     *channel_name;
    GtkWidget *image;

    if (dialog->props_channel == NULL)
        return FALSE;

    menu = gtk_menu_new ();
    g_signal_connect (G_OBJECT (menu), "selection-done",
        G_CALLBACK (gtk_widget_destroy), NULL);

    g_object_get (dialog->props_channel, "channel-name", &channel_name, NULL);
    mi = gtk_menu_item_new_with_label (channel_name);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
    gtk_widget_set_sensitive (mi, FALSE);
    gtk_widget_show (mi);
    g_free (channel_name);

    mi = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
    gtk_widget_show (mi);

    mi = gtk_image_menu_item_new_with_mnemonic (_("_Reset"));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
    g_signal_connect_swapped (G_OBJECT (mi), "activate",
        G_CALLBACK (xfce_settings_editor_dialog_channel_reset), dialog);
    gtk_widget_show (mi);

    image = gtk_image_new_from_stock (GTK_STOCK_REVERT_TO_SAVED, GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi), image);
    gtk_widget_show (image);

    mi = gtk_image_menu_item_new_with_mnemonic (_("_Monitor"));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
    gtk_widget_show (mi);

    image = gtk_image_new_from_icon_name ("utilities-system-monitor", GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi), image);
    g_signal_connect_swapped (G_OBJECT (mi), "activate",
        G_CALLBACK (xfce_settings_editor_dialog_channel_monitor), dialog);
    gtk_widget_show (image);

    mi = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
    gtk_widget_show (mi);

    mi = gtk_image_menu_item_new_from_stock (GTK_STOCK_REFRESH, NULL);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
    g_signal_connect_swapped (G_OBJECT (mi), "activate",
        G_CALLBACK (xfce_settings_editor_dialog_load_channels), dialog);
    gtk_widget_show (mi);

    gtk_menu_popup (GTK_MENU (menu),
                    NULL, NULL, NULL, NULL, 3,
                    gtk_get_current_event_time ());

    return TRUE;
}



static gboolean
xfce_settings_editor_dialog_channel_button_press (GtkWidget                *treeview,
                                                  GdkEventButton           *event,
                                                  XfceSettingsEditorDialog *dialog)
{
    guint        modifiers;
    GtkTreePath *path;

    if (event->type == GDK_BUTTON_PRESS)
    {
        modifiers = event->state & gtk_accelerator_get_default_mod_mask ();
        if (event->button == 3 || (event->button == 1 && modifiers == GDK_CONTROL_MASK))
        {
             if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview), event->x, event->y,
                                                &path, NULL, NULL, NULL))
             {
                 gtk_tree_view_set_cursor (GTK_TREE_VIEW (treeview), path, NULL, FALSE);
                 gtk_tree_path_free (path);

                 return xfce_settings_editor_dialog_channel_menu (dialog);;
             }
        }
    }

    return FALSE;
}



static gchar *
xfce_settings_editor_dialog_selected (XfceSettingsEditorDialog *dialog,
                                      gboolean                 *is_real_prop,
                                      gboolean                 *is_array)
{
    GtkTreeSelection *selection;
    GtkTreeIter       iter;
    gchar            *property = NULL;
    GtkTreeModel     *model;
    GtkTreeIter       parent_iter;
    GValue            name_val = { 0, };
    GString          *string_prop;
    gboolean          property_real = TRUE;
    gchar            *type_name;

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->props_treeview));
    if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
        model = GTK_TREE_MODEL (dialog->props_store);
        gtk_tree_model_get (model, &iter, PROP_COLUMN_FULL, &property, -1);

        /* if this is not a real property, look it up by the tree structure */
        if (property == NULL)
        {
            string_prop = g_string_new (NULL);
            for (;;)
            {
                gtk_tree_model_get_value (model, &iter, PROP_COLUMN_NAME, &name_val);
                g_string_prepend (string_prop, g_value_get_string (&name_val));
                g_string_prepend_c (string_prop, '/');
                g_value_unset (&name_val);

                if (!gtk_tree_model_iter_parent (model, &parent_iter, &iter))
                    break;

                iter = parent_iter;
            }
            property = g_string_free (string_prop, FALSE);
            property_real = FALSE;
        }
        else if (is_array != NULL)
        {
            gtk_tree_model_get (model, &iter, PROP_COLUMN_TYPE, &type_name, -1);
            *is_array = g_strcmp0 (type_name, g_type_name (xfce_settings_array_type ())) == 0;
            g_free (type_name);
        }
    }

    if (is_real_prop != NULL)
        *is_real_prop = property_real;

    return property;
}



static void
xfce_settings_editor_dialog_value_changed (GtkCellRenderer          *renderer,
                                           const gchar              *str_path,
                                           const GValue             *new_value,
                                           XfceSettingsEditorDialog *dialog)
{
    GtkTreeModel     *model = GTK_TREE_MODEL (dialog->props_store);
    GtkTreePath      *path;
    GtkTreeIter       iter;
    gchar            *property;
    GtkTreeSelection *selection;

    g_return_if_fail (G_IS_VALUE (new_value));
    g_return_if_fail (XFCE_IS_SETTINGS_EDITOR_DIALOG (dialog));

    /* only change values on selected paths, this to avoid miss clicking */
    path = gtk_tree_path_new_from_string (str_path);
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->props_treeview));
    if (gtk_tree_selection_path_is_selected (selection, path)
        && gtk_tree_model_get_iter (model, &iter, path))
    {
        gtk_tree_model_get (model, &iter, PROP_COLUMN_FULL, &property, -1);
        if (G_LIKELY (property != NULL))
        {
            if (!xfconf_channel_is_property_locked (dialog->props_channel, property))
                xfconf_channel_set_property (dialog->props_channel, property, new_value);
            g_free (property);
        }
    }
    gtk_tree_path_free (path);
}



static void
xfce_settings_editor_dialog_selection_changed (GtkTreeSelection         *selection,
                                               XfceSettingsEditorDialog *dialog)
{
    gchar    *property;
    gboolean  can_edit = FALSE;
    gboolean  can_reset = FALSE;
    gboolean  is_real_prop = TRUE;
    gboolean  is_array = FALSE;

    g_return_if_fail (dialog->props_channel == NULL
                      || XFCONF_IS_CHANNEL (dialog->props_channel));

    /* do nothing if the entre channel is locked */
    if (dialog->props_channel != NULL
        && gtk_widget_get_sensitive (dialog->button_new))
    {
        property = xfce_settings_editor_dialog_selected (dialog, &is_real_prop, &is_array);

        can_edit = !xfconf_channel_is_property_locked (dialog->props_channel, property);
        can_reset = can_edit && is_real_prop;

        if (is_array)
          can_edit = FALSE;

        g_free (property);
    }

    gtk_widget_set_sensitive (dialog->button_edit, can_edit);
    gtk_widget_set_sensitive (dialog->button_reset, can_reset);
}



static gboolean
xfce_settings_editor_dialog_query_tooltip (GtkWidget                *treeview,
                                           gint                      x,
                                           gint                      y,
                                           gboolean                  keyboard_mode,
                                           GtkTooltip               *tooltip,
                                           XfceSettingsEditorDialog *dialog)
{
    GtkTreeIter        iter;
    GtkTreePath       *path;
    GValue             value = { 0, };
    GtkTreeModel      *model;
    gboolean           show = FALSE;
    const gchar       *text;
    GtkTreeViewColumn *column;
    GList             *columns;
    gint               idx;

    gtk_tree_view_convert_widget_to_bin_window_coords (GTK_TREE_VIEW (treeview), x, y, &x, &y);

    if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview), x, y, &path,
                                       &column, NULL, NULL))
    {
        columns = gtk_tree_view_get_columns (GTK_TREE_VIEW (treeview));
        idx = g_list_index (columns, column);
        g_list_free (columns);

        model = GTK_TREE_MODEL (dialog->props_store);
        if (idx < 2 && gtk_tree_model_get_iter (model, &iter, path))
        {
            gtk_tree_model_get_value (model, &iter,
                idx == 0 ? PROP_COLUMN_FULL : PROP_COLUMN_TYPE, &value);

            text = g_value_get_string (&value);
            if (text != NULL)
            {
                gtk_tooltip_set_text (tooltip, text);
                show = TRUE;
            }
            g_value_unset (&value);
        }

        gtk_tree_path_free (path);
    }

    return show;
}



static void
xfce_settings_editor_dialog_row_activated (GtkTreeView              *treeview,
                                           GtkTreePath              *path,
                                           GtkTreeViewColumn        *column,
                                           XfceSettingsEditorDialog *dialog)
{
    GtkTreeModel *model = GTK_TREE_MODEL (dialog->props_store);
    GtkTreeIter   iter;

    if (gtk_tree_model_get_iter (model, &iter, path))
    {
        if (gtk_tree_model_iter_has_child (model, &iter))
        {
            if (gtk_tree_view_row_expanded (treeview, path))
                gtk_tree_view_collapse_row (treeview, path);
            else
                gtk_tree_view_expand_row (treeview, path, FALSE);
        }
        else if (gtk_widget_is_sensitive (dialog->button_edit))
        {
            xfce_settings_editor_dialog_property_edit (dialog);
        }
    }
}



static gboolean
xfce_settings_editor_dialog_key_press_event (GtkTreeView              *treeview,
                                             GdkEventKey              *event,
                                             XfceSettingsEditorDialog *dialog)
{
    if (event->keyval == GDK_Delete
        && gtk_widget_get_sensitive (dialog->button_reset))
    {
        xfce_settings_editor_dialog_property_reset (dialog);
        return TRUE;
    }
    else if (event->keyval == GDK_Insert
             && gtk_widget_get_sensitive (dialog->button_new))
    {
        xfce_settings_editor_dialog_property_new (dialog);
        return TRUE;
    }

    return FALSE;
}



static void
xfce_settings_editor_dialog_property_dialog (XfceSettingsEditorDialog *dialog,
                                             gboolean                  make_new)
{
    GtkWidget *prop_dialog;
    gchar     *property;

    property = xfce_settings_editor_dialog_selected (dialog, NULL, NULL);

    prop_dialog = xfce_settings_prop_dialog_new (GTK_WINDOW (dialog),
                                                 dialog->props_channel,
                                                 make_new ? NULL : property);
    if (make_new)
    {
        /* hint for the parent property based on selected property */
        xfce_settings_prop_dialog_set_parent_property (
            XFCE_SETTINGS_PROP_DIALOG (prop_dialog), property);
    }

    gtk_dialog_run (GTK_DIALOG (prop_dialog));
    gtk_widget_destroy (prop_dialog);

    g_free (property);
}



static void
xfce_settings_editor_dialog_property_new (XfceSettingsEditorDialog *dialog)
{
    xfce_settings_editor_dialog_property_dialog (dialog, TRUE);
}



static void
xfce_settings_editor_dialog_property_edit (XfceSettingsEditorDialog *dialog)
{
    xfce_settings_editor_dialog_property_dialog (dialog, FALSE);
}



static void
xfce_settings_editor_dialog_property_reset (XfceSettingsEditorDialog *dialog)
{
    gchar *property;

    property = xfce_settings_editor_dialog_selected (dialog, NULL, NULL);
    if (property != NULL
        && xfce_dialog_confirm (GTK_WINDOW (dialog),
                                GTK_STOCK_REVERT_TO_SAVED, _("_Reset"),
                                _("Resetting a property will permanently remove those custom settings."),
                                _("Are you sure you want to reset property \"%s\"?"), property))
    {
        xfconf_channel_reset_property (dialog->props_channel, property, FALSE);
    }

    g_free (property);
}



GtkWidget *
xfce_settings_editor_dialog_new (void)
{
    return g_object_new (XFCE_TYPE_SETTINGS_EDITOR_DIALOG, NULL);
}
