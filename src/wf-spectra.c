/*
 * wf-spectra.c
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

#include "wf-spectra.h"

static WfSpectra *
wf_spectra_copy (WfSpectra *self)
{
    WfSpectra *copy;

    copy = wf_spectra_new (self->n_bands);
    copy->magnitude = g_memdup2 (self->magnitude, self->n_bands * sizeof (float));
    copy->phase = g_memdup2 (self->magnitude, self->n_bands * sizeof (float));
    return copy;
}

void
wf_spectra_free (WfSpectra *self)
{
    g_free (self->magnitude);
    g_free (self->phase);

    self->magnitude = NULL;
    self->phase = NULL;
}

G_DEFINE_BOXED_TYPE (WfSpectra, wf_spectra, wf_spectra_copy, wf_spectra_free)

WfSpectra *
wf_spectra_new (guint n_bands)
{
    WfSpectra *spectra;

    spectra = g_malloc (sizeof (WfSpectra));
    spectra->n_bands = n_bands;
    spectra->magnitude = g_malloc (n_bands * sizeof (float));
    spectra->phase = g_malloc (n_bands * sizeof (float));
    return spectra;
}

void
wf_spectra_set_values (WfSpectra    *self,
                       const GValue *mag_db,
                       const GValue *phase)
{
    const GValue *mag_val, *phase_val;

    for (guint i = 0; i < self->n_bands; i++) {
        mag_val = gst_value_list_get_value (mag_db, i);
        phase_val = gst_value_list_get_value (phase, i);
        self->magnitude[i] = pow (10, g_value_get_float (mag_val) / 20.0);
        self->phase[i] = g_value_get_float (phase_val);
    }
}
