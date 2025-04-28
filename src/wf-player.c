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

#include "wf-player.h"

struct _WfPlayer
{
    GObject parent;

    GstElement *playbin;
    GstBus *bus;
};

static void wf_player_dispose  (GObject *object);
static void wf_player_finalize (GObject *object);

/* Bus callbacks */

static void bus_error_cb (GstBus     *bus,
                          GstMessage *msg,
                          gpointer    user_data);

G_DEFINE_FINAL_TYPE (WfPlayer, wf_player, G_TYPE_OBJECT)

static void
wf_player_class_init (WfPlayerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = wf_player_dispose;
    object_class->finalize = wf_player_finalize;
}

static void
wf_player_init (WfPlayer *self)
{
    self->playbin = gst_element_factory_make ("playbin", "playbin");
    self->bus = gst_element_get_bus (self->playbin);
    g_signal_connect (self->bus, "message::error", G_CALLBACK (bus_error_cb), self);
    gst_bus_add_signal_watch (self->bus);
}

static void
wf_player_dispose (GObject *object)
{
    WfPlayer *player = WF_PLAYER (object);

    gst_object_unref (player->playbin);
    gst_bus_remove_signal_watch (player->bus);
    gst_object_unref (player->bus);
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
bus_error_cb (GstBus     *bus,
              GstMessage *msg,
              gpointer    user_data)
{
    GError *error = NULL;
    gchar *debug_info;

    gst_message_parse_error (msg, &error, &debug_info);
    g_printerr ("Error: %s\n", debug_info);
}

void
wf_player_set_file (WfPlayer    *self,
                    const gchar *uri)
{
    g_return_if_fail (WF_IS_PLAYER (self));

    g_object_set (self->playbin, "uri", uri, NULL);
}

void
wf_player_play (WfPlayer *self)
{
    int status;

    g_return_if_fail (WF_IS_PLAYER (self));

    status = gst_element_set_state (self->playbin, GST_STATE_PLAYING);
    if (status == GST_STATE_CHANGE_FAILURE) {
        g_printerr ("Error: state change failure\n");
    }
}

