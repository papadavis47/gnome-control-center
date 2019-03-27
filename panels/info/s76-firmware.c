#include "s76-firmware.h"
#include <glib/gi18n.h>

static void print (gchar *element, gpointer data) {
  g_info ("firmware description: %s", element);
}

void s76_firmware_check (
  S76FirmwareDaemon  **firmware_daemon,
  S76FirmwareVersion **firmware_version,
  GtkButton           *firmware_button,
  GtkLabel            *firmware_label,
  gchar              **firmware_digest,
  gchar              **firmware_changelog
) {
  gtk_widget_hide (GTK_WIDGET (firmware_button));
  const gchar *label;

  if (g_file_test ("/sys/firmware/efi", G_FILE_TEST_IS_DIR)) {
    label = _("No Updates Available");

    *firmware_daemon = g_slice_new0 (S76FirmwareDaemon);
    *firmware_version = g_slice_new0 (S76FirmwareVersion);
    **firmware_version = s76_firmware_version_new ();

    if (!s76_firmware_daemon_connect (*firmware_daemon)) {
      int needs_update = s76_firmware_daemon_needs_update (
        *firmware_daemon,
        *firmware_version,
        firmware_digest,
        firmware_changelog
      );

      if (needs_update == S76_FIRMWARE_NEEDS_UPDATE) {
        g_info ("firmware update is available: %s", (*firmware_version)->bios);
        g_ptr_array_foreach ((*firmware_version)->changes, print, NULL);

        label = _("Firmware Updates Available");
        gtk_widget_show (GTK_WIDGET (firmware_button));
      }
    }
  } else {
    label = _("Only Supported on EFI Installs");
  }

  gtk_label_set_label (firmware_label, label);
}
