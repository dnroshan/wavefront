/* wf-window.c
 *
 * Copyright 2025 Dilnavas Roshan
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include "wf-window.h"
#include "wf-player.h"
#include "wf-waveform.h"
#include "wf-seek-bar.h"

struct _WfWindow
{
    AdwApplicationWindow parent_instance;

    WfPlayer *player;
    WfWaveform *waveform;

    /* Template widgets */
    GtkWidget *play_button;
    WfSeekBar *seek_bar;
};

static void dispose             (GObject *object);
static void action_open_cb      (GSimpleAction *action,
                                 GVariant      *parameters,
                                 gpointer       user_data);
static void play_button_cb      (GtkButton *button,
                                 gpointer   user_data);
static void position_changed_cb (WfWindow *self,
                                 guint64 pos,
                                 gpointer user_data);
static void duration_changed_cb (WfWindow *self,
                                 guint64 duration,
                                 gpointer user_data);
static void seeked_cb           (WfWindow *self,
                                 guint64 pos,
                                 gpointer user_data);

static GActionEntry window_actions[] =
{
    {"open", action_open_cb}
};

G_DEFINE_FINAL_TYPE (WfWindow, wf_window, ADW_TYPE_APPLICATION_WINDOW)

static void
wf_window_class_init (WfWindowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = dispose;

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/cc/placid/Wavefront/ui/wf-window.ui");

    gtk_widget_class_bind_template_child (widget_class, WfWindow, play_button);
    gtk_widget_class_bind_template_child (widget_class, WfWindow, seek_bar);
}

static void
wf_window_init (WfWindow *self)
{
    g_type_ensure (WF_TYPE_SEEK_BAR);

    gtk_widget_init_template (GTK_WIDGET (self));

    g_action_map_add_action_entries (G_ACTION_MAP (self), window_actions,
                                     G_N_ELEMENTS (window_actions), self);

    self->player = wf_player_new ();
    g_signal_connect_swapped (self->player, "position-changed",
                              G_CALLBACK (position_changed_cb), self);
    g_signal_connect_swapped (self->player, "duration-changed",
                              G_CALLBACK (duration_changed_cb), self);
    g_signal_connect (self->play_button, "clicked", G_CALLBACK (play_button_cb), self);

    self->waveform = wf_waveform_new ();
    g_object_bind_property (self->waveform, "peaks", self->seek_bar, "peaks", G_BINDING_DEFAULT);
    g_signal_connect_swapped (self->seek_bar, "seeked", G_CALLBACK (seeked_cb), self);
}

static void
dispose (GObject *object)
{
    WfWindow *window = WF_WINDOW (object);

    g_clear_object (&window->player);
    g_clear_object (&window->waveform);
    G_OBJECT_CLASS (wf_window_parent_class)->dispose (object);
}

static void
file_opened_async_cb (GObject      *source,
                      GAsyncResult *result,
                      gpointer      user_data)
{
    GError *error = NULL;
    WfWindow *window = user_data;
    GFile *file;
    gchar *uri;

    file = gtk_file_dialog_open_finish (GTK_FILE_DIALOG (source), result, &error);
    if (!file) {
        g_printerr ("Error: %s\n", error->message);
        g_free (error);
        return;
    }

    uri = g_file_get_uri (file);
    wf_waveform_set_file (window->waveform, uri);
    wf_player_set_file (window->player, uri);
}

static void
action_open_cb (GSimpleAction *action,
                GVariant      *parameters,
                gpointer       user_data)
{
    GtkFileDialog *file_dialog;

    file_dialog = gtk_file_dialog_new ();
    gtk_file_dialog_open (file_dialog, GTK_WINDOW (user_data), NULL,
                          file_opened_async_cb, user_data);
    g_object_unref (file_dialog);
}

static void
play_button_cb (GtkButton *button,
                gpointer   user_data)
{
    WfWindow *window = WF_WINDOW (user_data);

    wf_player_play (window->player);
}

static void
position_changed_cb (WfWindow *self, guint64 pos, gpointer user_data)
{
    wf_seek_bar_set_position (self->seek_bar, pos);
}

static void
duration_changed_cb (WfWindow *self, guint64 duration, gpointer user_data)
{
    wf_seek_bar_set_duration (self->seek_bar, duration);
}

static void
seeked_cb (WfWindow *self, guint64 pos, gpointer user_data)
{
    wf_player_set_position (self->player, pos);
}

