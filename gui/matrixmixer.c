/* Matrix Mixer
 *
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"

#include "../src/matrixmixer.h"

#define RTK_URI MATMIX_URI
#define RTK_GUI "ui"

#define GD_W  35
#define GD_H  34
#define GD_RAD 10
#define GD_CX 17.5
#define GD_CY 17.5

#define M_OFFSET (N_INPUTS + N_OUTPUTS)
#define M_SIZE (N_INPUTS * N_OUTPUTS)

typedef struct {
	LV2UI_Write_Function write;
	LV2UI_Controller     controller;
	LV2UI_Touch*         touch;

	RobWidget* rw;
	RobWidget* matrix;

	RobTkDial* mtx_gain[M_SIZE];
	RobTkLbl*  mtx_lbl_i[N_INPUTS];
	RobTkLbl*  mtx_lbl_o[N_OUTPUTS];
	RobTkLbl*  mtx_lbl;

	cairo_surface_t*      mtx_sf[6];
	PangoFontDescription* font;

	bool disable_signals;

	const char* nfo;
} MatMixUI;

static void
create_faceplate (MatMixUI* ui)
{
	cairo_t* cr;
	float    c_bg[4];
	get_color_from_theme (1, c_bg);

#define MTX_SF(SF)                                                             \
  SF = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 2 * GD_W, 2 * GD_H);   \
  cr = cairo_create (SF);                                                      \
  cairo_scale (cr, 2.0, 2.0);                                                  \
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);                              \
  cairo_rectangle (cr, 0, 0, GD_W, GD_H);                                      \
  CairoSetSouerceRGBA (c_bg);                                                  \
  cairo_fill (cr);                                                             \
  CairoSetSouerceRGBA (c_g60);                                                 \
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);                                \
  cairo_set_line_width (cr, 1.0);

#define MTX_ARROW_H               \
  cairo_move_to (cr, 5, GD_CY);   \
  cairo_rel_line_to (cr, -5, -4); \
  cairo_rel_line_to (cr, 0, 8);   \
  cairo_close_path (cr);          \
  cairo_fill (cr);

#define MTX_ARROW_V                \
  cairo_move_to (cr, GD_CX, GD_H); \
  cairo_rel_line_to (cr, -4, -5);  \
  cairo_rel_line_to (cr, 8, 0);    \
  cairo_close_path (cr);           \
  cairo_fill (cr);

	MTX_SF (ui->mtx_sf[0]);
	MTX_ARROW_H;
	MTX_ARROW_V;

	cairo_move_to (cr, 0, GD_CY);
	cairo_line_to (cr, GD_W, GD_CY);
	cairo_stroke (cr);
	cairo_move_to (cr, GD_CX, 0);
	cairo_line_to (cr, GD_CX, GD_H);
	cairo_stroke (cr);
	cairo_destroy (cr);

	// top-row
	MTX_SF (ui->mtx_sf[1]);
	MTX_ARROW_H;
	MTX_ARROW_V;

	cairo_move_to (cr, 0, GD_CY);
	cairo_line_to (cr, GD_W, GD_CY);
	cairo_stroke (cr);
	cairo_move_to (cr, GD_CX, GD_CY);
	cairo_line_to (cr, GD_CX, GD_H);
	cairo_stroke (cr);
	cairo_destroy (cr);

	// left column
	MTX_SF (ui->mtx_sf[2]);
	MTX_ARROW_V;

	cairo_move_to (cr, 0, GD_CY);
	cairo_line_to (cr, GD_W, GD_CY);
	cairo_stroke (cr);
	cairo_move_to (cr, GD_CX, 0);
	cairo_line_to (cr, GD_CX, GD_H);
	cairo_stroke (cr);
	cairo_destroy (cr);

	// right column
	MTX_SF (ui->mtx_sf[3]);
	MTX_ARROW_H;
	MTX_ARROW_V;

	cairo_move_to (cr, 0, GD_CY);
	cairo_line_to (cr, GD_CX, GD_CY);
	cairo_stroke (cr);
	cairo_move_to (cr, GD_CX, 0);
	cairo_line_to (cr, GD_CX, GD_H);
	cairo_stroke (cr);
	cairo_destroy (cr);

	// top-left
	MTX_SF (ui->mtx_sf[4]);
	MTX_ARROW_V;

	cairo_move_to (cr, 0, GD_CY);
	cairo_line_to (cr, GD_W, GD_CY);
	cairo_stroke (cr);
	cairo_move_to (cr, GD_CX, GD_CY);
	cairo_line_to (cr, GD_CX, GD_H);
	cairo_stroke (cr);
	cairo_destroy (cr);

	// top-right
	MTX_SF (ui->mtx_sf[5]);
	MTX_ARROW_H;
	MTX_ARROW_V;

	cairo_move_to (cr, 0, GD_CY);
	cairo_line_to (cr, GD_CX, GD_CY);
	cairo_stroke (cr);
	cairo_move_to (cr, GD_CX, GD_CY);
	cairo_line_to (cr, GD_CX, GD_H);
	cairo_stroke (cr);
	cairo_destroy (cr);
}

static float
knob_pos_to_gain (float v)
{
	if (v == 0.f) {
		return 0;
	}
	return exp (((pow (fabs (v), 0.125) * 150.) - 144.) * log (2.) / 6.);
}

static float
knob_gain_to_pos (float v)
{
	if (v == 0.f) {
		return 0;
	}
	return pow ((6. * log (fabs (v)) / log (2.) + 144.) / 150., 8.);
}

static float
knob_to_db (float v)
{
	return 20.f * log10f (knob_pos_to_gain (v));
}

static void
dial_annotation_db (RobTkDial* d, cairo_t* cr, void* data)
{
	MatMixUI* ui = (MatMixUI*)(data);
	char      txt[16];
	switch (d->click_state) {
		case 1:
			snprintf (txt, 16, "\u00D8%+4.1fdB", knob_to_db (d->cur));
			break;
		default:
			snprintf (txt, 16, "%+4.1fdB", knob_to_db (d->cur));
			break;
	}

	int tw, th;
	cairo_save (cr);
	PangoLayout* pl = pango_cairo_create_layout (cr);
	pango_layout_set_font_description (pl, ui->font);
	pango_layout_set_text (pl, txt, -1);
	pango_layout_get_pixel_size (pl, &tw, &th);
	cairo_translate (cr, d->w_width / 2, d->w_height - 0);
	cairo_translate (cr, -tw / 2.0, -th);
	cairo_set_source_rgba (cr, .0, .0, .0, .5);
	rounded_rectangle (cr, -1, -1, tw + 3, th + 1, 3);
	cairo_fill (cr);
	CairoSetSouerceRGBA (c_wht);
	pango_cairo_show_layout (cr, pl);
	g_object_unref (pl);
	cairo_restore (cr);
	cairo_new_path (cr);
}

/* ****************************************************************************
 * UI callbacks
 */

static bool
cb_mtx_gain (RobWidget* w, void* handle)
{
	MatMixUI* ui = (MatMixUI*)handle;

	if (ui->disable_signals) {
		return TRUE;
	}

	unsigned int n;
	memcpy (&n, w->name, sizeof (unsigned int));

	float val = knob_pos_to_gain (robtk_dial_get_value (ui->mtx_gain[n]));
	if (robtk_dial_get_state (ui->mtx_gain[n]) == 1) {
		val *= -1;
	}
	ui->write (ui->controller, M_OFFSET + n, sizeof (float), 0, (const void*)&val);
	return TRUE;
}


static RobWidget*
robtk_dial_mouse_intercept (RobWidget* handle, RobTkBtnEvent* ev)
{
	RobTkDial* d = (RobTkDial*)GET_HANDLE (handle);
	MatMixUI* ui = (MatMixUI*)d->handle;

	if (!d->sensitive) {
		return NULL;
	}

	if (ev->button == 2) {
		/* middle-click exclusively assigns output */
		unsigned int n;
		memcpy (&n, d->rw->name, sizeof (unsigned int));

		unsigned c = n % N_OUTPUTS;
		unsigned r = n / N_OUTPUTS;
		for (uint32_t i = 0; i < N_OUTPUTS; ++i) {
			unsigned int nn = r * N_OUTPUTS + i;
			if (i == c) {
				if (d->cur == 0) {
					robtk_dial_set_value (ui->mtx_gain[nn], knob_gain_to_pos (1.f));
				} else {
					robtk_dial_set_value (ui->mtx_gain[nn], knob_gain_to_pos (0.f));
				}
			} else {
				robtk_dial_set_value (ui->mtx_gain[nn], knob_gain_to_pos (0.f));
			}
		}
		return handle;
	}
	return robtk_dial_mousedown (handle, ev);
}

/******************************************************************************
 * RobWidget
 */

static RobWidget*
toplevel (MatMixUI* ui, void* const top)
{
	ui->rw = rob_vbox_new (FALSE, 2);
	robwidget_make_toplevel (ui->rw, top);
	robwidget_toplevel_enable_scaling (ui->rw);

	ui->font = pango_font_description_from_string ("Mono 9px");

	create_faceplate (ui);

	ui->matrix = rob_table_new (/*rows*/ 2 + N_INPUTS, /*cols*/ 1 + N_OUTPUTS, FALSE);

	ui->mtx_lbl = robtk_lbl_new ("Matrix Mixer");
	rob_table_attach (ui->matrix, robtk_lbl_widget (ui->mtx_lbl),
	                  1, N_OUTPUTS + 1,
	                  0, 1,
	                  2, 2, RTK_SHRINK, RTK_SHRINK);

	/* matrix */
	for (unsigned int r = 0; r < N_INPUTS; ++r) {
		char txt[16];
		sprintf (txt, "In %d", r + 1);
		ui->mtx_lbl_i[r] = robtk_lbl_new (txt);
		rob_table_attach (ui->matrix, robtk_lbl_widget (ui->mtx_lbl_i[r]),
		                  0, 1,
		                  r + 1, r + 2,
		                  2, 2, RTK_SHRINK, RTK_SHRINK);

		for (unsigned int c = 0; c < N_OUTPUTS; ++c) {
			unsigned int n = r * N_OUTPUTS + c;

			// TODO log-scale mapping  -inf || -60 .. +6dB

			ui->mtx_gain[n] = robtk_dial_new_with_size (
			    0, 1, knob_gain_to_pos(1.0) / 192.f,
			    GD_W, GD_H, GD_CX, GD_CY, GD_RAD);

			robtk_dial_set_default (ui->mtx_gain[n], knob_gain_to_pos (c == r ? 1 : 0));
			robtk_dial_set_callback (ui->mtx_gain[n], cb_mtx_gain, ui);
			robtk_dial_annotation_callback (ui->mtx_gain[n], dial_annotation_db, ui);
			float detent = knob_gain_to_pos (1.f);
			robtk_dial_set_detents (ui->mtx_gain[n], 1 , &detent);

			robtk_dial_enable_states (ui->mtx_gain[n], 1);
			robtk_dial_set_state_color (ui->mtx_gain[n], 1, 1.0, .0, .0, .3);
			robtk_dial_set_default_state (ui->mtx_gain[n], 0);

			robwidget_set_mousedown (ui->mtx_gain[n]->rw, robtk_dial_mouse_intercept);
			ui->mtx_gain[n]->displaymode = 3;

			if (ui->touch) {
				robtk_dial_set_touch (ui->mtx_gain[n], ui->touch->touch, ui->touch->handle, M_OFFSET + n);
			}

			/* set faceplate */
			if (c == N_OUTPUTS - 1 && r == 0) {
				robtk_dial_set_scaled_surface_scale (ui->mtx_gain[n], ui->mtx_sf[5], 2.0);
			} else if (c == 0 && r == 0) {
				robtk_dial_set_scaled_surface_scale (ui->mtx_gain[n], ui->mtx_sf[4], 2.0);
			} else if (c == N_OUTPUTS - 1) {
				robtk_dial_set_scaled_surface_scale (ui->mtx_gain[n], ui->mtx_sf[3], 2.0);
			} else if (c == 0) {
				robtk_dial_set_scaled_surface_scale (ui->mtx_gain[n], ui->mtx_sf[2], 2.0);
			} else if (r == 0) {
				robtk_dial_set_scaled_surface_scale (ui->mtx_gain[n], ui->mtx_sf[1], 2.0);
			} else {
				robtk_dial_set_scaled_surface_scale (ui->mtx_gain[n], ui->mtx_sf[0], 2.0);
			}

			rob_table_attach (ui->matrix, robtk_dial_widget (ui->mtx_gain[n]),
			                  c + 1, c + 2,
			                  r + 1, r + 2,
			                  0, 0, RTK_SHRINK, RTK_SHRINK);

			memcpy (ui->mtx_gain[n]->rw->name, &n, sizeof (unsigned int));
		}
	}

	/* matrix out labels */
	for (unsigned int c = 0; c < N_OUTPUTS; ++c) {
		char txt[16];
		sprintf (txt, "Out\n%d", c + 1);
		ui->mtx_lbl_o[c] = robtk_lbl_new (txt);
		rob_table_attach (ui->matrix, robtk_lbl_widget (ui->mtx_lbl_o[c]),
		                  c + 1, c + 2,
		                  N_INPUTS + 1, N_INPUTS + 2,
		                  2, 2, RTK_SHRINK, RTK_SHRINK);
	}

	/* top-level packing */
	rob_vbox_child_pack (ui->rw, ui->matrix, TRUE, TRUE);
	return ui->rw;
}

/******************************************************************************
 * LV2
 */

static void
gui_cleanup (MatMixUI* ui)
{
	for (unsigned int r = 0; r < N_INPUTS; ++r) {
		robtk_lbl_destroy (ui->mtx_lbl_i[r]);
		for (unsigned int c = 0; c < N_OUTPUTS; ++c) {
			unsigned int n = r * N_OUTPUTS + c;
			robtk_dial_destroy (ui->mtx_gain[n]);
		}
	}

	for (int i = 0; i < N_OUTPUTS; ++i) {
		robtk_lbl_destroy (ui->mtx_lbl_o[i]);
	}

	robtk_lbl_destroy (ui->mtx_lbl);

	for (int i = 0; i < 6; ++i) {
		cairo_surface_destroy (ui->mtx_sf[i]);
	}
	pango_font_description_free (ui->font);

	rob_table_destroy (ui->matrix);
	rob_box_destroy (ui->rw);
}

#define LVGL_RESIZEABLE

static void ui_disable(LV2UI_Handle handle) { }
static void ui_enable(LV2UI_Handle handle) { }

static LV2UI_Handle
instantiate (
    void* const               ui_toplevel,
    const LV2UI_Descriptor*   descriptor,
    const char*               plugin_uri,
    const char*               bundle_path,
    LV2UI_Write_Function      write_function,
    LV2UI_Controller          controller,
    RobWidget**               widget,
    const LV2_Feature* const* features)
{
	MatMixUI* ui = (MatMixUI*)calloc (1, sizeof (MatMixUI));
	if (!ui) {
		return NULL;
	}

	for (int i = 0; features[i]; ++i) {
		if (!strcmp (features[i]->URI, LV2_UI__touch)) {
			ui->touch = (LV2UI_Touch*)features[i]->data;
		}
	}

	ui->nfo             = robtk_info (ui_toplevel);
	ui->write           = write_function;
	ui->controller      = controller;
	ui->disable_signals = true;

	*widget             = toplevel (ui, ui_toplevel);
	ui->disable_signals = false;
	return ui;
}

static enum LVGLResize
plugin_scale_mode (LV2UI_Handle handle)
{
	return LVGL_LAYOUT_TO_FIT;
}

static void
cleanup (LV2UI_Handle handle)
{
	MatMixUI* ui = (MatMixUI*)handle;
	gui_cleanup (ui);
	free (ui);
}

static const void*
extension_data (const char* uri)
{
	return NULL;
}

static void
port_event (
    LV2UI_Handle handle,
    uint32_t     port,
    uint32_t     buffer_size,
    uint32_t     format,
    const void*  buffer)
{
	MatMixUI* ui = (MatMixUI*)handle;

	if (format != 0) {
		return;
	}
	const float v = *(float*)buffer;

	if (port >= M_OFFSET && port < M_OFFSET + M_SIZE) {
		ui->disable_signals = true;
		robtk_dial_set_value (ui->mtx_gain[port - M_OFFSET], knob_gain_to_pos (v));
		robtk_dial_set_state (ui->mtx_gain[port - M_OFFSET], v < 0 ? 1 : 0);
		ui->disable_signals = false;
	}
}
