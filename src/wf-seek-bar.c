/*
 * wf-seek-bar.c
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

#include "wf-seek-bar.h"
#include "wf-waveform.h"

struct _WfSeekBar
{
    GtkWidget parent;

    guint64 duration;
    guint64 position;

    GArray *peaks;
    GArray *bars;

    gdouble bar_width;
    gdouble bar_spacing;
    gdouble cursor_x;
    gdouble drag_x;

    AdwStyleManager *style_manager;
    GdkRGBA *hover_color;
};

enum
{
    PROP_ZERO,
    PROP_PEAKS,
    PROP_POSITION,
    PROP_DURATION,
    N_PROPS
};

enum
{
    SEEKED,
    N_SIGNAL
};

static GParamSpec *properties[N_PROPS] = {NULL, };
static guint signals[N_SIGNAL] = {0};

/* GObject vfuncs */

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

/* GtkWidget vfuncs */

static void measure       (GtkWidget      *widget,
                           GtkOrientation  orientation,
                           gint            for_size,
                           gint           *minimum,
                           gint           *natural,
                           gint           *baseline,
                           gint           *natural_baseline);

static void size_allocate (GtkWidget *widget,
                           gint       width,
                           gint       height,
                           gint       baseline);

static void snapshot      (GtkWidget   *widget,
                           GtkSnapshot *snapshot);

/* event callbacks */

static void pressed_cb     (WfSeekBar *self,
                            gint       n_press,
                            gdouble    x,
                            gdouble    y,
                            gpointer   user_data);

static void released_cb    (WfSeekBar *self,
                            gint       n_press,
                            gdouble    x,
                            gdouble    y,
                            gpointer   user_data);

static void motion_cb      (WfSeekBar *self,
                            gdouble    x,
                            gdouble    y,
                            gpointer   user_data);

static void enter_cb       (WfSeekBar *self,
                            gdouble    x,
                            gdouble    y,
                            gpointer   user_data);

static void leave_cb       (WfSeekBar *self,
                            gdouble    x,
                            gdouble    y,
                            gpointer   user_data);

static void drag_begin_cb  (WfSeekBar *self,
                            gdouble    start_x,
                            gdouble    start_y,
                            gpointer   user_data);

static void drag_update_cb (WfSeekBar *self,
                            gdouble    offset_x,
                            gdouble    offset_y,
                            gpointer   user_data);

static void drag_end_cb    (WfSeekBar *self,
                            gdouble    offset_x,
                            gdouble    offset_y,
                            gpointer   user_data);

static void accent_color_notify_cb (WfSeekBar  *self,
                                    GParamSpec *pspec,
                                    gpointer    user_data);

gdouble     interpolate   (GArray *peaks,
                           guint index);
static void generate_bars (WfSeekBar *self);

G_DEFINE_FINAL_TYPE (WfSeekBar, wf_seek_bar, GTK_TYPE_WIDGET)

static void
wf_seek_bar_class_init (WfSeekBarClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->get_property = get_property;
    object_class->set_property = set_property;
    object_class->dispose = dispose;
    object_class->finalize = finalize;

    widget_class->measure = measure;
    widget_class->size_allocate = size_allocate;
    widget_class->snapshot = snapshot;

    gtk_widget_class_set_css_name (widget_class, "wfseekbar");

    properties[PROP_PEAKS] =
        g_param_spec_boxed ("peaks",
                            NULL, NULL,
                            G_TYPE_ARRAY,
                            G_PARAM_READWRITE);

    properties[PROP_DURATION] =
        g_param_spec_uint64 ("duration",
                             NULL, NULL,
                             0, G_MAXUINT64, 0,
                             G_PARAM_READWRITE);

    properties[PROP_POSITION] =
        g_param_spec_uint64 ("position",
                             NULL, NULL,
                             0, G_MAXUINT64, 0,
                             G_PARAM_READWRITE);

    signals[SEEKED] =
        g_signal_new ("seeked",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                      0,
                      NULL, NULL, NULL,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_UINT64);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
wf_seek_bar_init (WfSeekBar *self)
{
    GtkGesture *click_controller;
    GtkGesture *drag_controller;
    GtkEventController *motion_controller;

    gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);
    click_controller = gtk_gesture_click_new ();
    /* TODO: Probably I should remove these, as drag events are enough. */
    g_signal_connect_swapped (click_controller, "pressed", G_CALLBACK (pressed_cb), self);
    g_signal_connect_swapped (click_controller, "released", G_CALLBACK (released_cb), self);
    gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (click_controller));

    motion_controller = gtk_event_controller_motion_new ();
    g_signal_connect_swapped (motion_controller, "motion", G_CALLBACK (motion_cb), self);
    g_signal_connect_swapped (motion_controller, "enter", G_CALLBACK (enter_cb), self);
    g_signal_connect_swapped (motion_controller, "leave", G_CALLBACK (leave_cb), self);
    gtk_widget_add_controller (GTK_WIDGET (self), motion_controller);

    drag_controller = gtk_gesture_drag_new ();
    g_signal_connect_swapped (drag_controller, "drag-begin", G_CALLBACK (drag_begin_cb), self);
    g_signal_connect_swapped (drag_controller, "drag-update", G_CALLBACK (drag_update_cb), self);
    g_signal_connect_swapped (drag_controller, "drag-end", G_CALLBACK (drag_end_cb), self);
    gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (drag_controller));

    self->bar_width = 4.0;
    self->bar_spacing = 2.2;

    self->style_manager = adw_style_manager_get_default ();
    g_signal_connect_swapped (self->style_manager, "notify::accent-color",
                              G_CALLBACK (accent_color_notify_cb), self);
    self->hover_color = adw_style_manager_get_accent_color_rgba (self->style_manager);
}

static void
get_property (GObject    *object,
              guint       property_id,
              GValue     *value,
              GParamSpec *pspec)
{
    WfSeekBar *seek_bar = WF_SEEK_BAR (object);

    switch (property_id) {
    case PROP_PEAKS:
        g_value_set_boxed (value, seek_bar->peaks);
        break;
    case PROP_DURATION:
        g_value_set_uint (value, seek_bar->duration);
        break;
    case PROP_POSITION:
        g_value_set_uint (value, seek_bar->position);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
    WfSeekBar *seek_bar = WF_SEEK_BAR (object);

    switch (property_id) {
    case PROP_PEAKS:
        wf_seek_bar_set_peaks (seek_bar, g_value_get_boxed (value));
        break;
    case PROP_DURATION:
        wf_seek_bar_set_duration (seek_bar, g_value_get_uint (value));
        break;
    case PROP_POSITION:
        wf_seek_bar_set_position (seek_bar, g_value_get_uint (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
dispose (GObject *object)
{
    WfSeekBar *seek_bar = WF_SEEK_BAR (object);

    g_clear_pointer (&seek_bar->peaks, g_array_unref);
    g_clear_pointer (&seek_bar->bars, g_array_unref);
    g_clear_object (&seek_bar->style_manager);
    G_OBJECT_CLASS (wf_seek_bar_parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
    WfSeekBar *seek_bar = WF_SEEK_BAR (object);

    gdk_rgba_free (seek_bar->hover_color);
    G_OBJECT_CLASS (wf_seek_bar_parent_class)->finalize (object);
}

/* TODO: I need to implement the measure function. */

static void
measure (GtkWidget      *widget,
         GtkOrientation  orientation,
         gint            for_size,
         gint           *minimum,
         gint           *natural,
         gint           *baseline,
         gint           *natural_baseline)
{
}

static void
size_allocate (GtkWidget *self,
               gint       width,
               gint       height,
               gint       baseline)
{
    WfSeekBar *seek_bar = WF_SEEK_BAR (self);

    if (seek_bar->peaks)
        generate_bars (seek_bar);
}

static void
pressed_cb (WfSeekBar *self,
            gint       n_press,
            gdouble    x,
            gdouble    y,
            gpointer   user_data)
{
}


static void
released_cb (WfSeekBar *self,
             gint       n_press,
             gdouble    x,
             gdouble    y,
             gpointer   user_data)
{
}

static void
motion_cb (WfSeekBar *self,
           gdouble    x,
           gdouble    y,
           gpointer   user_data)
{
    self->cursor_x = x;
    gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
enter_cb (WfSeekBar *self,
          gdouble    x,
          gdouble    y,
          gpointer   user_data)
{
    gtk_widget_grab_focus (GTK_WIDGET (self));
}

static void
leave_cb (WfSeekBar *self,
          gdouble    x,
          gdouble    y,
          gpointer   user_data)
{
    self->cursor_x = 0;
    gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
drag_begin_cb (WfSeekBar *self,
               gdouble    start_x,
               gdouble    start_y,
               gpointer   user_data)
{
    gint width = gtk_widget_get_width (GTK_WIDGET (self));
    guint64 pos;

    self->drag_x = start_x;
    pos = (self->drag_x / (gdouble) width) * self->duration;
    g_signal_emit (self, signals[SEEKED], 0, pos);
}

static void
drag_update_cb (WfSeekBar *self,
                gdouble    offset_x,
                gdouble    offset_y,
                gpointer   user_data)
{
    gint width = gtk_widget_get_width (GTK_WIDGET (self));
    guint64 pos;

    self->drag_x += offset_x;
    pos = (self->drag_x / (gdouble) width) * self->duration;
    g_signal_emit (self, signals[SEEKED], 0, pos);
}

static void
drag_end_cb (WfSeekBar *self,
             gdouble    offset_x,
             gdouble    offset_y,
             gpointer   user_data)
{
    gint width = gtk_widget_get_width (GTK_WIDGET (self));
    guint64 pos;

    self->drag_x += offset_x;
    pos = (self->drag_x / (gdouble) width) * self->duration;
    g_signal_emit (self, signals[SEEKED], 0, pos);
}

static void
snapshot (GtkWidget   *widget,
          GtkSnapshot *snapshot)
{
    WfSeekBar *seek_bar = WF_SEEK_BAR (widget);
    graphene_rect_t bar_rect;
    GdkRGBA white = {1.0, 1.0, 1.0, 1.0};
    GdkRGBA color;
    gint width, height;
    gdouble delta;
    gdouble offset = 0.0;
    gdouble bar_height;
    gdouble pos;

    if (!seek_bar->bars)
        return;

    width = gtk_widget_get_width (widget);
    height = gtk_widget_get_height (widget);
    gtk_widget_get_color (widget, &color);

    pos = seek_bar->duration ? seek_bar->position / (gdouble) seek_bar->duration * width : 0.0;
    delta = seek_bar->bar_width + seek_bar->bar_spacing;
    gtk_snapshot_push_mask (snapshot, GSK_MASK_MODE_ALPHA);
    for (int i = 0; i < seek_bar->bars->len; i++) {
        bar_height = g_array_index (seek_bar->bars, gdouble, i);
        bar_rect = GRAPHENE_RECT_INIT (offset, (1 - bar_height) * height / 2,
                                       seek_bar->bar_width, bar_height * height);
        gtk_snapshot_append_color (snapshot, &white, &bar_rect);
        offset += delta;
    }
    gtk_snapshot_pop (snapshot);
    gtk_snapshot_append_color (snapshot, seek_bar->hover_color,
                               &GRAPHENE_RECT_INIT (0, 0, seek_bar->cursor_x, height));
    color.alpha = 0.6;
    gtk_snapshot_append_color (snapshot, &color,
                               &GRAPHENE_RECT_INIT (0, 0, pos, height));
    color.alpha = 0.3;
    gtk_snapshot_append_color (snapshot, &color,
                               &GRAPHENE_RECT_INIT (pos, 0, width - pos, height));
    gtk_snapshot_pop (snapshot);
}

static void
accent_color_notify_cb (WfSeekBar  *self,
                        GParamSpec *pspec,
                        gpointer    user_data)
{
    g_clear_pointer (&self->hover_color, gdk_rgba_free);
    self->hover_color = adw_style_manager_get_accent_color_rgba (self->style_manager);
}

WfSeekBar *
wf_seek_bar_new (void)
{
    return g_object_new (WF_TYPE_SEEK_BAR, NULL);
}

/* TODO: This is too naive. I should come up a better interpolation function. */

gdouble
interpolate (GArray *peaks, guint index)
{
    WfPeakData *current, *prev, *next;

    if (index == 0) {
        current = &g_array_index (peaks, WfPeakData, index);
        next = &g_array_index (peaks, WfPeakData, index + 1);
        return (current->left + next->left) / 2;
    } else if (index + 1 == peaks->len) {
        current = &g_array_index (peaks, WfPeakData, index);
        prev = &g_array_index (peaks, WfPeakData, index - 1);
        return (current->left + prev->left) / 2;
    }

    prev = &g_array_index (peaks, WfPeakData, index - 1);
    current = &g_array_index (peaks, WfPeakData, index);
    next = &g_array_index (peaks, WfPeakData, index + 1);

    return (prev->left + current->left + next->left) / 3;
}

/* TODO: Is this the best way? I may need to rewrite this. */

static void
generate_bars (WfSeekBar *self)
{
    gint width;
    guint n_peaks;
    guint n_bars;
    gdouble p;
    gdouble val;

    width = gtk_widget_get_width (GTK_WIDGET (self));

    n_peaks = self->peaks->len;
    n_bars = (width - self->bar_spacing) / (self->bar_width + self->bar_spacing);
    p = n_bars / (gdouble) n_peaks;

    if (self->bars)
        g_array_unref (self->bars);

    self->bars = g_array_new (FALSE, FALSE, sizeof (gdouble));
    for (int i = 0; i < n_bars; i++) {
        val = interpolate (self->peaks, (guint) (i / p));
        g_array_append_val (self->bars, val);
    }
}

void
wf_seek_bar_set_peaks (WfSeekBar *self,
                       GArray    *peaks)
{
    g_return_if_fail (WF_IS_SEEK_BAR (self));

    if (self->peaks)
        g_array_unref (self->peaks);

    self->peaks = g_array_ref (peaks);
    generate_bars (self);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PEAKS]);
    gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
wf_seek_bar_set_duration (WfSeekBar *self,
                          guint64    duration)
{
    g_return_if_fail (WF_IS_SEEK_BAR (self));

    self->duration = duration;
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DURATION]);
    gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
wf_seek_bar_set_position (WfSeekBar *self,
                          guint64    position)
{
    g_return_if_fail (WF_IS_SEEK_BAR (self));

    self->position = position;
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_POSITION]);
    gtk_widget_queue_draw (GTK_WIDGET (self));
}

