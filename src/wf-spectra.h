/*
 * wf-spectra.h
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

#define WF_TYPE_SPECTRA (wf_spectra_get_type ())

typedef struct
{
    guint n_bands;

    gfloat *magnitude;
    gfloat *phase;
} WfSpectra;

GType      wf_spectra_get_type   (void);
WfSpectra *wf_spectra_new        (guint n_bands);
void       wf_spectra_set_values (WfSpectra    *self,
                                  const GValue *mag_db,
                                  const GValue *phase);
void       wf_spectra_free       (WfSpectra *self);

G_END_DECLS
