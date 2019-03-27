#include <dmi-info.h>
#include <glib.h>

static char *get_sys_info (gchar *path) {
  g_autofree gchar *buffer = NULL;
  return g_file_get_contents (path, &buffer, NULL, NULL)
    ? g_strdup (g_strstrip (buffer))
    : NULL;
}

char *get_product_name (void) {
  return get_sys_info ("/sys/class/dmi/id/product_name");
}

char *get_product_version (void) {
  return get_sys_info ("/sys/class/dmi/id/product_version");
}

char *get_sys_vendor (void) {
  return get_sys_info ("/sys/class/dmi/id/sys_vendor");
}
