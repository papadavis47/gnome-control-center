#include <s76-firmware-dialog.h>
#include <glib.h>
#include <glib/gi18n.h>

static void cancel_dialog (GtkButton *button, GtkDialog *dialog);

typedef struct {
  GString *output;
  gchar   *pattern;
} FoldData;

static void join_str_lambda (gchar *element, FoldData *data) {
  g_string_append (data->output, data->pattern);
  g_string_append (data->output, element);
}

static gchar *join_str (GPtrArray *array, gchar *pattern) {
  GString *strbuf = g_string_new (NULL);
  FoldData data = { strbuf, pattern };
  g_ptr_array_foreach (array, join_str_lambda, &data);
  gchar *output = g_string_free (data.output, FALSE);
  return output;
}

FirmwareUpdateDialog firmware_dialog_new (gchar *version, GPtrArray *changes) {
  GtkLabel *title = (GtkLabel*) gtk_label_new ("<b>Firmware Update</b>");
  gtk_label_set_use_markup (title, TRUE);

  g_autofree gchar *changelog = join_str (changes, "\t* ");

  GtkLabel *text = (GtkLabel*) gtk_label_new (
    g_strdup_printf (_("Firmware version %s is available. Fixes and features"
      " include:\n\n%s\n\nIf you're on a laptop, <b>plug into power</b>"
      " before you begin."), version, changelog)
  );

  gtk_label_set_line_wrap (text, TRUE);
  gtk_label_set_use_markup (text, TRUE);
  gtk_widget_set_valign (GTK_WIDGET (text), GTK_ALIGN_START);

  GtkWidget *icon = gtk_image_new_from_icon_name ("application-x-cd-image", 6);
  gtk_widget_set_valign (GTK_WIDGET (icon), GTK_ALIGN_START);

  GtkDialog *dialog = (GtkDialog*) g_object_new (GTK_TYPE_DIALOG,
                                    "use-header-bar", TRUE,
                                    NULL);

  GtkButton *cancel = GTK_BUTTON (gtk_button_new_with_label ("Cancel"));
  g_signal_connect(cancel, "clicked", G_CALLBACK (cancel_dialog), dialog);

  GtkButton *reboot = GTK_BUTTON (gtk_button_new_with_label ("Reboot and Install"));
  gtk_style_context_add_class (
    gtk_widget_get_style_context ((GtkWidget*) reboot),
    GTK_STYLE_CLASS_DESTRUCTIVE_ACTION
  );

  GtkHeaderBar *header = GTK_HEADER_BAR (gtk_dialog_get_header_bar (dialog));
  gtk_header_bar_set_custom_title (header, GTK_WIDGET (title));
  gtk_header_bar_set_show_close_button (header, FALSE);
  gtk_header_bar_pack_start (header, GTK_WIDGET (cancel));
  gtk_header_bar_pack_end (header, GTK_WIDGET (reboot));

  GtkContainer *box = GTK_CONTAINER (gtk_dialog_get_content_area (dialog));
  gtk_orientable_set_orientation (GTK_ORIENTABLE (box), GTK_ORIENTATION_HORIZONTAL);
  gtk_container_set_border_width (box, 12);
  gtk_box_set_spacing (GTK_BOX (box), 6);
  gtk_container_add (box, GTK_WIDGET (icon));
  gtk_container_add (box, GTK_WIDGET (text));

  g_signal_connect_swapped (dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog);
  gtk_widget_show_all (GTK_WIDGET (dialog));

  FirmwareUpdateDialog firmware_dialog = { reboot, dialog };
  return firmware_dialog;
}

void firmware_dialog_connect_reboot (FirmwareUpdateDialog *dialog, GCallback callback, gpointer data) {
  FirmwareScheduleData *cbdata = g_slice_new0 (FirmwareScheduleData);
  cbdata->dialog = dialog->dialog;
  cbdata->data = data;

  g_signal_connect (dialog->reboot, "clicked", callback, cbdata);
}

int firmware_dialog_run (FirmwareUpdateDialog *dialog) {
  return gtk_dialog_run (dialog->dialog);
}

static void cancel_dialog (GtkButton *button, GtkDialog *dialog) {
  gtk_dialog_response (dialog, GTK_RESPONSE_CANCEL);
}
