/*
 * wf-seek-bar.h
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

#include <adwaita.h>

G_BEGIN_DECLS

#define WF_TYPE_SEEK_BAR (wf_seek_bar_get_type ())
G_DECLARE_FINAL_TYPE (WfSeekBar, wf_seek_bar, WF, SEEK_BAR, GtkWidget)

WfSeekBar *wf_seek_bar_new          (void);
void       wf_seek_bar_set_peaks    (WfSeekBar *self,
                                     GArray     *peaks);
void       wf_seek_bar_set_duration (WfSeekBar *self,
                                     guint64    duration);
void       wf_seek_bar_set_position (WfSeekBar *self,
                                     guint64    position);

G_END_DECLS

