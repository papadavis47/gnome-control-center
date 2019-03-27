/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2017 Canonical Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <glib.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

#include "shell/list-box-helper.h"
#include "cc-ubuntu-panel.h"
#include "cc-ubuntu-resources.h"

#include "panels/display/cc-display-config-manager-dbus.h"
#include "panels/display/cc-display-config.h"

#define MIN_ICONSIZE 16.0
#define MAX_ICONSIZE 64.0
#define DEFAULT_ICONSIZE 48.0
#define ICONSIZE_KEY "dash-max-icon-size"

#define UBUNTU_DOCK_SCHEMA "org.gnome.shell.extensions.dash-to-dock"
#define UBUNTU_DOCK_ALL_MONITORS_KEY "multi-monitor"
#define UBUNTU_DOCK_ON_MONITOR_KEY "preferred-monitor"

struct _CcUbuntuPanel {
  CcPanel parent_instance;

  GtkBuilder *builder;

  GSettings *ubuntu_dock_settings;
  GtkWidget *ubuntu_dock_placement_combo;
  GtkWidget *show_on_row;
  gint ubuntu_dock_placement_primary_index;

  CcDisplayConfigManager *display_config_manager;
};

struct _CcUbuntuPanelClass {
  CcPanelClass parent;
};

CC_PANEL_REGISTER (CcUbuntuPanel, cc_ubuntu_panel);

static void
cc_ubuntu_panel_dispose (GObject *object)
{
  CcUbuntuPanel *panel = CC_UBUNTU_PANEL (object);

  g_clear_object (&panel->display_config_manager);

  G_OBJECT_CLASS (cc_ubuntu_panel_parent_class)->dispose (object);
}

static void
cc_ubuntu_panel_finalize (GObject *object)
{
  CcUbuntuPanel *panel = CC_UBUNTU_PANEL (object);

  G_OBJECT_CLASS (cc_ubuntu_panel_parent_class)->finalize (object);
}

static void
on_screen_changed (CcUbuntuPanel *panel)
{
  CcDisplayConfig *current;
  GList *outputs, *l;
  GList *sorted = NULL;
  gint n = 0;

  if (!panel->display_config_manager)
    return;

  current = cc_display_config_manager_get_current (panel->display_config_manager);
  if (!current)
    return;

  GtkWidget *ubuntu_dock_placement_combo = panel->ubuntu_dock_placement_combo;
  GtkCellRenderer *ubuntu_dock_placement_cell;
  GtkListStore *ubuntu_dock_placement_liststore = gtk_list_store_new (1, G_TYPE_STRING);
  GtkTreeIter ubuntu_dock_placement_iter;
  gboolean ubuntu_dock_on_all_monitors = g_settings_get_boolean (panel->ubuntu_dock_settings, UBUNTU_DOCK_ALL_MONITORS_KEY);
  gint ubuntu_dock_current_index = g_settings_get_int (panel->ubuntu_dock_settings, UBUNTU_DOCK_ON_MONITOR_KEY);
  gtk_list_store_clear (ubuntu_dock_placement_liststore);

  outputs = cc_display_config_get_monitors(current);
  for (l = outputs; l != NULL; l = l->next) {
    CcDisplayMonitor *output = l->data;
    if (cc_display_monitor_is_builtin (output))
      sorted = g_list_prepend (sorted, output);
    else
      sorted = g_list_append (sorted, output);
  }

  n = 0;
  for (l = sorted; l != NULL; l = l->next) {
    CcDisplayMonitor *output = l->data;

    const gchar *monitor_name = cc_display_monitor_get_display_name (output);
    gtk_list_store_append (ubuntu_dock_placement_liststore, &ubuntu_dock_placement_iter);
    gtk_list_store_set (ubuntu_dock_placement_liststore, &ubuntu_dock_placement_iter, 0, monitor_name, -1);

    if (cc_display_monitor_is_primary (output)) {
      panel->ubuntu_dock_placement_primary_index = n;
    }
    n++;
  }

  if (n < 2) {
    gtk_widget_set_visible (panel->show_on_row, FALSE);
  } else {
    gtk_widget_set_visible (panel->show_on_row, TRUE);
  }

  gtk_list_store_prepend (ubuntu_dock_placement_liststore, &ubuntu_dock_placement_iter);
  gtk_list_store_set (ubuntu_dock_placement_liststore, &ubuntu_dock_placement_iter, 0, _("All displays"), -1);
  gtk_combo_box_set_model (GTK_COMBO_BOX (ubuntu_dock_placement_combo), GTK_TREE_MODEL (ubuntu_dock_placement_liststore));
  g_object_unref (ubuntu_dock_placement_liststore);
  gtk_cell_layout_clear (GTK_CELL_LAYOUT (ubuntu_dock_placement_combo));
  ubuntu_dock_placement_cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (ubuntu_dock_placement_combo), ubuntu_dock_placement_cell, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (ubuntu_dock_placement_combo), ubuntu_dock_placement_cell,
                                  "text", 0,
                                  NULL);

  gint selection = 0;
  if (ubuntu_dock_on_all_monitors != TRUE) {
    if ((ubuntu_dock_current_index != -1) && (ubuntu_dock_current_index < g_list_length (outputs)))
        selection = ubuntu_dock_current_index + 1;
    else
      selection = panel->ubuntu_dock_placement_primary_index + 1;
  }
  gtk_combo_box_set_active (GTK_COMBO_BOX (ubuntu_dock_placement_combo), selection);

  g_clear_object (&current);
}

static void
session_bus_ready (GObject        *source,
                   GAsyncResult   *res,
                   CcUbuntuPanel  *panel)
{
  GDBusConnection *bus;
  GError *error = NULL;

  bus = g_bus_get_finish (res, &error);
  if (!bus)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("Failed to get session bus: %s", error->message);
        }
      g_error_free (error);
      return;
    }

  panel->display_config_manager = cc_display_config_manager_dbus_new ();
  g_signal_connect_object (panel->display_config_manager, "changed",
                           G_CALLBACK (on_screen_changed),
                           panel,
                           G_CONNECT_SWAPPED);
}


static void
iconsize_widget_refresh (GtkAdjustment *iconsize_adj, GSettings *settings)
{
  gint value = g_settings_get_int (settings, ICONSIZE_KEY);
  gtk_adjustment_set_value (iconsize_adj, (gdouble)value / 2);
}

static void
ext_iconsize_changed_callback (GSettings *settings,
                              gchar *key,
                              gpointer user_data)
{
  iconsize_widget_refresh (GTK_ADJUSTMENT (user_data), settings);
}

static gchar *
on_iconsize_format_value (GtkScale *scale, gdouble value)
{
  return g_strdup_printf ("%d", (int)value * 2);
}

static void
on_iconsize_changed (GtkAdjustment *adj, CcUbuntuPanel *panel)
{
  gint value = (gint)gtk_adjustment_get_value (adj) * 2;
  if (g_settings_get_int (panel->ubuntu_dock_settings, ICONSIZE_KEY) != value)
    g_settings_set_int (panel->ubuntu_dock_settings, ICONSIZE_KEY, value);
}

static void
on_ubuntu_dock_placement_combo_changed (GtkComboBox *combo, CcUbuntuPanel *panel)
{
  gint active = gtk_combo_box_get_active (combo);

  gboolean ubuntu_dock_on_all_monitors = g_settings_get_boolean (panel->ubuntu_dock_settings, UBUNTU_DOCK_ALL_MONITORS_KEY);
  gint ubuntu_dock_current_index = g_settings_get_int (panel->ubuntu_dock_settings, UBUNTU_DOCK_ON_MONITOR_KEY);

  if (active < 0)
    return;
  else if (active == 0) {
    if (ubuntu_dock_on_all_monitors != TRUE) {
      g_settings_set_boolean (panel->ubuntu_dock_settings, UBUNTU_DOCK_ALL_MONITORS_KEY, TRUE);
      g_settings_apply (panel->ubuntu_dock_settings);
    }
  }
  else {
    active--;
    GSettings *delayed_settings = g_settings_new (UBUNTU_DOCK_SCHEMA);
    g_settings_delay (delayed_settings);
    if (ubuntu_dock_on_all_monitors != FALSE)
      g_settings_set_boolean (delayed_settings, UBUNTU_DOCK_ALL_MONITORS_KEY, FALSE);
    if (ubuntu_dock_current_index != active) {
      if (!(ubuntu_dock_current_index == -1 && (active == panel->ubuntu_dock_placement_primary_index)))
        g_settings_set_int (delayed_settings, UBUNTU_DOCK_ON_MONITOR_KEY, active);
    }
    g_settings_apply (delayed_settings);
    g_object_unref (delayed_settings);
  }
}

static void
ext_ubuntu_dock_placement_changed_callback (GSettings *settings,
                                            guint key,
                                            gpointer user_data)
{
  CcUbuntuPanel *panel = CC_UBUNTU_PANEL (user_data);
  int selection = 0;

  if (g_settings_get_boolean (panel->ubuntu_dock_settings, UBUNTU_DOCK_ALL_MONITORS_KEY) == FALSE) {
    selection = g_settings_get_int (panel->ubuntu_dock_settings, UBUNTU_DOCK_ON_MONITOR_KEY);
    if (selection == -1)
      selection = panel->ubuntu_dock_placement_primary_index;
    selection++; // offset in combox
  }

  gtk_combo_box_set_active (GTK_COMBO_BOX (panel->ubuntu_dock_placement_combo), selection);
}

static void
cc_ubuntu_panel_init (CcUbuntuPanel *panel)
{
  GtkWidget *w;
  GtkWidget *widget, *box, *label, *seclabel;
  GtkWidget *ubuntu_dock_listbox;
  GtkWidget *pos_combo;
  GtkListBoxRow *row;
  GtkWidget *frame;
  GtkWidget *sw, *iconsize_scale;
  GtkAdjustment *iconsize_adj;
  gchar *s;
  GError *error = NULL;

  panel->builder = gtk_builder_new ();
  if (gtk_builder_add_from_resource (panel->builder,
                                     "/org/gnome/control-center/ubuntu/ubuntu.ui",
                                     &error) == 0) {
    g_error ("Error loading UI file: %s", error->message);
    g_error_free (error);
    return;
  }

  w = GTK_WIDGET (gtk_builder_get_object (panel->builder,
                                          "ccubuntu-scrolled-window"));
  ubuntu_dock_listbox = GTK_WIDGET (gtk_builder_get_object (panel->builder,
                                    "ccubuntu-listbox"));
  gtk_list_box_set_header_func (GTK_LIST_BOX (ubuntu_dock_listbox), cc_list_box_update_header_func, NULL, NULL);
  gtk_container_add (GTK_CONTAINER(panel), w);

  /* Only load if we have ubuntu dock or dash to dock installed */
  GSettingsSchema *schema = g_settings_schema_source_lookup(g_settings_schema_source_get_default(),
                                                            UBUNTU_DOCK_SCHEMA, TRUE);
  if (!schema) {
    g_resources_register (cc_ubuntu_get_resource());
    g_warning ("No Ubuntu Dock is installed here. Panel disabled. Please fix your installation.");
    return;
  }
  panel->ubuntu_dock_settings = g_settings_new_full (schema, NULL, NULL);
  g_settings_schema_unref (schema);

  /* register session bus */
  g_bus_get (G_BUS_TYPE_SESSION,
            NULL,
            (GAsyncReadyCallback) session_bus_ready,
            panel);

  /* Layout */
  label = gtk_label_new (_("Auto-hide the Dock"));
  row = GTK_LIST_BOX_ROW (gtk_list_box_row_new ());
  box = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (row), box);
  gtk_container_add (GTK_CONTAINER (ubuntu_dock_listbox), GTK_WIDGET (row));
  gtk_widget_set_margin_start (box, 20);
  gtk_widget_set_margin_end (box, 20);
  gtk_widget_set_margin_top (box, 10);
  gtk_widget_set_margin_bottom (box, 10);
  gtk_grid_set_row_spacing (GTK_GRID (box), 2);
  gtk_grid_set_column_spacing (GTK_GRID (box), 16);
  gtk_widget_set_valign (box, GTK_ALIGN_CENTER);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
  gtk_grid_attach (GTK_GRID (box), label, 0, 0, 1, 1);
  seclabel = gtk_label_new (_("The dock hides when any windows overlap with it."));
  gtk_widget_set_halign (seclabel, GTK_ALIGN_START);
  GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (seclabel));
  gtk_style_context_add_class (context, "dim-label");
  PangoAttribute *scale = pango_attr_scale_new (0.9);
  PangoAttrList *attr_lst = pango_attr_list_new();
  pango_attr_list_insert (attr_lst, scale);
  gtk_label_set_attributes (GTK_LABEL (seclabel), attr_lst);
  gtk_grid_attach(GTK_GRID(box), seclabel, 0, 1, 1, 1);
  sw = gtk_switch_new();
  g_settings_bind (panel->ubuntu_dock_settings, "dock-fixed",
                   sw, "active",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);
  gtk_widget_set_halign (sw, GTK_ALIGN_END);
  gtk_widget_set_valign (sw, GTK_ALIGN_CENTER);
  gtk_grid_attach (GTK_GRID (box), sw, 1, 0, 1, 2);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), sw);

  label = gtk_label_new (_("Icon size"));
  row = GTK_LIST_BOX_ROW (gtk_list_box_row_new ());
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 50);
  gtk_container_add (GTK_CONTAINER (row), box);
  gtk_container_add (GTK_CONTAINER (ubuntu_dock_listbox), GTK_WIDGET (row));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
  gtk_widget_set_margin_start (label, 20);
  gtk_widget_set_margin_end (label, 20);
  gtk_widget_set_margin_top (label, 16);
  gtk_widget_set_margin_bottom (label, 16);
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  /* Icon size change - we halve the sizes so we can only get even values*/
  iconsize_adj = gtk_adjustment_new (DEFAULT_ICONSIZE / 2, MIN_ICONSIZE / 2, MAX_ICONSIZE / 2, 1, 4, 0);
  iconsize_scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, iconsize_adj);
  gtk_scale_set_value_pos (GTK_SCALE (iconsize_scale), GTK_POS_LEFT);
  gtk_widget_set_size_request (iconsize_scale, 264, -1);
  gtk_scale_add_mark (GTK_SCALE (iconsize_scale), DEFAULT_ICONSIZE / 2, GTK_POS_BOTTOM, NULL);
  gtk_widget_set_margin_start (iconsize_scale, 20);
  gtk_widget_set_margin_end (iconsize_scale, 20);
  gtk_widget_set_valign (iconsize_scale, GTK_ALIGN_CENTER);
  g_signal_connect_object (panel->ubuntu_dock_settings, "changed::" ICONSIZE_KEY,
                           G_CALLBACK (ext_iconsize_changed_callback), iconsize_adj, 0);
  g_signal_connect_object (G_OBJECT (iconsize_scale), "format-value",
                           G_CALLBACK (on_iconsize_format_value), iconsize_scale, 0);
  g_signal_connect_object (iconsize_adj, "value_changed",
                           G_CALLBACK (on_iconsize_changed), panel, 0);
  iconsize_widget_refresh (iconsize_adj, panel->ubuntu_dock_settings);
  gtk_box_pack_start (GTK_BOX (box), iconsize_scale, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), iconsize_scale);

  label = gtk_label_new (_("Show on"));
  panel->show_on_row = gtk_list_box_row_new ();
  row = GTK_LIST_BOX_ROW (panel->show_on_row );
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 50);
  gtk_container_add (GTK_CONTAINER (row), box);
  gtk_container_add (GTK_CONTAINER (ubuntu_dock_listbox), GTK_WIDGET (row));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
  gtk_widget_set_margin_start (label, 20);
  gtk_widget_set_margin_end (label, 20);
  gtk_widget_set_margin_top (label, 16);
  gtk_widget_set_margin_bottom (label, 16);
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);
  panel->ubuntu_dock_placement_combo = gtk_combo_box_new ();
  gtk_widget_set_margin_start (panel->ubuntu_dock_placement_combo, 20);
  gtk_widget_set_margin_end (panel->ubuntu_dock_placement_combo, 20);
  gtk_widget_set_valign (panel->ubuntu_dock_placement_combo, GTK_ALIGN_CENTER);
  g_signal_connect_object (G_OBJECT (panel->ubuntu_dock_placement_combo), "changed",
                           G_CALLBACK (on_ubuntu_dock_placement_combo_changed), panel, 0);
  g_signal_connect_object (panel->ubuntu_dock_settings, "changed::" UBUNTU_DOCK_ALL_MONITORS_KEY,
                           G_CALLBACK (ext_ubuntu_dock_placement_changed_callback), panel, 0);
  g_signal_connect_object (panel->ubuntu_dock_settings, "changed::" UBUNTU_DOCK_ON_MONITOR_KEY,
                           G_CALLBACK (ext_ubuntu_dock_placement_changed_callback), panel, 0);
  gtk_box_pack_start (GTK_BOX (box), panel->ubuntu_dock_placement_combo, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), panel->ubuntu_dock_placement_combo);

  label = gtk_label_new (_("Position on screen"));
  row = GTK_LIST_BOX_ROW (gtk_list_box_row_new ());
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 50);
  gtk_container_add (GTK_CONTAINER (row), box);
  gtk_container_add (GTK_CONTAINER (ubuntu_dock_listbox), GTK_WIDGET (row));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
  gtk_widget_set_margin_start (label, 20);
  gtk_widget_set_margin_end (label, 20);
  gtk_widget_set_margin_top (label, 16);
  gtk_widget_set_margin_bottom (label, 16);
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);
  pos_combo = gtk_combo_box_text_new();
  gtk_widget_set_margin_start (pos_combo, 20);
  gtk_widget_set_margin_end (pos_combo, 20);
  gtk_widget_set_valign (pos_combo, GTK_ALIGN_CENTER);
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (pos_combo), "LEFT", C_("Position on screen for the Ubuntu dock", "Left"));
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (pos_combo), "BOTTOM", C_("Position on screen for the Ubuntu dock", "Bottom"));
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (pos_combo), "RIGHT", C_("Position on screen for the Ubuntu dock", "Right"));
  g_settings_bind (panel->ubuntu_dock_settings, "dock-position",
                   pos_combo, "active-id",
                   G_SETTINGS_BIND_DEFAULT);
  gtk_box_pack_start (GTK_BOX (box), pos_combo, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), pos_combo);

  gtk_widget_show_all(w);

  g_resources_register (cc_ubuntu_get_resource ());
}

static void
cc_ubuntu_panel_class_init (CcUbuntuPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  /* Separate dispose() and finalize() functions are necessary
   * to make sure we cancel the running thread before the panel
   * gets finalized */
  object_class->dispose = cc_ubuntu_panel_dispose;
  object_class->finalize = cc_ubuntu_panel_finalize;
}
