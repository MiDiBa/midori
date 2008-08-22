/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2008 Arnaud Renevier <arenevier@fdn.fr>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#if HAVE_CONFIG_H
    #include <config.h>
#endif

#include "midori-addons.h"

#include "sokoke.h"
#include "gjs.h"

#include <webkit/webkit.h>
#include <JavaScriptCore/JavaScript.h>
#include <glib/gi18n.h>
#include <string.h>

struct _MidoriAddons
{
    GtkVBox parent_instance;

    GtkWidget* web_widget;
    MidoriAddonKind kind;
    GtkWidget* toolbar;
    GtkWidget* treeview;
};

G_DEFINE_TYPE (MidoriAddons, midori_addons, GTK_TYPE_VBOX)

enum
{
    PROP_0,

    PROP_WEB_WIDGET,
    PROP_KIND
};

static void
midori_addons_set_property (GObject*      object,
                            guint         prop_id,
                            const GValue* value,
                            GParamSpec*   pspec);

static void
midori_addons_get_property (GObject*    object,
                            guint       prop_id,
                            GValue*     value,
                            GParamSpec* pspec);

GType
midori_addon_kind_get_type (void)
{
    static GType type = 0;
    if (!type)
    {
        static const GEnumValue values[] = {
         { MIDORI_ADDON_NONE, "MIDORI_ADDON_NONE", N_("None") },
         { MIDORI_ADDON_EXTENSIONS, "MIDORI_ADDON_EXTENSIONS", N_("Extensions") },
         { MIDORI_ADDON_USER_SCRIPTS, "MIDORI_USER_SCRIPTS", N_("Userscripts") },
         { MIDORI_ADDON_USER_STYLES, "MIDORI_USER_STYLES", N_("Userstyles") },
         { 0, NULL, NULL }
        };
        type = g_enum_register_static ("MidoriAddonKind", values);
    }
    return type;
}

static void
midori_addons_class_init (MidoriAddonsClass* class)
{
    GObjectClass* gobject_class;
    GParamFlags flags;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->set_property = midori_addons_set_property;
    gobject_class->get_property = midori_addons_get_property;

    flags = G_PARAM_READWRITE | G_PARAM_CONSTRUCT;

    g_object_class_install_property (gobject_class,
                                     PROP_WEB_WIDGET,
                                     g_param_spec_object (
                                     "web-widget",
                                     _("Web Widget"),
                                     _("The assigned web widget"),
                                     GTK_TYPE_WIDGET,
                                     G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_KIND,
                                     g_param_spec_enum (
                                     "kind",
                                     _("Kind"),
                                     _("The kind of addons"),
                                     MIDORI_TYPE_ADDON_KIND,
                                     MIDORI_ADDON_NONE,
                                     flags));
}

static void
midori_addons_set_property (GObject*      object,
                            guint         prop_id,
                            const GValue* value,
                            GParamSpec*   pspec)
{
    MidoriAddons* addons = MIDORI_ADDONS (object);

    switch (prop_id)
    {
    case PROP_WEB_WIDGET:
        midori_addons_set_web_widget (addons, g_value_get_object (value));
        break;
    case PROP_KIND:
        midori_addons_set_kind (addons, g_value_get_enum (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_addons_get_property (GObject*    object,
                            guint       prop_id,
                            GValue*     value,
                            GParamSpec* pspec)
{
    MidoriAddons* addons = MIDORI_ADDONS (object);

    switch (prop_id)
    {
    case PROP_WEB_WIDGET:
        g_value_set_object (value, midori_addons_get_web_widget (addons));
        break;
    case PROP_KIND:
        g_value_set_enum (value, midori_addons_get_kind (addons));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static const gchar*
_addons_get_folder (MidoriAddons* addons)
{
    switch (addons->kind)
    {
    case MIDORI_ADDON_EXTENSIONS:
        return "extensions";
    case MIDORI_ADDON_USER_SCRIPTS:
        return "scripts";
    case MIDORI_ADDON_USER_STYLES:
        return "styles";
    default:
        return NULL;
    }
}

static const gchar*
_addons_get_extension (MidoriAddons* addons)
{
    switch (addons->kind)
    {
    case MIDORI_ADDON_EXTENSIONS:
        return ".midori.js";
    case MIDORI_ADDON_USER_SCRIPTS:
        return ".user.js";
    case MIDORI_ADDON_USER_STYLES:
        return ".user.css";
    default:
        return NULL;
    }
}

static GSList*
_addons_get_files (MidoriAddons* addons)
{
    GSList* files;
    GDir* addon_dir;
    const gchar* addons_name;
    const gchar* addons_extension;
    const char* const *datadirs;
    const gchar* filename;
    gchar *dirname;
    gchar *fullname;

    files = NULL;
    addons_name = _addons_get_folder (addons);
    addons_extension = _addons_get_extension (addons);

    /* system data dirs */
    datadirs = g_get_system_data_dirs ();
    while (*datadirs)
    {
        dirname = g_build_filename (*datadirs, PACKAGE_NAME, addons_name, NULL);
        if ((addon_dir = g_dir_open (dirname, 0, NULL)))
        {
            while ((filename = g_dir_read_name (addon_dir)))
            {
                if (g_str_has_suffix (filename, addons_extension))
                {
                    fullname = g_build_filename (dirname, filename, NULL);
                    files = g_slist_prepend (files, fullname);
                }
            }
            g_dir_close (addon_dir);
        }
        g_free (dirname);
        datadirs++;
    }

    /* user data dir */
    dirname = g_build_filename (g_get_user_data_dir () , PACKAGE_NAME,
                                addons_name, NULL);
    if ((addon_dir = g_dir_open (dirname, 0, NULL)))
    {
        while ((filename = g_dir_read_name (addon_dir)))
        {
            if (g_str_has_suffix (filename, addons_extension))
            {
                fullname = g_build_filename (dirname, filename, NULL);
                files = g_slist_prepend (files, fullname);
            }
        }
        g_dir_close (addon_dir);
    }
    g_free (dirname);

    return files;
}

static void
midori_addons_button_add_clicked_cb (GtkToolItem*  toolitem,
                                     MidoriAddons* addons)
{
    GtkWidget* dialog = gtk_message_dialog_new (
        GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (addons))),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
        "Put scripts in the folder ~/.local/share/midori/%s",
        _addons_get_folder (addons));
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
}

static void
midori_addons_treeview_render_icon_cb (GtkTreeViewColumn* column,
                                       GtkCellRenderer*   renderer,
                                       GtkTreeModel*      model,
                                       GtkTreeIter*       iter,
                                       GtkWidget*         treeview)
{
    /* gchar* source_id;
    gtk_tree_model_get (model, iter, 2, &source_id, -1); */

    g_object_set (renderer, "stock-id", GTK_STOCK_FILE, NULL);

    /* g_free (source_id); */
}

static void
midori_addons_treeview_render_text_cb (GtkTreeViewColumn* column,
                                       GtkCellRenderer*   renderer,
                                       GtkTreeModel*      model,
                                       GtkTreeIter*       iter,
                                       GtkWidget*         treeview)
{
    gchar* displayname;

    gtk_tree_model_get (model, iter, 0, &displayname, -1);

    g_object_set (renderer, "text", displayname, NULL);

    g_free (displayname);
}

static void
midori_addons_treeview_row_activated_cb (GtkTreeView*       treeview,
                                         GtkTreePath*       path,
                                         GtkTreeViewColumn* column,
                                         MidoriAddons*     addons)
{
    /*GtkTreeModel* model = gtk_tree_view_get_model (treeview);
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter (model, &iter, path))
    {
        gchar* b;
        gtk_tree_model_get (model, &iter, 2, &b, -1);
        g_free (b);
    }*/
}

static void
midori_addons_init (MidoriAddons* addons)
{
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer_text;
    GtkCellRenderer* renderer_pixbuf;

    addons->web_widget = NULL;

    addons->treeview = gtk_tree_view_new ();
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (addons->treeview), FALSE);
    column = gtk_tree_view_column_new ();
    renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer_pixbuf, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_pixbuf,
        (GtkTreeCellDataFunc)midori_addons_treeview_render_icon_cb,
        addons->treeview, NULL);
    renderer_text = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer_text, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_text,
        (GtkTreeCellDataFunc)midori_addons_treeview_render_text_cb,
        addons->treeview, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (addons->treeview), column);
    g_signal_connect (addons->treeview, "row-activated",
                      G_CALLBACK (midori_addons_treeview_row_activated_cb),
                      addons);
    gtk_widget_show (addons->treeview);
    gtk_box_pack_start (GTK_BOX (addons), addons->treeview, TRUE, TRUE, 0);
}

static gboolean
_metadata_from_file (const gchar* filename,
                     GSList**     includes,
                     GSList**     excludes,
                     gchar**      name,
                     gchar**      description)
{
    GIOChannel* channel;
    gboolean found_meta;
    gchar* line;
    gchar* rest_of_line;

    if (!g_file_test (filename, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK))
        return FALSE;

    channel = g_io_channel_new_file (filename, "r", 0);
    if (!channel)
        return FALSE;

    found_meta = FALSE;

    while (g_io_channel_read_line (channel, &line, NULL, NULL, NULL)
           == G_IO_STATUS_NORMAL)
    {
        if (g_str_has_prefix (line, "// ==UserScript=="))
            found_meta = TRUE;
        else if (found_meta)
        {
            if (g_str_has_prefix (line, "// ==/UserScript=="))
                found_meta = FALSE;
            else if (g_str_has_prefix (line, "// @require ") ||
                g_str_has_prefix (line, "// @resource "))
            {
                    /* We don't support these, so abort here */
                    g_free (line);
                    g_io_channel_shutdown (channel, false, 0);
                    g_slist_free (*includes);
                    g_slist_free (*excludes);
                    return FALSE;
             }
             else if (includes && g_str_has_prefix (line, "// @include "))
             {
                 rest_of_line = g_strdup (line + strlen ("// @include "));
                 rest_of_line =  g_strstrip (rest_of_line);
                 *includes = g_slist_prepend (*includes, rest_of_line);
             }
             else if (excludes && g_str_has_prefix (line, "// @exclude "))
             {
                 rest_of_line = g_strdup (line + strlen ("// @exclude "));
                 rest_of_line =  g_strstrip (rest_of_line);
                 *excludes = g_slist_prepend (*excludes, rest_of_line);
             }
             else if (name && g_str_has_prefix (line, "// @name "))
             {
                 rest_of_line = g_strdup (line + strlen ("// @name "));
                 rest_of_line =  g_strstrip (rest_of_line);
                 *name = rest_of_line;
             }
             else if (description && g_str_has_prefix (line, "// @description "))
             {
                 rest_of_line = g_strdup (line + strlen ("// @description "));
                 rest_of_line =  g_strstrip (rest_of_line);
                 *description = rest_of_line;
             }
        }
        g_free (line);
    }
    g_io_channel_shutdown (channel, false, 0);
    g_io_channel_unref (channel);

    return TRUE;
}

#define HAVE_GREGEX GLIB_CHECK_VERSION (2, 14, 0)

#if HAVE_GREGEX
static gchar*
_convert_to_simple_regexp (const gchar* pattern)
{
    guint len;
    gchar* dest;
    guint pos;
    guint i;
    gchar c;

    len = strlen (pattern);
    dest = g_malloc0 (len * 2 + 1);
    dest[0] = '^';
    pos = 1;

    for (i = 0; i < len; i++)
    {
        c = pattern[i];
        switch (c)
        {
            case '*':
                dest[pos] = '.';
                dest[pos + 1] = c;
                pos++;
                pos++;
                break;
            case '.' :
            case '?' :
            case '^' :
            case '$' :
            case '+' :
            case '{' :
            case '[' :
            case '|' :
            case '(' :
            case ')' :
            case ']' :
            case '\\' :
               dest[pos] = '\\';
               dest[pos + 1] = c;
               pos++;
               pos++;
               break;
            case ' ' :
                break;
            default:
               dest[pos] = pattern[i];
               pos ++;
        }
    }
    return dest;
}
#else
static bool
_match_with_wildcard (const gchar* str,
                      const gchar* pattern)
{
    gchar** parts;
    gchar** parts_ref;
    const gchar* subpart;
    gchar* newsubpart;

    parts = g_strsplit (pattern, "*", 0);
    parts_ref = parts;
    subpart = str;
    do
    {
        newsubpart = g_strstr_len (subpart, strlen (subpart), *parts);
        if (!newsubpart)
        {
            g_strfreev (parts_ref);
            return FALSE;
        }
        subpart = newsubpart + strlen (*parts);
        parts++;
    }
    while (*parts);
    g_strfreev (parts_ref);
    return TRUE;
}
#endif

static gboolean
_may_load_script (const gchar* uri,
                  GSList**     includes,
                  GSList**     excludes)
{
    gboolean match;
    GSList* list;
    #if HAVE_GREGEX
    gchar* re;
    #else
    guint uri_len;
    guint pattern_len;
    #endif

    if (*includes)
        match = FALSE;
    else
        match = TRUE;

    list = *includes;
    #if !HAVE_GREGEX
    uri_len = strlen (uri);
    #endif
    while (list)
    {
        #if HAVE_GREGEX
        re = _convert_to_simple_regexp (list->data);
        if (g_regex_match_simple (re, uri, 0, 0))
        {
            match = TRUE;
            break;
        }
        g_free (re);
        #else
        pattern_len = strlen (list->data);
        if (!g_ascii_strncasecmp (uri, list->data, MAX (uri_len, pattern_len)))
        {
            match = TRUE;
            break;
        }
        else if (_match_with_wildcard (uri, list->data))
        {
            match = TRUE;
            break;
        }
        #endif
        list = list->next;
    }
    if (!match)
    {
        return FALSE;
    }
    list = *excludes;
    while (list)
    {
        #if HAVE_GREGEX
        re = _convert_to_simple_regexp (list->data);
        if (g_regex_match_simple (re, uri, 0, 0))
        {
            match = FALSE;
            break;
        }
        g_free (re);
        #else
        pattern_len = strlen (list->data);
        if (!g_ascii_strncasecmp (uri, list->data, MAX (uri_len, pattern_len)))
        {
            match = FALSE;
            break;
        }
        else if (_match_with_wildcard (uri, list->data))
        {
            match = FALSE;
            break;
        }
         #endif
        list = list->next;
    }
    return match;
}

static gboolean
_js_script_from_file (JSContextRef js_context,
                      const gchar* filename,
                      gchar**      exception)
{
    gboolean result = FALSE;
    gchar* script;
    GError* error = NULL;
    gchar* wrapped_script;

    if (g_file_get_contents (filename, &script, NULL, &error))
    {
        /* Wrap the script to prevent global variables */
        wrapped_script = g_strdup_printf (
            "window.addEventListener ('DOMContentLoaded',"
            "function () { %s }, true);", script);
        if (gjs_script_eval (js_context, wrapped_script, exception))
            result = TRUE;
        g_free (wrapped_script);
        g_free (script);
    }
    else
    {
        *exception = g_strdup (error->message);
        g_error_free (error);
    }
    return result;
}

static gboolean
_js_style_from_file (JSContextRef js_context,
                     const gchar* filename,
                     gchar**      exception)
{
    gboolean result = FALSE;
    gchar* style;
    GError* error = NULL;
    guint i, n;
    gchar* style_script;

    if (g_file_get_contents (filename, &style, NULL, &error))
    {
        n = strlen (style);
        for (i = 0; i < n; i++)
        {
            /* Replace line breaks with spaces */
            if (style[i] == '\n' || style[i] == '\r')
                style[i] = ' ';
            /* Change all single quotes to double quotes */
            if (style[i] == '\'')
                style[i] = '\"';
        }
        style_script = g_strdup_printf (
            "window.addEventListener ('DOMContentLoaded',"
            "function () {"
            "var mystyle = document.createElement(\"style\");"
            "mystyle.setAttribute(\"type\", \"text/css\");"
            "mystyle.appendChild(document.createTextNode('%s'));"
            "var head = document.getElementsByTagName(\"head\")[0];"
            "if (head) head.appendChild(mystyle);"
            "else document.documentElement.insertBefore(mystyle, document.documentElement.firstChild);"
            "}, true);",
            style);
        if (gjs_script_eval (js_context, style_script, exception))
            result = TRUE;
        g_free (style_script);
        g_free (style);
    }
    else
    {
        *exception = g_strdup (error->message);
        g_error_free (error);
    }
    return result;
}

static void
midori_web_widget_window_object_cleared_cb (GtkWidget*         web_widget,
                                            WebKitWebFrame*    web_frame,
                                            JSGlobalContextRef js_context,
                                            JSObjectRef        js_window,
                                            MidoriAddons*      addons)
{
    gchar* fullname;
    GSList* includes;
    GSList* excludes;
    const gchar* uri;
    gchar* exception;
    gchar* message;
    GSList* addon_files;
    GSList* list;

    uri = webkit_web_frame_get_uri (web_frame);
    if (!uri)
        return;

    addon_files = _addons_get_files (addons);
    list = addon_files;
    while (addon_files)
    {
        fullname = addon_files->data;
        includes = NULL;
        excludes = NULL;
        if (addons->kind == MIDORI_ADDON_USER_SCRIPTS &&
            !_metadata_from_file (fullname, &includes, &excludes, NULL, NULL))
        {
            addon_files = g_slist_next (addon_files);
            continue;
        }
        if (includes || excludes)
            if (!_may_load_script (uri, &includes, &excludes))
            {
                addon_files = g_slist_next (addon_files);
                continue;
            }
        g_slist_free (includes);
        g_slist_free (excludes);
        exception = NULL;
        if (addons->kind == MIDORI_ADDON_USER_SCRIPTS &&
            !_js_script_from_file (js_context, fullname, &exception))
        {
            message = g_strdup_printf ("console.error ('%s');", exception);
            gjs_script_eval (js_context, message, NULL);
            g_free (message);
            g_free (exception);
        }
        else if (addons->kind == MIDORI_ADDON_USER_STYLES &&
            !_js_style_from_file (js_context, fullname, &exception))
        {
            message = g_strdup_printf ("console.error ('%s');", exception);
            gjs_script_eval (js_context, message, NULL);
            g_free (message);
            g_free (exception);
        }
        addon_files = addon_files->next;
    }
    g_slist_free (list);
}

/**
 * midori_addons_new:
 * @web_widget: a web widget
 * @kind: the kind of addon
 *
 * Creates a new addons widget.
 *
 * @web_widget can be one of the following:
 *     %MidoriBrowser, %MidoriWebView, %WebKitWebView
 *
 * Return value: a new #MidoriAddons
 **/
GtkWidget*
midori_addons_new (GtkWidget*      web_widget,
                   MidoriAddonKind kind)
{
    MidoriAddons* addons;

    g_return_val_if_fail (GTK_IS_WIDGET (web_widget), NULL);

    addons = g_object_new (MIDORI_TYPE_ADDONS,
                           /* "web-widget", web_widget,
                           "kind", kind, */ NULL);
    addons->web_widget = web_widget;
    midori_addons_set_kind (addons, kind);

    return GTK_WIDGET (addons);
}

/**
 * midori_addons_set_web_widget:
 * @addons: a #MidoriAddons
 * @web_widget: a web widget
 *
 * Sets the assigned web widget. Basically any widget
 * with a window-object-cleared qualifies as such.
 *
 * Note: This may only be set once.
 **/
void
midori_addons_set_web_widget (MidoriAddons* addons,
                              GtkWidget*    web_widget)
{
    g_return_if_fail (MIDORI_IS_ADDONS (addons));
    g_return_if_fail (GTK_IS_WIDGET (addons));
    g_return_if_fail (g_signal_lookup ("window-object-cleared", G_TYPE_FROM_INSTANCE (web_widget)));

    /* FIXME: Implement this */
}

/**
 * midori_addons_get_web_widget:
 * @addons: a #MidoriAddons
 *
 * Determines the assigned web widget.
 *
 * Return value: a web widget
 **/
GtkWidget*
midori_addons_get_web_widget (MidoriAddons* addons)
{
    return addons->web_widget;
}

/**
 * midori_addons_set_kind:
 * @addons: a #MidoriAddons
 * @kind: a #MidoriAddonKind
 *
 * Sets the kind of addons.
 *
 * Note: This may only be set once.
 **/
void
midori_addons_set_kind (MidoriAddons*   addons,
                        MidoriAddonKind kind)
{
    GtkListStore* liststore;
    gchar* fullname;
    gchar* displayname;
    gchar* name;
    GtkTreeIter iter;
    GSList* addon_files;
    GSList* list;

    g_return_if_fail (MIDORI_IS_ADDONS (addons));
    g_return_if_fail (addons->kind == MIDORI_ADDON_NONE);

    if (kind == MIDORI_ADDON_NONE)
        return;

    g_return_if_fail (addons->web_widget);

    addons->kind = kind;

    if (kind == MIDORI_ADDON_USER_SCRIPTS
        || kind == MIDORI_ADDON_USER_STYLES)
        g_signal_connect (addons->web_widget, "window-object-cleared",
            G_CALLBACK (midori_web_widget_window_object_cleared_cb), addons);

    liststore = gtk_list_store_new (3, G_TYPE_STRING,
                                    G_TYPE_INT,
                                    G_TYPE_STRING);

    addon_files = _addons_get_files (addons);
    list = addon_files;
    while (addon_files)
    {
        fullname = addon_files->data;
        displayname =  g_filename_display_basename (fullname);

        if (kind == MIDORI_ADDON_USER_SCRIPTS)
        {
            name = NULL;
            if (!_metadata_from_file (fullname, NULL, NULL, &name, NULL))
                continue;
            if (name)
            {
                g_free (displayname);
                displayname = name;
            }
        }

        gtk_list_store_append (liststore, &iter);
        gtk_list_store_set (liststore, &iter, 0, displayname, 1, 0, 2, "", -1);
        addon_files = addon_files->next;
    }

    g_slist_free (list);

    gtk_tree_view_set_model (GTK_TREE_VIEW (addons->treeview),
                             GTK_TREE_MODEL (liststore));
    g_object_notify (G_OBJECT (addons), "kind");
}

/**
 * midori_addons_get_kind:
 * @addons: a #MidoriAddons
 *
 * Determines the kind of addons.
 *
 * Return value: a #MidoriAddonKind
 **/
MidoriAddonKind
midori_addons_get_kind (MidoriAddons* addons)
{
    return addons->kind;
}

/**
 * midori_addons_get_toolbar:
 * @addons: a #MidoriAddons
 *
 * Retrieves the toolbar of the addons. A new widget
 * is created on the first call of this function.
 *
 * Return value: a toolbar widget
 **/
GtkWidget*
midori_addons_get_toolbar (MidoriAddons* addons)
{
    GtkWidget* toolbar;
    GtkToolItem* toolitem;

    g_return_val_if_fail (MIDORI_IS_ADDONS (addons), NULL);

    if (!addons->toolbar)
    {
        toolbar = gtk_toolbar_new ();
        gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH_HORIZ);
        gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_BUTTON);
        toolitem = gtk_tool_item_new ();
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        toolitem = gtk_separator_tool_item_new ();
        gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (toolitem),
                                          FALSE);
        gtk_tool_item_set_expand (toolitem, TRUE);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_ADD);
        gtk_tool_item_set_is_important (toolitem, TRUE);
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (midori_addons_button_add_clicked_cb), addons);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        addons->toolbar = toolbar;

        g_signal_connect (addons->toolbar, "destroy",
                          G_CALLBACK (gtk_widget_destroyed),
                          &addons->toolbar);
    }

    return addons->toolbar;
}
