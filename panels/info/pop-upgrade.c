#include "pop-upgrade.h"
#include <glib/gi18n.h>

const char *METHOD_FETCH_UPDATES = "FetchUpdates";
const char *METHOD_RECOVERY_UPGRADE_FILE = "RecoveryUpgradeFile";
const char *METHOD_RECOVERY_UPGRADE_RELEASE = "RecoveryUpgradeRelease";
const char *METHOD_RELEASE_CHECK = "ReleaseCheck";
const char *METHOD_RELEASE_UPGRADE = "ReleaseUpgrade";
const char *METHOD_RELEASE_REPAIR = "ReleaseRepair";
const char *METHOD_STATUS = "Status";
const char *METHOD_PACKAGE_UPGRADE = "UpgradePackages";

const guint8 POP_UPGRADE_RELEASE_METHOD_OFFLINE = 1;
const guint8 POP_UPGRADE_RELEASE_METHOD_RECOVERY = 2;

const gchar *POP_UPGRADE_SIGNAL_PACKAGE_FETCH_RESULT = "PackageFetchResult";
const gchar *POP_UPGRADE_SIGNAL_PACKAGE_FETCHING = "PackageFetching";
const gchar *POP_UPGRADE_SIGNAL_PACKAGE_FETCHED = "PackageFetched";
const gchar *POP_UPGRADE_SIGNAL_PACKAGE_UPGRADE = "PackageUpgrade";
const gchar *POP_UPGRADE_SIGNAL_RECOVERY_DOWNLOAD_PROGRESS = "RecoveryDownloadProgress";
const gchar *POP_UPGRADE_SIGNAL_RECOVERY_EVENT = "RecoveryUpgradeEvent";
const gchar *POP_UPGRADE_SIGNAL_RECOVERY_RESULT = "RecoveryUpgradeResult";
const gchar *POP_UPGRADE_SIGNAL_RELEASE_EVENT = "ReleaseUpgradeEvent";
const gchar *POP_UPGRADE_SIGNAL_RELEASE_RESULT = "ReleaseUpgradeResult";

const guint8 POP_UPGRADE_STATUS_INACTIVE = 0;
const guint8 POP_UPGRADE_STATUS_FETCHING_PACKAGES = 1;
const guint8 POP_UPGRADE_STATUS_RECOVERY_UPGRADE = 2;
const guint8 POP_UPGRADE_STATUS_RELEASE_UPGRADE = 3;
const guint8 POP_UPGRADE_STATUS_PACKAGE_UPGRADE = 4;

const gchar *POP_UPGRADE_BUS_NAME = "com.system76.PopUpgrade";
const gchar *POP_UPGRADE_OBJECT_PATH = "/com/system76/PopUpgrade";
const gchar *POP_UPGRADE_INTERFACE_NAME = "com.system76.PopUpgrade";

const gchar *pop_upgrade_recovery_event_as_str (guint8 event) {
  switch (event) {
  case 1:
    return _("Fetching recovery files");
  case 2:
    return _("Verifying checksums of fetched files");
  case 3:
    return _("Syncing recovery files with recovery partition");
  case 4:
    return _("Recovery partition upgrade completed");
  case 5:
    return _("Recovery partition upgrade failed");
  default:
    return NULL;
  }
}

const gchar *pop_upgrade_release_event_as_str (guint8 event) {
  switch (event) {
  case 1:
    return _("Updating package lists for the current release");
  case 2:
    return _("Fetching updated packages for the current release");
  case 3:
    return _("Upgrading packages for the current release");
  case 4:
    return _("Ensuring that system-critical packages are installed");
  case 5:
    return _("Update the source lists to the new release");
  case 6:
    return _("Fetching packages for the new release");
  case 7:
    return _("Attempting live upgrade to the new release");
  case 8:
    return _("Setting up the system to perform an offline upgrade on the next boot");
  case 9:
    return _("Setting up the recovery partition to install the new release");
  case 10:
    return _("The new release is ready to install");
  case 11:
    return _("The new release was successfully installed");
  case 12:
    return _("An error occurred while setting up the release upgrade");
  default:
    return NULL;
  }
}

ReleaseCheck release_check_new (void) {
    ReleaseCheck status = { NULL, NULL, FALSE };
    return status;
}

PopUpgradeDaemonStatus pop_upgrade_daemon_status_new (void) {
  PopUpgradeDaemonStatus status = { 0, 0 };
  return status;
}

void release_check_free (ReleaseCheck *self) {
    if (NULL != self) {
        if (NULL != self->current) {
            g_clear_pointer (&self->current, g_free);
        }

        if (NULL != self->next) {
            g_clear_pointer (&self->next, g_free);
        }
    }
}

PopUpgradeDaemon pop_upgrade_daemon_new (void) {
    PopUpgradeDaemon daemon = { NULL };
    return daemon;
}

int pop_upgrade_daemon_connect (PopUpgradeDaemon *self, GError **error) {
    g_info ("attempting to connect to the Pop upgrade daemon");

    self->proxy = g_dbus_proxy_new_for_bus_sync (
        G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, NULL,
        POP_UPGRADE_BUS_NAME, POP_UPGRADE_OBJECT_PATH,
        POP_UPGRADE_INTERFACE_NAME, NULL, error);

    if (self->proxy == NULL) {
        g_warning ("failed to reach PopUpgrade: %s", (*error)->message);
        return -1;
    }

    return 0;
}

int pop_upgrade_daemon_recovery_upgrade_by_release (PopUpgradeDaemon *self,
                                                    GError **error,
                                                    gchar *version, gchar *arch, guint8 flags) {
    g_info ("upgrading the recovery partition by release");

    GVariant *input[3];
    input[0] = g_variant_new_string (version);
    input[1] = g_variant_new_string (arch);
    input[2] = g_variant_new_byte (flags);

    g_autoptr(GVariant) retval = g_dbus_proxy_call_sync (self->proxy, METHOD_RECOVERY_UPGRADE_RELEASE,
                                                         g_variant_new_tuple (input, 3),
                                                         G_DBUS_CALL_FLAGS_NONE, -1, NULL, error);

    if (*error != NULL) {
      g_warning ("failed to call %s on PopUpgrade: %s", METHOD_RECOVERY_UPGRADE_RELEASE, (*error)->message);
      return -1;
    }

    return 0;
}

int pop_upgrade_daemon_release_check (PopUpgradeDaemon *self, GError **error, ReleaseCheck *status) {
    g_info ("checking for a new Pop release");
    if (NULL == status) {
        g_warning ("status input is null, when it should not be");
        return -1;
    }

    g_autoptr(GVariant) retval = NULL;

    retval = g_dbus_proxy_call_sync (self->proxy, METHOD_RELEASE_CHECK, NULL,
                                     G_DBUS_CALL_FLAGS_NONE, -1, NULL, error);

    if (*error != NULL) {
        g_warning("error happened");
        g_warning ("failed to call %s on PopUpgrade: %s", METHOD_RELEASE_CHECK, (*error)->message);
        return -1;
    }

    const char *expected = "(ssb)";

    if (retval == NULL) {
      g_warning ("failed to call %s on PopUpgrade: expected %s, but received nothing",
                 METHOD_STATUS, expected);
      return -2;
    }

    g_variant_get (retval, expected, &status->current, &status->next, &status->available);

    return 0;
}

int pop_upgrade_daemon_release_upgrade (PopUpgradeDaemon *self, GError **error, guint8 how,
                                        gchar *from, gchar *to)
{
    g_info ("beginning release upgrade for Pop");

    GVariant *input[3];
    input[0] = g_variant_new_byte (how);
    input[1] = g_variant_new_string (from);
    input[2] = g_variant_new_string (to);

    g_autoptr(GVariant) retval = g_dbus_proxy_call_sync (self->proxy, METHOD_RELEASE_UPGRADE,
                                                         g_variant_new_tuple (input, 3),
                                                         G_DBUS_CALL_FLAGS_NONE, -1, NULL, error);

    if (*error != NULL) {
        g_warning ("failed to call %s on PopUpgrade: %s", METHOD_RELEASE_UPGRADE, (*error)->message);
        return -1;
    }

    return 0;
}

int pop_upgrade_daemon_repair (PopUpgradeDaemon *self, GError **error) {
    g_info ("pop is checking for required system repairs");

    g_autoptr(GVariant) retval = g_dbus_proxy_call_sync (self->proxy, METHOD_RELEASE_REPAIR, NULL,
                                                         G_DBUS_CALL_FLAGS_NONE, -1, NULL, error);

    if (*error != NULL) {
        g_warning ("failed to call %s on PopUpgrade: %s", METHOD_RELEASE_REPAIR, (*error)->message);
        return -1;
    }

    return 0;
}

int pop_upgrade_daemon_status (PopUpgradeDaemon *self, GError **error, PopUpgradeDaemonStatus *status) {
    g_info ("checking the status of the Pop upgrade daemon");
    g_autoptr(GVariant) retval = NULL;

    if (NULL == self || NULL == self->proxy) {
      g_warning ("pop_upgrade_daemon_status called with null daemon/proxy");
      return -1;
    }

    retval = g_dbus_proxy_call_sync (self->proxy, METHOD_STATUS, NULL,
                                     G_DBUS_CALL_FLAGS_NONE, -1, NULL, error);

    if (error != NULL && *error != NULL) {
        g_warning ("failed to call %s on PopUpgrade: %s", METHOD_STATUS, (*error)->message);
        g_variant_unref (retval);
        return -1;
    }

    const char *expected = "(yy)";

    if (retval == NULL) {
      g_warning ("failed to call %s on PopUpgrade: expected %s, but received nothing", METHOD_STATUS, expected);
      return -2;
    }
    status->status = g_variant_get_byte (
      g_variant_get_child_value (retval, 0)
    );

    status->sub_status = g_variant_get_byte (
      g_variant_get_child_value (retval, 1)
    );

    return 0;
}

static PopUpgradeOption pop_upgrade_option_new () {
  // On click, this will initiate the pop upgrade process.
  GtkWidget *button = gtk_button_new ();
  gtk_label_set_use_markup (GTK_LABEL (gtk_bin_get_child (GTK_BIN (button))), TRUE);
  gtk_widget_set_can_focus (button, TRUE);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_style_context_add_class (gtk_widget_get_style_context (button),
                               GTK_STYLE_CLASS_SUGGESTED_ACTION);

  // The label to describe availability of an upgrade.
  GtkWidget *label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), button);

  // A box for containing the label and button.
  GtkWidget *button_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_container_add (GTK_CONTAINER (button_box), label);
  gtk_box_pack_end (GTK_BOX (button_box), button, FALSE, FALSE, FALSE);

  // Displays progress of the release upgrade in progress.
  GtkWidget *progress = gtk_progress_bar_new ();
  gtk_progress_bar_set_ellipsize (GTK_PROGRESS_BAR (progress), PANGO_ELLIPSIZE_END);
  gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR (progress), TRUE);

  // Stack for containing the button box and progress bar.
  GtkWidget *stack = gtk_stack_new ();
  gtk_container_add (GTK_CONTAINER (stack), button_box);
  gtk_container_add (GTK_CONTAINER (stack), progress);
  gtk_widget_set_margin_start (stack, 20);
  gtk_widget_set_margin_end (stack, 20);
  gtk_widget_set_margin_top (stack, 9);
  gtk_widget_set_margin_bottom (stack, 9);
  gtk_widget_set_visible (stack, TRUE);
  gtk_widget_show_all (stack);
  gtk_stack_set_visible_child (GTK_STACK (stack), button_box);

  PopUpgradeOption option = { button, label, button_box, progress, stack };
  return option;
}

PopUpgradeWidgets pop_upgrade_frame (GtkFrame *frame) {
  PopUpgradeOption os = pop_upgrade_option_new ();
  PopUpgradeOption rec = pop_upgrade_option_new ();

  // Each row is added to a list, primarily used for visual style.
  GtkWidget *list = gtk_list_box_new ();
  gtk_widget_set_can_focus (list, TRUE);
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);
  gtk_container_add (GTK_CONTAINER (list), os.stack);
  gtk_container_add (GTK_CONTAINER (list), rec.stack);
  gtk_widget_show_all (list);

  // Add the list to the UI in the upgrade frame.
  gtk_container_add (GTK_CONTAINER (frame), list);

  PopUpgradeWidgets widgets = { os, rec };
  return widgets;
}
