#include <gio/gio.h>
#include <glib.h>
#include <gmodule.h>

extern const int S76_FIRMWARE_NEEDS_UPDATE;

typedef struct {
  gchar *bios;
  gchar *ec;
  gchar *ec2;
  gchar *me;
  GPtrArray *changes;
} S76FirmwareVersion;

S76FirmwareVersion s76_firmware_version_new (void);

// Verify that all the fields were set.
int s76_firmware_version_verify (S76FirmwareVersion *version);

// Get the latest firmware information from the changelog.
int s76_firmware_version_from_changelog(S76FirmwareVersion *self,
                                        gchar *changelog, gchar *current_bios);

// Free the gchar strings contained by the struct.
void s76_firmware_version_free(S76FirmwareVersion *version);

typedef struct {
  GDBusProxy *proxy;
} S76FirmwareDaemon;

S76FirmwareDaemon s76_firmware_daemon_new (void);

int s76_firmware_daemon_connect (S76FirmwareDaemon *daemon);

int s76_firmware_daemon_download (S76FirmwareDaemon *daemon,
                                  gchar       **digest,
                                  gchar       **changelog);

int s76_firmware_daemon_bios (S76FirmwareDaemon *daemon,
                              gchar            **model,
                              gchar            **version);

int s76_firmware_daemon_schedule(S76FirmwareDaemon *self, gchar *digest);


// Returns -1 on error, 0 on no update required, and 1 on an update
// required.
int s76_firmware_daemon_needs_update(S76FirmwareDaemon *daemon,
                                     S76FirmwareVersion *sversion,
                                     gchar **digest, gchar **changelog);
