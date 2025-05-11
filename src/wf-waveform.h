/*
 * wf-waveform.h
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

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define WF_TYPE_WAVEFORM (wf_waveform_get_type ())
G_DECLARE_FINAL_TYPE (WfWaveform, wf_waveform, WF, WAVEFORM, GObject)


WfWaveform *wf_waveform_new       (void);
void        wf_waveform_set_file  (WfWaveform *self,
                                     const gchar *uri);
GArray     *wf_waveform_get_peaks (WfWaveform *self);

#define WF_TYPE_PEAK_DATA (wf_peak_data_get_type ())

typedef struct
{
    gdouble right;
    gdouble left;
} WfPeakData;

GType wf_peak_data_get_type (void);

WfPeakData *wf_peak_data_copy (WfPeakData *self);
void        wf_peak_data_free (WfPeakData *self);


G_END_DECLS

