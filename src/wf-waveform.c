/*
 * wf-waveform.c
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

#include "config.h"

#include <math.h>
#include <gst/gst.h>

#include "wf-waveform.h"

struct _WfWaveform
{
    GObject parent;

    GstElement *pipeline;
    GstBus *bus;
    gint watch_id;

    GArray *peaks;
    gdouble max_right;
    gdouble max_left;
};

enum
{
    PROP_ZERO,
    PROP_PEAKS,
    N_PROPS
};

enum
{
    READY,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = {0, };
static GParamSpec *properties[N_PROPS] = {NULL, };

static void get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec);
static void set_property (GObject      *object,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *pspec);
static void dispose      (GObject *object);
static void finalize     (GObject *object);

static gboolean message_handler (GstBus     *bus,
                                 GstMessage *message,
                                 gpointer    user_data);

G_DEFINE_FINAL_TYPE (WfWaveform, wf_waveform, G_TYPE_OBJECT)

static void
wf_waveform_class_init (WfWaveformClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->get_property = get_property;
    object_class->set_property = set_property;
    object_class->dispose = dispose;
    object_class->finalize = finalize;

    properties[PROP_PEAKS] =
        g_param_spec_boxed ("peaks",
                            NULL, NULL,
                            G_TYPE_ARRAY, G_PARAM_READABLE);

    signals[READY] =
        g_signal_new ("ready",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                      0, NULL, NULL, NULL,
                      G_TYPE_NONE, 0);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
dispose (GObject *object)
{
    WfWaveform *waveform = WF_WAVEFORM (object);

    if (waveform->watch_id) {
        g_source_remove (waveform->watch_id);
        waveform->watch_id = 0;
    }

    if (waveform->pipeline) {
        gst_element_set_state (waveform->pipeline, GST_STATE_NULL);
        gst_object_unref (waveform->pipeline);
        waveform->pipeline = NULL;
    }

    g_clear_pointer (&waveform->bus, gst_object_unref);
    g_clear_pointer (&waveform->peaks, g_array_unref);

    G_OBJECT_CLASS (wf_waveform_parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
    G_OBJECT_CLASS (wf_waveform_parent_class)->finalize (object);
}

static void
get_property (GObject    *object,
              guint       property_id,
              GValue     *value,
              GParamSpec *pspec)
{
    WfWaveform *waveform = WF_WAVEFORM (object);

    switch (property_id) {
    case PROP_PEAKS:
        g_value_set_boxed (value, waveform->peaks);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
    switch (property_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
wf_waveform_init (WfWaveform *self)
{
}

static void
create_pipeline (WfWaveform *self)
{
    GstElement *fakesink, *level;

    self->pipeline = gst_parse_launch ("uridecodebin name=uridecodebin "
                                       "! audioconvert ! audio/x-raw,channels=2 "
                                       "! level name=level interval=250000000 "
                                       "! fakesink name=fakesink", NULL);
    if (!self->pipeline) {
        g_printerr ("Error: failed building pipeline\n");
        return;
    }

    fakesink = gst_bin_get_by_name (GST_BIN (self->pipeline), "fakesink");
    level = gst_bin_get_by_name (GST_BIN (self->pipeline), "level");

    g_object_set (level, "post-messages", TRUE, NULL);
    g_object_set (fakesink, "qos", FALSE, "sync", FALSE, NULL);

    self->bus = gst_pipeline_get_bus (GST_PIPELINE (self->pipeline));
    self->watch_id = gst_bus_add_watch (self->bus, message_handler, self);
}

static void
destroy_pipeline (WfWaveform *self)
{
    gst_element_set_state (self->pipeline, GST_STATE_NULL);
    g_source_remove (self->watch_id);
    gst_object_unref (self->bus);
    gst_object_unref (self->pipeline);

    self->pipeline = NULL;
    self->bus = NULL;
    self->watch_id = 0;
}

static void
noarmalize_peaks (WfWaveform *self)
{
    WfPeakData *peak_data;

    for (int i = 0; i < self->peaks->len; i++) {
        peak_data = &g_array_index (self->peaks, WfPeakData, i);
        peak_data->right /= self->max_right;
        peak_data->left /= self->max_left;
    }
}

static gboolean
message_handler (GstBus     *bus,
                 GstMessage *message,
                 gpointer    user_data)
{
    WfWaveform *waveform = WF_WAVEFORM (user_data);
    GError *error = NULL;
    gchar *debug_msg;
    const gchar *name;
    const GstStructure *structure;
    const GValue *value, *v;
    GValueArray *value_array;
    gdouble right, left;
    WfPeakData peak_pair;

    switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ELEMENT:
        structure = gst_message_get_structure (message);
        name = gst_structure_get_name (structure);
        if (!g_strcmp0 (name, "level")) {
            G_GNUC_BEGIN_IGNORE_DEPRECATIONS
            value = gst_structure_get_value (structure, "peak");
            value_array = g_value_get_boxed (value);
            v = g_value_array_get_nth (value_array, 0);
            right = g_value_get_double (v);
            v = g_value_array_get_nth (value_array, 1);
            left = g_value_get_double (v);
            G_GNUC_END_IGNORE_DEPRECATIONS

            right = pow (10, right / 20);
            left = pow (10, left / 20);

            if (waveform->max_right < right)
                waveform->max_right = right;
            if (waveform->max_left < left)
                waveform->max_left = left;

            peak_pair.right = right;
            peak_pair.left = left;
            g_array_append_val (waveform->peaks, peak_pair);
        }
        break;
    case GST_MESSAGE_EOS:
        destroy_pipeline (waveform);
        noarmalize_peaks (waveform);
        g_object_notify_by_pspec (G_OBJECT (waveform), properties[PROP_PEAKS]);
        g_signal_emit (waveform, signals[READY], 0);
        break;
    case GST_MESSAGE_ERROR:
        gst_message_parse_error (message, &error, &debug_msg);
        g_printerr ("Error: %s\n", error->message);
        g_error_free (error);
        g_free (debug_msg);
        break;
    default:
        break;
    }

    return TRUE;
}

static void
generate_peaks (WfWaveform  *self,
                const gchar *uri)
{
    GstElement *uridecode;
    gint status;

    create_pipeline (self);
    uridecode = gst_bin_get_by_name (GST_BIN (self->pipeline), "uridecodebin");
    g_object_set (uridecode, "uri", uri, NULL);

    if (self->peaks)
        g_array_unref (self->peaks);

    self->peaks = g_array_new (FALSE, FALSE, sizeof (WfPeakData));

    self->max_left = G_MINDOUBLE;
    self->max_right = G_MINDOUBLE;

    status = gst_element_set_state (self->pipeline, GST_STATE_PLAYING);
    if (status == GST_STATE_CHANGE_FAILURE) {
        g_printerr ("Error: state change failure\n");
    }
}

WfWaveform *
wf_waveform_new (void)
{
    return g_object_new (WF_TYPE_WAVEFORM, NULL);
}

void
wf_waveform_set_file (WfWaveform *self, const gchar *uri)
{
    g_return_if_fail (WF_IS_WAVEFORM (self));

    generate_peaks (self, uri);
}

GArray *
wf_waveform_get_peaks (WfWaveform *self)
{
    g_return_val_if_fail (WF_IS_WAVEFORM (self), NULL);

    return self->peaks;
}

G_DEFINE_BOXED_TYPE (WfPeakData, wf_peak_data, wf_peak_data_copy, wf_peak_data_free)

WfPeakData *
wf_peak_data_copy (WfPeakData *self)
{
    WfPeakData *copy;

    copy = g_slice_new (WfPeakData);
    copy->right = self->right;
    copy->left = self->left;
    return copy;
}

void
wf_peak_data_free (WfPeakData *self)
{
}

