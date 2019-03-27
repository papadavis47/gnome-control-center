#include "gtk/gtk.h"

#include "s76-firmware-daemon.h"
#include "s76-firmware-dialog.h"

void s76_firmware_check(S76FirmwareDaemon  **firmware_daemon,
                        S76FirmwareVersion **firmware_version,
                        GtkButton *firmware_button,
                        GtkLabel *firmware_label,
                        gchar **firmware_digest,
                        gchar **firmware_changelog);
