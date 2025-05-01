/*
 * wf-player.c
 *
 * Copyright 2025 Dilnavas Roshan <dilnavasroshan@gmail.com>
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gst/gst.h>
#include <gst/play/play.h>

#include "wf-player.h"

struct _WfPlayer
{
    GObject parent;

    GstPlay *play;
    GstPlaySignalAdapter *signal_adaptor;
    GstBus *bus;
};

enum
{
    POSITION_CHNAGED,
    N_SIGNALS
};

static guint wf_player_signals[N_SIGNALS] = {0};

static void wf_player_dispose  (GObject *object);
static void wf_player_finalize (GObject *object);

/* Bus callbacks */

static void position_updated_cb   (WfPlayer *self,
                                   guint64   pos,
                                   gpointer  user_data);

static void state_changed_cb      (WfPlayer     *self,
                                   GstPlayState *state,
                                   gpointer      user_data);

static void end_of_stream_cb      (WfPlayer *self,
                                   gpointer  user_data);

static void error_cb              (WfPlayer     *self,
                                   GError       *error,
                                   GstStructure *details,
                                   gpointer      user_data);

static void media_info_updated_cb (WfPlayer         *self,
                                   GstPlayMediaInfo *media_info,
                                   gpointer          user_data);

G_DEFINE_FINAL_TYPE (WfPlayer, wf_player, G_TYPE_OBJECT)

static void
wf_player_class_init (WfPlayerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = wf_player_dispose;
    object_class->finalize = wf_player_finalize;

    wf_player_signals[POSITION_CHNAGED] =
        g_signal_new ("position-changed",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL, NULL,
                      G_TYPE_NONE, 1, G_TYPE_DOUBLE);
}

static void
wf_player_init (WfPlayer *self)
{
    GstElement *filter_pipeline;
    GstElement *equalizer, *spectrum;
    GstElement *play_pipeline;
    GstPad *src_pad, *sink_pad;
    GstPad *ghost_src_pad, *ghost_sink_pad;

    equalizer = gst_element_factory_make ("equalizer-10bands", "equalizer");
    spectrum = gst_element_factory_make ("spectrum", "spectrum");

    sink_pad = gst_element_get_static_pad (equalizer, "sink");
    src_pad = gst_element_get_static_pad (spectrum, "src");
    gst_pad_set_active (sink_pad, TRUE);
    gst_pad_set_active (src_pad, TRUE);

    filter_pipeline = gst_pipeline_new (NULL);
    gst_bin_add_many (GST_BIN (filter_pipeline), equalizer, spectrum, NULL);

    gst_element_link (equalizer, spectrum);

    ghost_sink_pad = gst_ghost_pad_new ("sink", sink_pad);
    ghost_src_pad = gst_ghost_pad_new ("src", src_pad);

    gst_element_add_pad (filter_pipeline, ghost_sink_pad);
    gst_element_add_pad (filter_pipeline, ghost_src_pad);

    self->play = gst_play_new (NULL);
    play_pipeline = gst_play_get_pipeline (self->play);
    g_object_set (play_pipeline, "audio-filter", filter_pipeline, NULL);
    self->signal_adaptor = gst_play_signal_adapter_new (self->play);

    g_signal_connect_swapped (self->signal_adaptor, "position-updated",
                              G_CALLBACK (position_updated_cb), self);
    g_signal_connect_swapped (self->signal_adaptor, "state-changed",
                              G_CALLBACK (state_changed_cb), self);
    g_signal_connect_swapped (self->signal_adaptor, "end-of-stream",
                              G_CALLBACK (end_of_stream_cb), self);
    g_signal_connect_swapped (self->signal_adaptor, "media-info-updated",
                              G_CALLBACK (media_info_updated_cb), self);
    g_signal_connect_swapped (self->signal_adaptor, "error",
                              G_CALLBACK (error_cb), self);
}

static void
wf_player_dispose (GObject *object)
{
    WfPlayer *player = WF_PLAYER (object);
    GstBus *bus;

    bus = gst_play_get_message_bus (player->play);
    gst_bus_set_flushing (bus, TRUE);
    gst_object_unref (bus);

    g_clear_object (&player->signal_adaptor);
    g_clear_object (&player->play);

    G_OBJECT_CLASS (wf_player_parent_class)->dispose (object);
}

static void
wf_player_finalize (GObject *object)
{
    G_OBJECT_CLASS (wf_player_parent_class)->finalize (object);
}

WfPlayer *
wf_player_new (void)
{
    return g_object_new (WF_TYPE_PLAYER, NULL);
}

static void
position_updated_cb (WfPlayer *self,
                     guint64   pos,
                     gpointer  user_data)
{
    guint64 duration;
    gdouble pos_frac;

    duration = gst_play_get_duration (self->play);
    pos_frac = pos / (gdouble) duration;
    g_signal_emit (self, wf_player_signals[POSITION_CHNAGED], 0, pos_frac);
}

static void
state_changed_cb (WfPlayer     *self,
                  GstPlayState *state,
                  gpointer      user_data)
{
}

static void
end_of_stream_cb (WfPlayer *self,
                  gpointer  user_data)
{
}

static void
error_cb (WfPlayer     *self,
          GError       *error,
          GstStructure *details,
          gpointer      user_data)
{
    g_printerr ("%s\n", error->message);
}

static void
media_info_updated_cb (WfPlayer         *self,
                       GstPlayMediaInfo *media_info,
                       gpointer          user_data)
{
}

void
wf_player_set_file (WfPlayer    *self,
                    const gchar *uri)
{
    g_return_if_fail (WF_IS_PLAYER (self));

    g_object_set (self->play, "uri", uri, NULL);
}

void
wf_player_play (WfPlayer *self)
{
    gst_play_play (self->play);
}

