#include <iostream>
#include <gtk/gtk.h>
#include <cstdint>
#include <cstdio>
#include <nlohmann/json.hpp>
#include <string>
#include <list>
#include <cstring>
#include <fstream>
#include <time.h>
#include <cmath>
#include <sys/stat.h>
#include <chrono>
#include <ctime>
#include <sstream>
#include <filesystem>
#include <stdio.h>
#include "m_includes/miniz/miniz.c"

int IMP_VERSION = 1;

using string = std::string;

/* structs */

typedef struct keybind {
	int key = -1;
  string key_str = "";
	string dest_path = "";
  GtkWidget *row = NULL;
  struct keybind* prev = NULL;
  struct keybind* next = NULL;
} keybind;

enum {
	IMF_MOVE = 0,
  IMF_COPY = 1,
  IMF_UNS_ASK = 0,
  IMF_UNS_SKIP = 1
};

string W_SOURCEFOLDER_FALLBACK = "<source>";
string W_UNSFOLDER_FALLBACK = "<move_to>";
string W_KEYBIND_FALLBACK = "<key>";
string W_DESTFOLDER_FALLBACK = "<dest>";
string W_SESEDIT_FALLBACK = "Edit session";
string W_SESEDIT_NEW_FALLBACK = "New session";

/* variables */
GtkApplication *app;
GtkWidget *window;
GtkWidget *image_header;
GtkWidget *image_content;
GdkPixbuf *image_pixbuf;
GdkPixbuf *image_pixbuf_fallback;
GdkPixbuf *image_pixbuf_generic;
GtkWidget *image_paned1;
GtkWidget *focus_button_event_box;
GtkWidget *start_ses_button;
GtkWidget *kb_listbox;
GdkPixbuf *WINDOW_ICON;
string WORKING_PATH;

int kb_presskey_prompt_key = -1;
int kb_row_max_width = -1;
bool kb_new_prompt_is_open = false;
bool kb_new_prompt_is_continuous = false;
keybind kb_new_prompt_keybind;
std::vector<std::filesystem::path> imf_paths;
string imf_toplevel_path = "";
int imf_action = IMF_MOVE;
int imf_uns_action = IMF_UNS_ASK;
string imf_path_current;
int imf_current_width = -1;
int imf_current_height = -1;
int imf_total_images = 0;
int imf_image_iterator = 0;
keybind* keybinds_first = NULL;
keybind* keybinds_last = NULL;

/* function declarations */
void imf_set_toplevel_path(string path);
void kb_delete_rows();
void kb_new_prompt();
void kb_pushback(keybind &kb);


string fi_read(string path) {
  std::ifstream f(path);
  std::string file( (std::istreambuf_iterator<char>(f) ),
                       (std::istreambuf_iterator<char>()    ) );
  return file;
}

void fi_write(string path, string data) {
  std::ofstream(path) << data;
}

void w_message(GtkWidget *w, string title, string message) {
  GtkWidget *dialog, *content_area;
  GtkDialogFlags flags = GTK_DIALOG_MODAL;
  dialog = gtk_dialog_new_with_buttons (title.c_str(),
                                         GTK_WINDOW(window),
                                         flags,
                                         "OK",
                                         GTK_RESPONSE_ACCEPT,
                                         NULL);
  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));


  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

  GtkWidget *info_label = gtk_label_new(message.c_str());

  gtk_box_pack_start(GTK_BOX(box), info_label, false, false, 0);
  gtk_widget_set_margin_top(box, 8);
  gtk_widget_set_margin_start(box, 8);
  gtk_widget_set_margin_end(box, 8);
  gtk_widget_set_margin_bottom(box, 8); 

  gtk_container_add (GTK_CONTAINER (content_area), box);
  g_signal_connect_swapped (dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);
  gtk_widget_show_all (dialog);
}

string w_select_folder_popup(GtkWidget *w, string title) {
  GtkWidget *dialog = gtk_file_chooser_dialog_new (title.c_str(),
                                      GTK_WINDOW(w),
                                      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                      ("Cancel"),
                                      GTK_RESPONSE_CANCEL,
                                      ("Select"),
                                      GTK_RESPONSE_ACCEPT,
                                      NULL);

  gint res = gtk_dialog_run (GTK_DIALOG (dialog));
  if (res != GTK_RESPONSE_ACCEPT) {
    gtk_widget_destroy(dialog);
    return "";
  }

  char *filename_c;
  GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
  filename_c = gtk_file_chooser_get_filename (chooser);
  string filename = filename_c;
  gtk_widget_destroy(dialog);
  return filename;
}


void ses_save_session(GtkWidget *w) {
  GtkWidget *dialog = gtk_file_chooser_dialog_new ("Save",
                                      GTK_WINDOW(window),
                                      GTK_FILE_CHOOSER_ACTION_SAVE,
                                      ("Cancel"),
                                      GTK_RESPONSE_CANCEL,
                                      ("Save"),
                                      GTK_RESPONSE_ACCEPT,
                                      NULL);
  GtkFileFilter *filter = gtk_file_filter_new();
  gtk_file_filter_add_pattern(filter, "*.impsesh");
  gtk_file_filter_set_name(filter, ".impsesh");
  gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), filter);

  gint res = gtk_dialog_run (GTK_DIALOG (dialog));
  if (res != GTK_RESPONSE_ACCEPT) {
    gtk_widget_destroy(dialog);
    return;
  }

  char *filename;
  GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
  filename = gtk_file_chooser_get_filename (chooser);
  string path = filename;
  if (path.size()>8) {
    string filename_extension = path.substr(path.size()-8);
    if (filename_extension!=".impsesh") {
      path.append(".impsesh");
    }
  } else {
    path.append(".impsesh");
  }

	// write session variables to .impsesh
  string out = "imp_";
  out.append(std::to_string(IMP_VERSION));
  out.append("\n");
  out.append(imf_toplevel_path);
  out.append("\n");
  out.append(std::to_string(imf_action));
  out.append("\n");
  out.append(std::to_string(imf_uns_action));
  out.append("\n");
  out.append(std::to_string(kb_new_prompt_is_continuous));
  out.append("\n");

  keybind *this_kb = keybinds_first;
  while (this_kb!=NULL) {
    out.append(std::to_string(this_kb->key));
    out.append("\n");
    out.append(this_kb->dest_path);
    out.append("\n");
    this_kb=this_kb->next;
  }

  fi_write(path, out);

  gtk_widget_destroy(dialog);
}

void ses_open_session(GtkWidget *w) {
  GtkWidget *dialog = gtk_file_chooser_dialog_new ("Open .impsesh",
                                      GTK_WINDOW(window),
                                      GTK_FILE_CHOOSER_ACTION_OPEN,
                                      ("Cancel"),
                                      GTK_RESPONSE_CANCEL,
                                      ("Select"),
                                      GTK_RESPONSE_ACCEPT,
                                      NULL);
  GtkFileFilter *filter = gtk_file_filter_new();
  gtk_file_filter_add_pattern(filter, "*.impsesh");
  gtk_file_filter_set_name(filter, ".impsesh");
  gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), filter);

  gint res = gtk_dialog_run (GTK_DIALOG (dialog));
  if (res != GTK_RESPONSE_ACCEPT) {
    gtk_widget_destroy(dialog);
    return;
  }

  char *filename;
  GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
  filename = gtk_file_chooser_get_filename (chooser);

  std::vector<string> lines;
  string in = fi_read(filename);
  int inf = in.find("\n");
  while (inf!=in.npos) {
    string this_line = in.substr(0,inf);
    lines.push_back(this_line);
    in = in.substr(inf+1);
    inf = in.find("\n");
  }

  if (lines.size()<5) {
    w_message(window,"File error","File is corrupt or not an impurge file");
    return;
  }

  string version_str = "imp_";
  version_str.append(std::to_string(IMP_VERSION));
  if (lines[0]!=version_str) {
    w_message(window,"Version error","File has wrong version");
    return;
  }

  kb_delete_rows();

  imf_set_toplevel_path(lines[1]);
  imf_action = std::stoi(lines[2]);
  imf_uns_action = std::stoi(lines[3]);
  kb_new_prompt_is_continuous = std::stoi(lines[4]);

  int i=5;
  while (i<lines.size()) {
    keybind this_kb;
    this_kb.key = std::stoi(lines[i++]);
    this_kb.key_str = gdk_keyval_name(this_kb.key);
    this_kb.dest_path = lines[i++];
    kb_pushback(this_kb);
  }

  gtk_widget_destroy(dialog);
}

void ses_edit_prompt_source_button(GtkWidget *w) {
  string filename = w_select_folder_popup(gtk_widget_get_toplevel(w), "Select source folder");
  if (filename!="") {
    imf_set_toplevel_path(filename);
    GtkWidget *label = gtk_bin_get_child(GTK_BIN(w));
    gtk_label_set_text(GTK_LABEL(label), imf_toplevel_path.c_str());
  }
}

void ses_edit_prompt_unsupported_changed(GtkWidget *w, gpointer user_data) {
  imf_uns_action = gtk_combo_box_get_active(GTK_COMBO_BOX(w));
}

void ses_edit_prompt_action_changed(GtkWidget *w) {
  imf_action = gtk_combo_box_get_active(GTK_COMBO_BOX(w));
}

void ses_edit_prompt_response(GtkWidget *w, gint response) {
  gtk_widget_destroy(w);
}

void ses_edit_prompt(GtkWidget *w, gpointer user_data) {
  string title = *(string*) user_data;
  GtkWidget *dialog, *content_area;
  GtkDialogFlags flags = GTK_DIALOG_MODAL;
  dialog = gtk_dialog_new_with_buttons (title.c_str(),
                                         GTK_WINDOW(window),
                                         flags,
                                         "OK",
                                         GTK_RESPONSE_ACCEPT,
                                         NULL);
  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

  GtkWidget *source_label = gtk_label_new("Source: ");
  string source_label_text = imf_toplevel_path;
  if (imf_toplevel_path=="") {
    source_label_text = W_SOURCEFOLDER_FALLBACK;
  }
  GtkWidget *source_button = gtk_button_new_with_label(source_label_text.c_str());
  gtk_label_set_ellipsize(GTK_LABEL(gtk_bin_get_child(GTK_BIN(source_button))), PANGO_ELLIPSIZE_START);
  GtkWidget *source_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_tooltip_text(source_box, "Directory to get the images from (recursive)");
  gtk_box_pack_start(GTK_BOX(source_box), source_label, false, false, 0);
  gtk_box_pack_start(GTK_BOX(source_box), source_button, true, true, 0);

  GtkWidget *action_label = gtk_label_new("Action: ");
  GtkWidget *action_combo = gtk_combo_box_text_new();
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(action_combo), "Move");
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(action_combo), "Copy");
  gtk_combo_box_set_active(GTK_COMBO_BOX(action_combo), imf_action);
  GtkWidget *action_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_tooltip_text(action_box, "What to do with the images");
  gtk_box_pack_start(GTK_BOX(action_box), action_label, false, false, 0);
  gtk_box_pack_start(GTK_BOX(action_box), action_combo, true, true, 0);

  GtkWidget *unsupported_label = gtk_label_new("Unsupported files: ");
  GtkWidget *unsupported_combo = gtk_combo_box_text_new();
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(unsupported_combo), "Ask");
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(unsupported_combo), "Skip");
  gtk_combo_box_set_active(GTK_COMBO_BOX(unsupported_combo), imf_uns_action);
  GtkWidget *unsupported_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_tooltip_text(unsupported_box, "What to do with unsupported/miscellaneous files");
  gtk_box_pack_start(GTK_BOX(unsupported_box), unsupported_label, false, false, 0);
  gtk_box_pack_start(GTK_BOX(unsupported_box), unsupported_combo, true, true, 0);

  gtk_widget_set_vexpand(GTK_WIDGET(source_box), false);
  gtk_widget_set_margin_bottom(source_box, 2);
  gtk_widget_set_vexpand(GTK_WIDGET(action_combo), false);
  gtk_widget_set_margin_bottom(action_combo, 2);
  gtk_widget_set_vexpand(GTK_WIDGET(unsupported_box), false);
  gtk_widget_set_margin_bottom(unsupported_box, 2);

  gtk_widget_set_margin_top(box, 4);
  gtk_widget_set_margin_start(box, 4);
  gtk_widget_set_margin_end(box, 4);
  gtk_widget_set_margin_bottom(box, 4);

  gtk_box_pack_start(GTK_BOX(box), source_box, false, false, 0);
  gtk_box_pack_start(GTK_BOX(box), action_box, false, false, 0);
  gtk_box_pack_start(GTK_BOX(box), unsupported_box, false, false, 0);

  g_signal_connect(G_OBJECT(source_button), "clicked", G_CALLBACK(ses_edit_prompt_source_button), NULL);
  g_signal_connect(G_OBJECT(action_combo), "changed", G_CALLBACK(ses_edit_prompt_action_changed), NULL);  
  g_signal_connect(G_OBJECT(unsupported_combo), "changed", G_CALLBACK(ses_edit_prompt_unsupported_changed), NULL);

  gtk_container_add (GTK_CONTAINER (content_area), box);
  g_signal_connect(dialog, "response", G_CALLBACK (ses_edit_prompt_response), NULL);

  gtk_widget_show_all (dialog);
}

void kb_delete_rows() {
  GtkWidget *kb_parent = gtk_widget_get_parent(kb_listbox);
  gtk_widget_destroy(kb_listbox);
  kb_listbox = gtk_list_box_new();
  gtk_container_add(GTK_CONTAINER(kb_parent), kb_listbox);
  gtk_widget_show_all(kb_listbox);
  kb_row_max_width = -1;
}

void kb_update_row_widths() {
  GList *children = gtk_container_get_children(GTK_CONTAINER(kb_listbox));
  if (g_list_length(children)==0) {
    return;
  }
  while (children!=NULL) {
    GtkWidget *this_row = GTK_WIDGET(children->data);
    GtkWidget *this_box = gtk_bin_get_child(GTK_BIN(this_row));
    GList *box_children = gtk_container_get_children(GTK_CONTAINER(this_box));
    GtkWidget *key_button = GTK_WIDGET(box_children->data);
    g_list_free(box_children);
    gtk_widget_set_size_request(key_button, kb_row_max_width, -1);
    children=children->next;
  }
  g_list_free(children);
}

void kb_check_row_widths() {
  GList *children = gtk_container_get_children(GTK_CONTAINER(kb_listbox));
  if (g_list_length(children)==0) {
    return;
  }
  int max_width = -1;
  while (children!=NULL) {
    GtkWidget *this_row = GTK_WIDGET(children->data);
    GtkWidget *this_box = gtk_bin_get_child(GTK_BIN(this_row));
    GList *box_children = gtk_container_get_children(GTK_CONTAINER(this_box));
    GtkWidget *key_button = GTK_WIDGET(box_children->data);
    g_list_free(box_children);
    gtk_widget_set_size_request(key_button, -1, -1);
    int width_request;
    gtk_widget_get_preferred_width(key_button, &width_request, NULL);
    if (width_request>max_width) {
      max_width = width_request;
    }
    children=children->next;
  }
  g_list_free(children);
  kb_row_max_width = max_width;
  kb_update_row_widths();
}

bool kb_presskey_prompt_keypress(GtkWidget *w) {
  GdkEventKey e = gtk_get_current_event()->key;
  kb_presskey_prompt_key = e.keyval;
  GtkWidget *dialog = GTK_WIDGET(gtk_widget_get_toplevel(w));
  gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
  return true;
}

int kb_presskey_prompt() {
  GtkWidget *dialog, *content_area;
  GtkDialogFlags flags = GTK_DIALOG_MODAL;
  dialog = gtk_dialog_new_with_buttons ("Press a key",
                                         GTK_WINDOW(window),
                                         flags,
                                         "Cancel",
                                         GTK_RESPONSE_CANCEL,
                                         NULL);
  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

  GtkWidget *event_box = gtk_event_box_new();
  GtkWidget *key_button = gtk_button_new_with_label("Press a key...");
  gtk_button_set_relief(GTK_BUTTON(key_button), GTK_RELIEF_NONE);
  gtk_widget_set_margin_top(key_button, 8);
  gtk_widget_set_margin_start(key_button, 8);
  gtk_widget_set_margin_end(key_button, 8);
  gtk_widget_set_margin_bottom(key_button, 8); 

  gtk_widget_add_events(event_box, GDK_KEY_PRESS_MASK);
  g_signal_connect(G_OBJECT(event_box), "key-press-event", G_CALLBACK(kb_presskey_prompt_keypress), NULL);

  gtk_container_add(GTK_CONTAINER(event_box), key_button);
  gtk_container_add(GTK_CONTAINER(content_area), event_box);

  gtk_widget_show_all(dialog);
  gtk_widget_grab_focus(key_button);

  gint response_id = gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
  if (response_id == GTK_RESPONSE_ACCEPT) {
    return kb_presskey_prompt_key;
  }
  return -1;
}

void kb_row_key_button(GtkWidget *w, gpointer user_data) {
  keybind *this_kb = (keybind*) user_data;
  this_kb->key = kb_presskey_prompt();
  GtkWidget *key_label = gtk_bin_get_child(GTK_BIN(w));

  if (this_kb->key != -1) {
    this_kb->key_str = gdk_keyval_name(this_kb->key);
    gtk_label_set_text(GTK_LABEL(key_label), this_kb->key_str.c_str());
  } else {
    this_kb->key_str = "";
    gtk_label_set_text(GTK_LABEL(key_label), W_KEYBIND_FALLBACK.c_str());
  }
}

void kb_row_dest_button(GtkWidget *w, gpointer user_data) {
  keybind &this_kb = *(keybind*) user_data;
  string filename = w_select_folder_popup(window, "Select key->dest folder");
  if (filename!="") {
    GtkWidget *label = gtk_bin_get_child(GTK_BIN(w));
    gtk_label_set_text(GTK_LABEL(label),filename.c_str());
    this_kb.dest_path = filename;
  }
}

void kb_row_close_button(GtkWidget *w, gpointer user_data) {
  keybind* this_kb = (keybind*) user_data;
  if (this_kb==keybinds_last) {
    keybinds_last = this_kb->prev;
    keybinds_last->next = NULL;
    if (keybinds_last==keybinds_first) {
      keybinds_last = NULL;
    }
  } else if (this_kb==keybinds_first) {
    if (this_kb->next!=NULL) {
      keybinds_first = this_kb->next;
      keybinds_first->prev = NULL;
    } else {
      keybinds_first = NULL;
    }
  } else {
    this_kb->prev->next = this_kb->next;
    this_kb->next->prev = this_kb->prev;
  }
  free(this_kb);
  GtkWidget *row = gtk_widget_get_parent(gtk_widget_get_parent(w));
  gtk_widget_destroy(row);
  kb_check_row_widths();
}

void kb_pushback(keybind &kb) {
  keybind *this_kb = new keybind;

  this_kb->key = kb.key;
  this_kb->key_str = kb.key_str;
  this_kb->dest_path = kb.dest_path;
  
  if (keybinds_first == NULL) {
    this_kb->prev = NULL;
    keybinds_first = this_kb;
  } else if (keybinds_last==NULL) {
    keybinds_last = this_kb;
    this_kb->prev = keybinds_first;
    keybinds_first->next = this_kb;
  } else {
    this_kb->prev = keybinds_last;
    keybinds_last->next = this_kb;
    keybinds_last = this_kb;
  }

  // add row
  GtkWidget *row = gtk_list_box_row_new();
  GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *key_button = gtk_button_new_with_label(this_kb->key_str.c_str());
  gtk_button_set_relief(GTK_BUTTON(key_button), GTK_RELIEF_NONE);
  GtkWidget *dest_button = gtk_button_new_with_label(this_kb->dest_path.c_str());
  gtk_button_set_relief(GTK_BUTTON(dest_button), GTK_RELIEF_NONE);
  GtkWidget *dest_label = gtk_bin_get_child(GTK_BIN(dest_button));
  gtk_label_set_ellipsize(GTK_LABEL(dest_label), PANGO_ELLIPSIZE_START);
  GtkWidget *close_button = gtk_button_new_from_icon_name("gtk-close", GTK_ICON_SIZE_MENU);
  gtk_button_set_relief(GTK_BUTTON(close_button), GTK_RELIEF_NONE);

  gtk_box_pack_start(GTK_BOX(row_box), key_button, false, false, 0);
  gtk_box_pack_start(GTK_BOX(row_box), dest_button, true, true, 0);
  gtk_box_pack_end(GTK_BOX(row_box), close_button, false, false, 0);

  gtk_container_add(GTK_CONTAINER(row), row_box);
  gtk_container_add(GTK_CONTAINER(kb_listbox), row);

  g_signal_connect(G_OBJECT(key_button), "clicked", G_CALLBACK(kb_row_key_button), this_kb);
  g_signal_connect(G_OBJECT(dest_button), "clicked", G_CALLBACK(kb_row_dest_button), this_kb);
  g_signal_connect(G_OBJECT(close_button), "clicked", G_CALLBACK(kb_row_close_button), this_kb);

  gtk_widget_show_all(row);

  int width_request;
  gtk_widget_get_preferred_width(key_button, &width_request, NULL);

  gtk_widget_hide(row);

  if (width_request>kb_row_max_width) {
    kb_row_max_width=width_request;
    kb_update_row_widths();
  } else {
    gtk_widget_set_size_request(key_button, kb_row_max_width, -1);
  }

  gtk_widget_show_all(row);

  this_kb->row = row;
}

void kb_new_prompt_key_button(GtkWidget *w) {
  kb_new_prompt_keybind.key = kb_presskey_prompt();
  kb_new_prompt_keybind.key_str = gdk_keyval_name(kb_new_prompt_keybind.key);
  GtkWidget *key_label = gtk_bin_get_child(GTK_BIN(w));
  if (kb_new_prompt_keybind.key != -1) {
    gtk_label_set_text(GTK_LABEL(key_label), kb_new_prompt_keybind.key_str.c_str());
  } else {
    gtk_label_set_text(GTK_LABEL(key_label), W_KEYBIND_FALLBACK.c_str());
  }
}

void kb_new_prompt_dest_button(GtkWidget *w) {
  string filename = w_select_folder_popup(window, "Select key->dest folder");
  if (filename!="") {
    GtkWidget *label = gtk_bin_get_child(GTK_BIN(w));
    gtk_label_set_text(GTK_LABEL(label),filename.c_str());
    kb_new_prompt_keybind.dest_path = filename;
  }
}

void kb_new_prompt_response(GtkWidget *w, gint response_id, gpointer user_data) {
  keybind *this_kb = keybinds_first;
  while (this_kb!=NULL) {
    if (kb_new_prompt_keybind.key == this_kb->key) {
      w_message(gtk_widget_get_toplevel(w), "New Keybind", "This key is already in use.");
      return;
    }
    this_kb = this_kb->next;
  }

  if (response_id!=GTK_RESPONSE_ACCEPT) {
    kb_new_prompt_is_open = false;
    gtk_widget_destroy(w);
    return;
  } else if (kb_new_prompt_keybind.key==-1) {
    w_message(gtk_widget_get_toplevel(w), "New Keybind", "Set the key");
    return;
  } else if (kb_new_prompt_keybind.dest_path=="") {
    w_message(gtk_widget_get_toplevel(w), "New Keybind", "Set the destination");
    return;
  }

  kb_pushback(kb_new_prompt_keybind);

  kb_new_prompt_is_continuous = bool(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(user_data)));
  kb_new_prompt_is_open = false;
  gtk_widget_destroy(w);

  if (kb_new_prompt_is_continuous) {
    kb_new_prompt();
  }
}

void kb_new_prompt() {
  kb_new_prompt_is_open = true;
  kb_new_prompt_keybind.key = -1;
  kb_new_prompt_keybind.key_str = "";
  kb_new_prompt_keybind.dest_path = ""; 

  GtkWidget *dialog, *content_area;
  GtkDialogFlags flags = GTK_DIALOG_DESTROY_WITH_PARENT;
  dialog = gtk_dialog_new_with_buttons ("New Keybind",
                                         GTK_WINDOW(window),
                                         flags,
                                         "Cancel",
                                         GTK_RESPONSE_CANCEL,
                                         "OK",
                                         GTK_RESPONSE_ACCEPT,
                                         NULL);
  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

  GtkWidget *key_label = gtk_label_new("Key: ");
  GtkWidget *key_button = gtk_button_new_with_label(W_KEYBIND_FALLBACK.c_str());
  GtkWidget *key_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start(GTK_BOX(key_box), key_label, false, false, 0);
  gtk_box_pack_end(GTK_BOX(key_box), key_button, true, true, 0);
  gtk_widget_set_vexpand(GTK_WIDGET(key_button), false);

  GtkWidget *dest_label = gtk_label_new("To:  ");
  GtkWidget *dest_button = gtk_button_new_with_label(W_DESTFOLDER_FALLBACK.c_str());

  GtkWidget *dest_button_label = gtk_bin_get_child(GTK_BIN(dest_button));
  gtk_label_set_ellipsize(GTK_LABEL(dest_button_label), PANGO_ELLIPSIZE_START);

  GtkWidget *dest_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start(GTK_BOX(dest_box), dest_label, false, false, 0);
  gtk_box_pack_end(GTK_BOX(dest_box), dest_button, true, true, 0);
  gtk_widget_set_vexpand(GTK_WIDGET(dest_button), false);

  GtkWidget *kb_prompt_is_continuous = gtk_check_button_new_with_label("Keep going");
  if (kb_new_prompt_is_continuous) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(kb_prompt_is_continuous), true);
  }

  gtk_box_pack_start(GTK_BOX(box), key_box, false, false, 0);
  gtk_widget_set_tooltip_text(key_box, "Key to press");
  gtk_box_pack_start(GTK_BOX(box), dest_box, false, false, 0);
  gtk_widget_set_tooltip_text(dest_box, "Where to move the image after pressing the key");
  gtk_box_pack_start(GTK_BOX(box), kb_prompt_is_continuous, false, false, 0);
  gtk_widget_set_tooltip_text(kb_prompt_is_continuous, "Keep making new keybinds");

  gtk_widget_set_margin_bottom(key_box, 2);
  gtk_widget_set_margin_top(box, 4);
  gtk_widget_set_margin_start(box, 4);
  gtk_widget_set_margin_end(box, 4);
  gtk_widget_set_margin_bottom(kb_prompt_is_continuous, 4);

  g_signal_connect(G_OBJECT(key_button), "clicked", G_CALLBACK(kb_new_prompt_key_button), NULL);
  g_signal_connect(G_OBJECT(dest_button), "clicked", G_CALLBACK(kb_new_prompt_dest_button), NULL);

  gtk_container_add (GTK_CONTAINER (content_area), box);
  g_signal_connect (dialog, "response", G_CALLBACK (kb_new_prompt_response), kb_prompt_is_continuous);
  gtk_widget_show_all (dialog);

  gtk_widget_grab_focus(key_button);
  g_signal_emit_by_name(G_OBJECT(key_button), "clicked");
}

void imf_setup() {
  imf_paths.clear();
  imf_paths.shrink_to_fit();
  std::filesystem::current_path(imf_toplevel_path);
  imf_total_images = 0;
  for (const std::filesystem::directory_entry& dir_entry : std::filesystem::recursive_directory_iterator(imf_toplevel_path)) {
    if (!dir_entry.is_directory()) {
      imf_paths.push_back(dir_entry.path());
      imf_total_images++;
    }
  }
  imf_image_iterator = 0;
}

bool imf_imagecontent_draw(GtkWidget *w, cairo_t *cr){
  double p_width = gtk_widget_get_allocated_width(image_content);
  double p_height = gtk_widget_get_allocated_height(image_content);
  double scale = ((p_height/imf_current_height)>(p_width/imf_current_width)) ? (p_width/imf_current_width) : (p_height/imf_current_height);
  double x2 = (p_width/2)-((imf_current_width*scale)/2);
  double y2 = (p_height/2)-((imf_current_height*scale)/2);
  cairo_scale(cr,scale,scale);
  gdk_cairo_set_source_pixbuf(cr, image_pixbuf, x2/scale, y2/scale);
  cairo_paint(cr);
  return true;
}

void imf_set_image() {
  if (image_pixbuf!=NULL) {
    g_object_unref(image_pixbuf);
  }
  if (imf_image_iterator >= imf_total_images) {
    image_pixbuf = gdk_pixbuf_copy(image_pixbuf_fallback);
    imf_current_width = gdk_pixbuf_get_width(image_pixbuf);
    imf_current_height = gdk_pixbuf_get_height(image_pixbuf);
    string header_text = "Finished (0/0)";
    gtk_label_set_text(GTK_LABEL(image_header), header_text.c_str());
    gtk_widget_queue_draw(image_content);
    return;
  }
  image_pixbuf = gdk_pixbuf_new_from_file(imf_paths[imf_image_iterator].u8string().c_str(), NULL);
  if (image_pixbuf==NULL) {
    if (imf_uns_action==IMF_UNS_SKIP) {
      imf_image_iterator++;
      imf_set_image();
    } else {
      image_pixbuf = gdk_pixbuf_copy(image_pixbuf_generic);
    }
  }
  imf_current_width = gdk_pixbuf_get_width(image_pixbuf);
  imf_current_height = gdk_pixbuf_get_height(image_pixbuf);
  string header_text = imf_paths[imf_image_iterator].filename().u8string();
  header_text.append(" (");
  header_text.append(std::to_string(imf_image_iterator+1));
  header_text.append("/");
  header_text.append(std::to_string(imf_total_images));
  header_text.append(")");
  gtk_label_set_text(GTK_LABEL(image_header), header_text.c_str());
  gtk_widget_queue_draw(image_content);
}

void imf_set_toplevel_path(string path) {
  imf_toplevel_path = path;
  if (path!="") {
    gtk_widget_hide(start_ses_button);
    gtk_widget_show_all(image_header);
    gtk_widget_show_all(focus_button_event_box);
    imf_setup();
    imf_set_image();
  } else {
    gtk_widget_show_all(start_ses_button);
    gtk_widget_hide(focus_button_event_box);
    gtk_widget_hide(image_header);
  }
}


bool ui_focus_keypress(GtkWidget *w) {
  GdkEventKey e = gtk_get_current_event()->key;
  keybind *this_kb = keybinds_first;
  if (imf_image_iterator>=imf_total_images) {
    return true;
  }
  while (this_kb!=NULL) {
    if (e.keyval == this_kb->key) {
      string new_path = this_kb->dest_path;
      new_path.append("/");
      new_path.append(imf_paths[imf_image_iterator].filename());
      const auto copy_options = std::filesystem::copy_options::overwrite_existing;
      try {
        std::filesystem::copy(imf_paths[imf_image_iterator], new_path, copy_options);
      } catch (std::filesystem::filesystem_error const& ex1)  {
        w_message(window, "Copy error", ex1.what());
      }
      if (imf_action == IMF_MOVE) {
        try {
          std::filesystem::remove(imf_paths[imf_image_iterator]);
        } catch (std::filesystem::filesystem_error const& ex2)  {
          w_message(window, "Remove error", ex2.what());
        }
      }
      imf_image_iterator++;
      imf_set_image();
      return true;
    }
    this_kb = this_kb->next;
  }
  return true;
}

void ui_get_focus(GtkWidget *w) {
  GtkWidget *label = gtk_bin_get_child(GTK_BIN(w));
  gtk_label_set_text(GTK_LABEL(label), "Press a key...");
}

void ui_lose_focus(GtkWidget *w) {
  GtkWidget *label = gtk_bin_get_child(GTK_BIN(w));
  gtk_label_set_text(GTK_LABEL(label), "<click here to get focus>");
}

void ui_start_ses_button(GtkWidget *w) {
  ses_edit_prompt(w, &W_SESEDIT_NEW_FALLBACK);
}

void imp_window() {
  window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "impurge");
  gtk_window_set_icon( GTK_WINDOW(window), WINDOW_ICON);
  gtk_window_set_resizable(GTK_WINDOW(window), true);
  gtk_window_set_default_size(GTK_WINDOW(window), 240, 360);

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add(GTK_CONTAINER(window), box);

  GtkWidget *toolbar = gtk_menu_bar_new();
  GtkWidget *file_menu = gtk_menu_item_new_with_label("Session");
  GtkWidget *file_menu_edit_item = gtk_menu_item_new_with_label("Edit");
  gtk_widget_set_tooltip_text(file_menu_edit_item, "Change the source folder or action");
  GtkWidget *file_menu_open_item = gtk_menu_item_new_with_label("Open");
  gtk_widget_set_tooltip_text(file_menu_open_item, "Load a session from a file");
  GtkWidget *file_menu_save_item = gtk_menu_item_new_with_label("Save");
  gtk_widget_set_tooltip_text(file_menu_save_item, "Save a session to a file");
  GtkWidget *submenu = gtk_menu_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(submenu), file_menu_edit_item);
  GtkWidget *file_menu_separator = gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(submenu), file_menu_separator);
  gtk_menu_shell_append(GTK_MENU_SHELL(submenu), file_menu_open_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(submenu), file_menu_save_item);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_menu), submenu);
  gtk_menu_shell_append(GTK_MENU_SHELL(toolbar), file_menu);
  g_signal_connect(G_OBJECT(file_menu_edit_item), "activate", G_CALLBACK(ses_edit_prompt), &W_SESEDIT_FALLBACK);
  g_signal_connect(G_OBJECT(file_menu_open_item), "activate", G_CALLBACK(ses_open_session), NULL);
  g_signal_connect(G_OBJECT(file_menu_save_item), "activate", G_CALLBACK(ses_save_session), NULL);
  gtk_box_pack_start(GTK_BOX(box), toolbar, false, false, 0);

  GtkWidget *image_paned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
  image_paned1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *image_paned2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_paned_pack1(GTK_PANED(image_paned), image_paned1, true, false);
  gtk_paned_pack2(GTK_PANED(image_paned), image_paned2, false, false);

  image_header = gtk_label_new("");
  gtk_label_set_ellipsize(GTK_LABEL(image_header), PANGO_ELLIPSIZE_START);
  gtk_widget_set_margin_start(image_header, 4);
  gtk_widget_set_margin_end(image_header, 4);  
  gtk_box_pack_start(GTK_BOX(image_paned1), image_header, false, false, 0);
  image_content = gtk_drawing_area_new();
  g_signal_connect (G_OBJECT(image_content), "draw", G_CALLBACK (imf_imagecontent_draw), NULL);
  gtk_widget_set_size_request(image_content, 32, 32);
  gtk_widget_set_margin_top(image_content, 4);
  gtk_widget_set_margin_start(image_content, 4);
  gtk_widget_set_margin_end(image_content, 4);
  gtk_widget_set_margin_bottom(image_content, 4);
  gtk_box_pack_start(GTK_BOX(image_paned1), image_content, true, true, 0);
  gtk_widget_set_vexpand(image_content, true);
  focus_button_event_box = gtk_event_box_new();
  GtkWidget *focus_button = gtk_button_new_with_label("<click here to get focus>");
  gtk_button_set_relief(GTK_BUTTON(focus_button), GTK_RELIEF_NONE);
  gtk_widget_set_margin_start(focus_button, 8);
  gtk_widget_set_margin_end(focus_button, 8);  
  g_signal_connect(G_OBJECT(focus_button), "focus-in-event", G_CALLBACK(ui_get_focus), NULL);
  g_signal_connect(G_OBJECT(focus_button), "focus-out-event", G_CALLBACK(ui_lose_focus), NULL);
  gtk_container_add(GTK_CONTAINER(focus_button_event_box), focus_button);
  gtk_widget_add_events(focus_button_event_box, GDK_KEY_PRESS_MASK);
  g_signal_connect(G_OBJECT(focus_button_event_box), "key-press-event", G_CALLBACK(ui_focus_keypress), NULL);

  gtk_box_pack_start(GTK_BOX(image_paned2), focus_button_event_box, false, false, 0);

  start_ses_button = gtk_button_new_with_label("Click to start...");
  g_signal_connect(G_OBJECT(start_ses_button), "clicked", G_CALLBACK(ui_start_ses_button), NULL);
  gtk_widget_set_vexpand(start_ses_button, false);
  gtk_box_pack_start(GTK_BOX(image_paned2), start_ses_button, false, false, 0);
  gtk_button_set_relief(GTK_BUTTON(start_ses_button), GTK_RELIEF_NONE);

  GtkWidget *keybinds_scrolled = gtk_scrolled_window_new(NULL, NULL);
  gtk_widget_set_margin_top(keybinds_scrolled, 4);
  gtk_widget_set_margin_bottom(keybinds_scrolled, 8);
  gtk_widget_set_margin_start(keybinds_scrolled, 4);
  gtk_widget_set_margin_end(keybinds_scrolled, 4);
  kb_listbox = gtk_list_box_new();
  gtk_container_add(GTK_CONTAINER(keybinds_scrolled), kb_listbox);
  gtk_box_pack_start(GTK_BOX(image_paned2), keybinds_scrolled, true, true, 0);

  gtk_paned_set_position(GTK_PANED(image_paned), 120);

  GtkWidget *new_keybind_button = gtk_button_new_with_label("+");
  gtk_widget_set_tooltip_text(new_keybind_button, "Make a new keybind");
  gtk_widget_set_margin_bottom(new_keybind_button, 4);
  gtk_widget_set_margin_start(new_keybind_button, 4);
  gtk_widget_set_margin_end(new_keybind_button, 4);
  g_signal_connect(G_OBJECT(new_keybind_button), "clicked", G_CALLBACK(kb_new_prompt), NULL);
  gtk_box_pack_start(GTK_BOX(image_paned2), new_keybind_button, false, false, 0);

  gtk_box_pack_start(GTK_BOX(box), image_paned, true, true, 0);

  gtk_widget_show_all(window);
  imf_set_toplevel_path("");
}

static void activate (GtkApplication* app, gpointer user_data) // bootstrap
{
  WORKING_PATH = std::filesystem::read_symlink("/proc/self/exe").parent_path().u8string();
  string path = WORKING_PATH;
  path.append("/icon.png");
  WINDOW_ICON = gdk_pixbuf_new_from_file(path.c_str(), NULL);
  image_pixbuf_fallback = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "gtk-missing-image", 128, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
  image_pixbuf_generic = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "unknown", 128, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
  image_pixbuf = gdk_pixbuf_copy(image_pixbuf_fallback);
  imf_current_width = gdk_pixbuf_get_width(image_pixbuf);
  imf_current_height = gdk_pixbuf_get_height(image_pixbuf);
  imp_window();
}

int main (int argc, char **argv) {
  int status;
  srand (time(NULL));

  app = gtk_application_new ("org.demonsuite.imp", G_APPLICATION_NON_UNIQUE);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return status;
}