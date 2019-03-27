/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include <stdlib.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <fontconfig/fontconfig.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>

#include "cc-common-language.h"

static char *get_lang_for_user_object_path (const char *path);

static gboolean
iter_for_language (GtkTreeModel *model,
                   const gchar  *lang,
                   GtkTreeIter  *iter,
                   gboolean      region)
{
        char *l;
        char *name;
        char *language;

        g_assert (gtk_tree_model_get_iter_first (model, iter));
        do {
                gtk_tree_model_get (model, iter, LOCALE_COL, &l, -1);
                if (g_strcmp0 (l, lang) == 0) {
                        g_free (l);
                        return TRUE;
                }
                g_free (l);
        } while (gtk_tree_model_iter_next (model, iter));

        name = gnome_normalize_locale (lang);
        if (name != NULL) {
                if (region) {
                        language = gnome_get_country_from_locale (name, NULL);
                }
                else {
                        language = gnome_get_language_from_locale (name, NULL);
                }

                gtk_list_store_insert_with_values (GTK_LIST_STORE (model),
                                                   iter,
                                                   -1,
                                                   LOCALE_COL, name,
                                                   DISPLAY_LOCALE_COL, language,
                                                   -1);
                g_free (name);
                g_free (language);
                return TRUE;
        }

        return FALSE;
}

gboolean
cc_common_language_get_iter_for_language (GtkTreeModel *model,
                                          const gchar  *lang,
                                          GtkTreeIter  *iter)
{
  return iter_for_language (model, lang, iter, FALSE);
}

gboolean
cc_common_language_has_font (const gchar *locale)
{
        const FcCharSet *charset;
        FcPattern       *pattern;
        FcObjectSet     *object_set;
        FcFontSet       *font_set;
        gchar           *language_code;
        gboolean         is_displayable;

        is_displayable = FALSE;
        pattern = NULL;
        object_set = NULL;
        font_set = NULL;

        if (!gnome_parse_locale (locale, &language_code, NULL, NULL, NULL))
                return FALSE;

        charset = FcLangGetCharSet ((FcChar8 *) language_code);
        if (!charset) {
                /* fontconfig does not know about this language */
                is_displayable = TRUE;
        }
        else {
                /* see if any fonts support rendering it */
                pattern = FcPatternBuild (NULL, FC_LANG, FcTypeString, language_code, NULL);

                if (pattern == NULL)
                        goto done;

                object_set = FcObjectSetCreate ();

                if (object_set == NULL)
                        goto done;

                font_set = FcFontList (NULL, pattern, object_set);

                if (font_set == NULL)
                        goto done;

                is_displayable = (font_set->nfont > 0);
        }

 done:
        if (font_set != NULL)
                FcFontSetDestroy (font_set);

        if (object_set != NULL)
                FcObjectSetDestroy (object_set);

        if (pattern != NULL)
                FcPatternDestroy (pattern);

        g_free (language_code);

        return is_displayable;
}

gchar *
cc_common_language_get_current_language (void)
{
        gchar *language;
        char *path;
        const gchar *locale;

	path = g_strdup_printf ("/org/freedesktop/Accounts/User%d", getuid ());
        language = get_lang_for_user_object_path (path);
        g_free (path);
        if (language != NULL && *language != '\0')
                return language;

        locale = (const gchar *) setlocale (LC_MESSAGES, NULL);
        if (locale)
                language = gnome_normalize_locale (locale);
        else
                language = NULL;

        return language;
}

gchar *
cc_common_language_get_property (const gchar *prop_name)
{
  GDBusConnection  *bus;
  gchar            *user_path;
  GError           *error = NULL;
  GVariant         *properties;
  GVariantIter     *iter;
  gchar            *key;
  GVariant         *value;
  gchar            *ret = NULL;

  if (g_strcmp0 (prop_name, "Language") != 0 && g_strcmp0 (prop_name, "FormatsLocale") != 0) {
    g_warning ("Invalid argument: '%s'", prop_name);
    return ret;
  }

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
  user_path = g_strdup_printf ("/org/freedesktop/Accounts/User%i", getuid ());

  properties = g_dbus_connection_call_sync (bus,
                                            "org.freedesktop.Accounts",
                                            user_path,
                                            "org.freedesktop.DBus.Properties",
                                            "GetAll",
                                            g_variant_new ("(s)", "org.freedesktop.Accounts.User"),
                                            G_VARIANT_TYPE ("(a{sv})"),
                                            G_DBUS_CALL_FLAGS_NONE,
                                            -1,
                                            NULL,
                                            &error);
  if (!properties) {
    g_warning ("Error calling GetAll() when retrieving properties for %s: %s", user_path, error->message);
    g_error_free (error);
                /* g_hash_table_lookup() is not NULL-safe, so don't return NULL */
                if (g_strcmp0 (prop_name, "Language") == 0)
                        ret = g_strdup ("en");
                else
                        ret = g_strdup ("en_US.UTF-8");
    goto out;
  }

  g_variant_get (properties, "(a{sv})", &iter);
  while (g_variant_iter_loop (iter, "{&sv}", &key, &value)) {
    if (g_strcmp0 (key, prop_name) == 0) {
      g_variant_get (value, "s", &ret);
      break;
    }
  }

  g_variant_unref (properties);
  g_variant_iter_free (iter);

out:
  g_object_unref (bus);
  g_free (user_path);

  return ret;
}

static char *
get_lang_for_user_object_path (const char *path)
{
	GError *error = NULL;
	GDBusProxy *user;
	GVariant *props;
	char *lang;

	user = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
					      G_DBUS_PROXY_FLAGS_NONE,
					      NULL,
					      "org.freedesktop.Accounts",
					      path,
					      "org.freedesktop.Accounts.User",
					      NULL,
					      &error);
	if (user == NULL) {
		g_warning ("Failed to get proxy for user '%s': %s",
			   path, error->message);
		g_error_free (error);
		return NULL;
	}

	props = g_dbus_proxy_get_cached_property (user, "Language");
	if (props == NULL) {
		g_object_unref (user);
		return NULL;
	}
	lang = g_variant_dup_string (props, NULL);

	g_variant_unref (props);
	g_object_unref (user);
	return lang;
}

/*
 * Note that @lang needs to be formatted like the locale strings
 * returned by gnome_get_all_locales().
 */
static void
insert_language (GHashTable *ht,
                 const char *lang)
{
        char *label_own_lang;
        char *label_current_lang;
        char *label_untranslated;
        char *key;

        cc_common_language_get_locale (lang, &key);

        label_own_lang = gnome_get_language_from_locale (key, key);
        label_current_lang = gnome_get_language_from_locale (key, NULL);
        label_untranslated = gnome_get_language_from_locale (key, "C");

        /* We don't have a translation for the label in
         * its own language? */
        if (g_strcmp0 (label_own_lang, label_untranslated) == 0) {
                if (g_strcmp0 (label_current_lang, label_untranslated) == 0)
                        g_hash_table_insert (ht, key, g_strdup (label_untranslated));
                else
                        g_hash_table_insert (ht, key, g_strdup (label_current_lang));
        } else {
                g_hash_table_insert (ht, key, g_strdup (label_own_lang));
        }

        g_free (label_own_lang);
        g_free (label_current_lang);
        g_free (label_untranslated);
}

gchar **
cc_common_language_get_installed_languages (void)
{
        gchar  *output;
        gchar  **langs = NULL;
        GError *error = NULL;

        if (!g_spawn_command_line_sync ("/usr/share/language-tools/language-options",
                                        &output, NULL, NULL, &error)) {
                g_warning ("Couldn't get installed languages: %s", error->message);
                g_error_free (error);
                return NULL;
        }
        langs = g_strsplit (output, "\n", 0);

        g_free(output);
        return langs;
}

GHashTable *
cc_common_language_get_initial_languages (void)
{
        GHashTable *ht;
        gchar **langs;
        gint i;

        ht = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

        langs = cc_common_language_get_installed_languages ();

        if (langs) {
                for (i = 0; langs[i]; i++) {
                        insert_language (ht, langs[i]);
                }

                g_strfreev (langs);
        }
/*
        insert_language (ht, "en_US.UTF-8");
        insert_language (ht, "en_GB.UTF-8");
        insert_language (ht, "de_DE.UTF-8");
        insert_language (ht, "fr_FR.UTF-8");
        insert_language (ht, "es_ES.UTF-8");
        insert_language (ht, "zh_CN.UTF-8");
        insert_language (ht, "ja_JP.UTF-8");
        insert_language (ht, "ru_RU.UTF-8");
        insert_language (ht, "ar_EG.UTF-8");
*/
        return ht;
}

void
cc_common_language_get_locale (const gchar *language, gchar **locale){
        gchar *command;
        GError *error = NULL;
        /* Get locale that corresponds to the language */
        command = g_strconcat ("/usr/share/language-tools/language2locale ", language, NULL);
        if (!g_spawn_command_line_sync (command, locale, NULL, NULL, &error)) {
               g_warning ("Couldn't get locale: %s", error->message);
                g_error_free (error);
                g_free (command);

                return;
        }

        g_free (command);
        g_strchomp (*locale);
        if (strlen (*locale) == 0) {
                g_warning ("Couldn't get locale for language: %s", language);
                return;
        }

}
static void
foreach_user_lang_cb (gpointer key,
                      gpointer value,
                      gpointer user_data)
{
        GtkListStore *store = (GtkListStore *) user_data;
        const char *locale = (const char *) key;
        const char *display_locale = (const char *) value;
        GtkTreeIter iter;

        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            LOCALE_COL, locale,
                            DISPLAY_LOCALE_COL, display_locale,
                            -1);
}

void
cc_common_language_add_user_languages (GtkTreeModel *model)
{
        char *name;
        GtkTreeIter iter;
        GtkListStore *store = GTK_LIST_STORE (model);
        GHashTable *user_langs;
        const char *display;
        const char *lang;

        gtk_list_store_clear (store);

        user_langs = cc_common_language_get_initial_languages ();

        /* Add the current locale first */
        lang = cc_common_language_get_property ("Language");
        cc_common_language_get_locale (lang, &name);
        display = g_hash_table_lookup (user_langs, name);
        if (!display) {
                insert_language (user_langs, lang);
                display = g_hash_table_lookup (user_langs, name);
        }

        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter, LOCALE_COL, name, DISPLAY_LOCALE_COL, display, -1);
        g_hash_table_remove (user_langs, name);
        g_free (name);

        /* The rest of the languages */
        g_hash_table_foreach (user_langs, (GHFunc) foreach_user_lang_cb, store);

        /* And now the "Other…" selection */
        //gtk_list_store_append (store, &iter);
        //gtk_list_store_set (store, &iter, LOCALE_COL, NULL, DISPLAY_LOCALE_COL, _("Other…"), -1);

        g_hash_table_destroy (user_langs);
}

typedef struct {
        gchar *lang;
        guint xid;
        GDBusProxy *pk_proxy, *pk_transaction_proxy;
        GPtrArray *array;
} PkTransactionData;

static void
on_pk_what_provides_ready (GObject      *source,
                           GAsyncResult *res,
                           PkTransactionData *pk_data)
{
  GError     *error;
  GVariant   *result;

  error = NULL;
  result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), res, &error);
  if (result == NULL)
    {
      g_debug ("Error getting PackageKit updates list: %s", error->message);
      g_error_free (error);
      return;
    }
}

static void
cc_common_language_install (guint xid, gchar **packages) {

  GDBusProxy *proxy = NULL;
  GError *error = NULL;
  GVariant *retval;

  /* get a session bus proxy */
  proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                 G_DBUS_PROXY_FLAGS_NONE, NULL,
                 "org.freedesktop.PackageKit",
                 "/org/freedesktop/PackageKit",
                 "org.freedesktop.PackageKit.Modify",
                 NULL, &error);
  if (proxy == NULL) {
    g_debug ("failed: %s", error->message);
    g_error_free (error);
    goto out;
  }

  /* issue the sync request */
  retval = g_dbus_proxy_call_sync (proxy,
                                   "InstallPackageNames",
                                   g_variant_new ("(u^a&ss)",
                                      xid,
                                      packages,
                                      "hide-finished"),
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1, /* timeout */
                                   NULL, /* cancellable */
                                   &error);
  if (retval == NULL) {
    g_debug ("failed: %s", error->message);
    g_error_free (error);
    goto out;
  }
  g_object_unref (retval);

out:
  if (proxy != NULL)
    g_object_unref (proxy);
  return;
}

static void
on_pk_transaction_signal (GDBusProxy *proxy,
                          char *sender_name,
                          char *signal_name,
                          GVariant *parameters,
                          PkTransactionData *pk_data)
{
  if (g_strcmp0 (signal_name, "Package") == 0) {
    gchar *package, *unused;
    guint32 status;
    gchar **split;

    g_variant_get (parameters, "(u&s&s)", &status, &package, &unused);

    if (status == 2) { /*PK_INFO_ENUM_AVAILABLE*/
      split = g_strsplit(package, ";", -1);
      g_ptr_array_add (pk_data->array, g_strdup (split[0]));

      g_strfreev(split);
    }
  } else if (!g_strcmp0 (signal_name, "Finished")) {
    char **lang;

    g_ptr_array_add (pk_data->array, NULL);

    lang = (char **) g_ptr_array_free (pk_data->array, FALSE);
    /* Now install all packages returned by the previous call */
    if (lang[0] != NULL)
      cc_common_language_install(pk_data->xid, lang);

    g_strfreev(lang);

  } else if (g_strcmp0 (signal_name, "Destroy") == 0) {
      g_free (pk_data->lang);
      g_clear_object (&pk_data->pk_transaction_proxy);
      g_clear_object (&pk_data->pk_proxy);
  }
}

static void
on_pk_get_tid_ready (GObject      *source,
                     GAsyncResult *res,
                     PkTransactionData *pk_data)

{
  GError     *error;
  GVariant   *result;
  char       *tid;

  const gchar * provides_args[] = { g_strdup_printf ("locale(%s)",pk_data->lang), NULL };
  error = NULL;
  result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), res, &error);
  if (result == NULL)
    {
      if (g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN) == FALSE)
        g_debug ("Error getting PackageKit transaction ID: %s", error->message);
        g_error_free (error);
      return;
    }

  g_variant_get (result, "(o)", &tid);

  pk_data->pk_transaction_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                                 G_DBUS_PROXY_FLAGS_NONE,
                                                                 NULL,
                                                                 "org.freedesktop.PackageKit",
                                                                 tid,
                                                                 "org.freedesktop.PackageKit.Transaction",
                                                                 NULL,
                                                                 NULL);
  g_free (tid);
  g_variant_unref (result);

  if (pk_data->pk_transaction_proxy == NULL)
    {
      g_debug ("Unable to get PackageKit transaction proxy object");
      return;
    }

  g_signal_connect (pk_data->pk_transaction_proxy,
                    "g-signal",
                    G_CALLBACK (on_pk_transaction_signal),
                    pk_data);

  g_dbus_proxy_call (pk_data->pk_transaction_proxy,
                     "WhatProvides",
                     /* TODO need to get enums from libpackagekit-glib2 */
                     g_variant_new ("(tu^a&s)",
                                    (guint64)1, /*PK_FILTER_ENUM_NONE*/
                                    (guint32)11, /*PK_PROVIDES_ENUM_LANGUAGE_SUPPORT*/
                                    provides_args),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     (GAsyncReadyCallback) on_pk_what_provides_ready,
                     pk_data);
}

gboolean
cc_common_language_maybe_install (guint xid, const gchar *lang, gboolean force)
{
        gchar *language_code, *territory_code, *territory_lang = NULL;
        gchar **langs;
        gboolean result = FALSE;
        int i;
        PkTransactionData *pk_data = NULL;

        gnome_parse_locale (lang, &language_code, &territory_code, NULL, NULL);

        /* If the language is already available, do nothing */
        if (g_strcmp0 (language_code, "zh") == 0 )
                territory_lang = g_strdup_printf ("%s_%s", language_code, territory_code);
        else
                territory_lang = g_strdup (language_code);

        langs = cc_common_language_get_installed_languages();
        for (i = 0; langs[i]; i++) {
                if (g_strrstr (langs[i], territory_lang) ) {
                        if (!force) {
                               g_warning ("Language is already installed");
                               result = TRUE;
                               goto out;
                        }
                }
         }

        g_warning ("Language %s not installed, trying to install it", lang);

        pk_data = g_new0 (PkTransactionData, 1);
        pk_data->lang = g_strdup (lang);
        pk_data->array = g_ptr_array_new ();
        pk_data->xid = xid;

        /* Now try to retrieve the list of packages needed to install this language */
        pk_data->pk_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                           G_DBUS_PROXY_FLAGS_NONE,
                                                           NULL,
                                                           "org.freedesktop.PackageKit",
                                                           "/org/freedesktop/PackageKit",
                                                           "org.freedesktop.PackageKit",
                                                           NULL,
                                                           NULL);
        if (pk_data->pk_proxy == NULL) {
                /* if there's a PK error, ignore and assume the lang is available */
                g_debug ("PackageKit not available, not installing language");
                goto out;
        }



        /* Retrieve PK transaction */
        g_dbus_proxy_call  (pk_data->pk_proxy,
                            "CreateTransaction",
                            NULL,
                            G_DBUS_CALL_FLAGS_NONE,
                            -1,
                            NULL,
                            (GAsyncReadyCallback) on_pk_get_tid_ready,
                            pk_data);

out:
        g_free (language_code);
        g_free (territory_code);
        g_free (territory_lang);
        g_strfreev (langs);

        return result;
}
