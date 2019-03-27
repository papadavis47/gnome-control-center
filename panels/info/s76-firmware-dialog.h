/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2019 System76
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
 */

#include <glib.h>
#include <glib-object.h>
#include <gobject/gvaluecollector.h>
#include <gtk/gtk.h>

typedef struct {
    GtkButton *reboot;
    GtkDialog *dialog;
} FirmwareUpdateDialog;

typedef struct {
  GtkDialog *dialog;
  gpointer data;
} FirmwareScheduleData;

FirmwareUpdateDialog firmware_dialog_new(gchar *version, GPtrArray *changes);

void firmware_dialog_connect_reboot(FirmwareUpdateDialog *dialog,
                                    GCallback callback, gpointer data);

int firmware_dialog_run(FirmwareUpdateDialog *dialog);
