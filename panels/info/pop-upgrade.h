#include <gio/gio.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gmodule.h>

extern const gchar *POP_UPGRADE_SIGNAL_PACKAGE_FETCH_RESULT;
extern const gchar *POP_UPGRADE_SIGNAL_PACKAGE_FETCHING;
extern const gchar *POP_UPGRADE_SIGNAL_PACKAGE_FETCHED;
extern const gchar *POP_UPGRADE_SIGNAL_PACKAGE_UPGRADE;
extern const gchar *POP_UPGRADE_SIGNAL_RECOVERY_DOWNLOAD_PROGRESS;
extern const gchar *POP_UPGRADE_SIGNAL_RECOVERY_EVENT;
extern const gchar *POP_UPGRADE_SIGNAL_RECOVERY_RESULT;
extern const gchar *POP_UPGRADE_SIGNAL_RELEASE_EVENT;
extern const gchar *POP_UPGRADE_SIGNAL_RELEASE_RESULT;

extern const guint8 POP_UPGRADE_STATUS_INACTIVE;
extern const guint8 POP_UPGRADE_STATUS_FETCHING_PACKAGES;
extern const guint8 POP_UPGRADE_STATUS_RECOVERY_UPGRADE;
extern const guint8 POP_UPGRADE_STATUS_RELEASE_UPGRADE;
extern const guint8 POP_UPGRADE_STATUS_PACKAGE_UPGRADE;

extern const guint8 POP_UPGRADE_RELEASE_METHOD_OFFLINE;
extern const guint8 POP_UPGRADE_RELEASE_METHOD_RECOVERY;

extern const char *POP_UPGRADE_METHOD_FETCH_UPDATES;
extern const char *POP_UPGRADE_METHOD_RECOVERY_UPGRADE_FILE;
extern const char *POP_UPGRADE_METHOD_RECOVERY_UPGRADE_RELEASE;
extern const char *POP_UPGRADE_METHOD_RELEASE_CHECK;
extern const char *POP_UPGRADE_METHOD_RELEASE_UPGRAD;
extern const char *POP_UPGRADE_METHOD_RELEASE_REPAIR;
extern const char *POP_UPGRADE_METHOD_STATUS;
extern const char *POP_UPGRADE_METHOD_PACKAGE_UPGRADE;

extern const gchar *POP_UPGRADE_BUS_NAME;
extern const gchar *POP_UPGRADE_OBJECT_PATH;
extern const gchar *POP_UPGRADE_INTERFACE_NAME;

// Fetch a static string which corresponds to an event ID.
//
// Returns `NULL` if the event ID is out of range.
const gchar *pop_upgrade_recovery_event_as_str (guint8 event);

// Fetch a static string which corresponds to an event ID.
//
// Returns `NULL` if the event ID is out of range.
const gchar *pop_upgrade_release_event_as_str (guint8 event);

// When used with `pop_upgrade_daemon_release_check ()`, this will contain the
// current release version, the next release version, and whether the next
// release is available.
typedef struct {
    gchar *current;
    gchar *next;
    gboolean available;
} ReleaseCheck;

// Constructs a new release status struct
ReleaseCheck release_check_new (void);

// Free strings which are contained within struct.
void release_check_free (ReleaseCheck *self);

// When used ith `pop_upgrade_daemon_release_status ()`, this will contain the
// status of the daemon.
typedef struct {
    guint8 status;
    guint8 sub_status;
} PopUpgradeDaemonStatus;

// Constructs a new daemon status struct.
PopUpgradeDaemonStatus pop_upgrade_daemon_status_new (void);

// Manages a connection to Pop's upgrade daemon.
typedef struct {
    GDBusProxy *proxy;
} PopUpgradeDaemon;

// Creates an empty value which hasn't been connected yet.
PopUpgradeDaemon pop_upgrade_daemon_new (void);

// Attempts to connect the empty daemon value to the daemon.
int pop_upgrade_daemon_connect (PopUpgradeDaemon *self, GError **error);

// Ask the daemon to perform a recovery upgrade by the Pop release API
int pop_upgrade_daemon_recovery_upgrade_by_release (PopUpgradeDaemon *self,
                                                    GError **error, char *version,
                                                    gchar *arch, guint8 flags);

// Ask the daemon if a new release is available.
int pop_upgrade_daemon_release_check (PopUpgradeDaemon *self,
                                      GError **error, ReleaseCheck *status);

/// Ask the daemon to perform a release upgrade.
int pop_upgrade_daemon_release_upgrade(PopUpgradeDaemon *self, GError **error,
                                       guint8 how, gchar *from, gchar *to);

// Ask the daemon to request that a release upgrade is performed.
int pop_upgrade_daemon_recovery_upgrade_by_release(PopUpgradeDaemon *self,
                                                   GError **error,
                                                   gchar *version, gchar *arch,
                                                   guint8 flags);

// Ask the daemon to attempt to perform any system repairs necessary.
int pop_upgrade_daemon_repair (PopUpgradeDaemon *self, GError **error);

// Ask the daemon about its current status.
int pop_upgrade_daemon_status (PopUpgradeDaemon *self,
                               GError **error,
                               PopUpgradeDaemonStatus *status);

typedef struct {
    GtkWidget *button;
    GtkWidget *label;
    GtkWidget *button_box;
    GtkWidget *progress;
    GtkWidget *stack;
} PopUpgradeOption;

typedef struct {
    PopUpgradeOption os;
    PopUpgradeOption rec;
} PopUpgradeWidgets;

PopUpgradeWidgets pop_upgrade_frame (GtkFrame *frame);
