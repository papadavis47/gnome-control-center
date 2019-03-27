#include <s76-firmware-daemon.h>
#include <json-glib/json-glib.h>
#include <json-glib/json-gobject.h>

const int S76_FIRMWARE_NEEDS_UPDATE = 1;

static const gchar *get_component (JsonReader *reader, char *member_name);
static void component_set (gchar **target, const gchar *input);
static void changelog_set (GPtrArray *array, const gchar *input);

S76FirmwareDaemon s76_firmware_daemon_new (void) {
  S76FirmwareDaemon daemon = { NULL };
  return daemon;
}

int s76_firmware_daemon_connect (S76FirmwareDaemon *self) {
  GError *error = NULL;

  self->proxy = g_dbus_proxy_new_for_bus_sync (
      G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, NULL,
      "com.system76.FirmwareDaemon", "/com/system76/FirmwareDaemon",
      "com.system76.FirmwareDaemon", NULL, &error);

  if (self->proxy == NULL) {
    g_warning ("failed to reach S76Firmware: %s", error->message);
    return -1;
  }

  return 0;
}

int s76_firmware_daemon_download (S76FirmwareDaemon  *self,
                                  gchar             **digest,
                                  gchar             **changelog)
{
  GError *error = NULL;
  GVariant *retval = NULL;

  retval = g_dbus_proxy_call_sync (self->proxy, "Download", NULL,
                                   G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

  if (retval == NULL) {
    g_warning ("failed to call Download on S76Firmware: %s\n", error->message);
    return -1;
  }

  g_variant_get (retval, "(ss)", digest, changelog);

  return 0;
}

int s76_firmware_daemon_bios (S76FirmwareDaemon *self,
                              gchar            **model,
                              gchar            **version)
{
  GError *error = NULL;
  GVariant *retval = NULL;

  retval = g_dbus_proxy_call_sync (self->proxy, "Bios", NULL,
                                   G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

  if (retval == NULL) {
    g_warning ("failed to call Bios on S76Firmware: %s\n", error->message);
    return -1;
  }

  g_variant_get (retval, "(ss)", model, version);

  return 0;
}

int s76_firmware_daemon_schedule (S76FirmwareDaemon *self, gchar *digest) {
  GError *error = NULL;
  GVariant *retval = NULL;

  retval = g_dbus_proxy_call_sync (self->proxy, "Schedule",
                                   g_variant_new ("s", digest),
                                   G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

  if (retval == NULL) {
    g_warning ("failed to call Schedule on S76Firmware: %s\n", error->message);
    return -1;
  }

  return 0;
}

int s76_firmware_daemon_needs_update (S76FirmwareDaemon *self,
                                      S76FirmwareVersion *sversion,
                                      gchar **digest, gchar **changelog) {
  g_autofree gchar *model, *version = NULL;
  int valid = s76_firmware_daemon_bios (self, &model, &version) == 0 &&
              s76_firmware_daemon_download (self, digest, changelog) == 0 &&
              s76_firmware_version_from_changelog (sversion, *changelog, version) == 0;

  if (!valid) {
    return -1;
  }

  int result = 0;
  if (!g_str_equal (version, sversion->bios)) {
    result = S76_FIRMWARE_NEEDS_UPDATE;
  }

  return result;
}

S76FirmwareVersion s76_firmware_version_new (void) {
  S76FirmwareVersion version = { NULL, NULL, NULL, NULL, g_ptr_array_new () };
  return version;
}

int s76_firmware_version_from_changelog (S76FirmwareVersion *self,
                                         gchar *changelog,
                                         gchar *current_bios)
{
  // Construct the reader from the changelog data.
  JsonParser *parser = json_parser_new ();
  json_parser_load_from_data (parser, changelog, -1, NULL);
  JsonReader *reader = json_reader_new (json_parser_get_root (parser));

  // Fetch the array from the versions field.
  json_reader_read_member (reader, "versions");
  gboolean found_current = FALSE;

  if (json_reader_is_array (reader)) {
    guint nelements = json_reader_count_elements (reader);
    for (guint index = 0; index < nelements; index = index + 1) {
      json_reader_read_element (reader, index);

      if (json_reader_is_object (reader)) {
        const gchar *bios = get_component (reader, "bios");
        component_set (&self->bios, bios);
        found_current = g_str_equal (current_bios, bios);

        component_set (&self->ec, get_component (reader, "ec"));
        component_set (&self->ec2, get_component (reader, "ec2"));
        component_set (&self->me, get_component (reader, "me"));

        if (!found_current) {
          changelog_set (self->changes, get_component (reader, "description"));
        }
      }

      json_reader_end_element (reader);
    }
  }

  g_object_unref (reader);
  g_object_unref (parser);

  return s76_firmware_version_verify (self);
}

void s76_firmware_version_free (S76FirmwareVersion *self) {
  if (self != NULL) {
    g_clear_pointer (&self->bios, g_free);
    g_clear_pointer (&self->ec, g_free);
    g_clear_pointer (&self->ec2, g_free);
    g_clear_pointer (&self->me, g_free);
    g_ptr_array_free (self->changes, TRUE);
  }
}

int s76_firmware_version_verify (S76FirmwareVersion *self) {
  if (!self->bios) {
    g_warning ("firmware bios version is null");
    return -1;
  } else if (!self->ec) {
    g_warning ("firmware ec version is null");
    return -1;
  } else if (!self->me) {
    g_warning ("firmware me version is null");
    return -1;
  }

  return 0;
}

static const gchar *get_component (JsonReader *reader, char *member_name) {
  const gchar *value = NULL;
  json_reader_read_member (reader, member_name);

  if (json_reader_is_value (reader)) {
    value = json_reader_get_string_value (reader);
  }

  json_reader_end_member (reader);
  return value;
}

static void component_set (gchar **target, const gchar *input) {
  if (input != NULL && *target == NULL) {
    *target = g_strdup (input);
  }
}

static void changelog_set (GPtrArray *array, const gchar *input) {
  if (input != NULL) {
    g_ptr_array_insert (array, -1, (gpointer) g_strdup (input));
  }
}
