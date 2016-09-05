/*-
 * Copyright (c) 2007, 2008 Edward Tomasz Napierała <trasz@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * This is piano_keyboard, piano keyboard-like GTK+ widget.  It contains
 * no MIDI-specific code.
 *
 * For questions and comments, contact Edward Tomasz Napierala <trasz@FreeBSD.org>.
 */

#include <assert.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "pianokeyboard.h"

#define PIANO_KEYBOARD_DEFAULT_WIDTH 730
#define PIANO_KEYBOARD_DEFAULT_HEIGHT 70

enum {
	NOTE_ON_SIGNAL,
	NOTE_OFF_SIGNAL,
	LAST_SIGNAL
};

static guint	piano_keyboard_signals[LAST_SIGNAL] = { 0 };

static void
draw_keyboard_cue(PianoKeyboard *pk)
{
	int w, h, first_note_in_lower_row, last_note_in_lower_row,
	    first_note_in_higher_row, last_note_in_higher_row;

	GdkGC *gc;

	w = pk->notes[0].w;
	h = pk->notes[0].h;

	gc = GTK_WIDGET(pk)->style->fg_gc[0];

	first_note_in_lower_row = (pk->octave + 5) * 12;
	last_note_in_lower_row = (pk->octave + 6) * 12 - 1;
	first_note_in_higher_row = (pk->octave + 6) * 12;
	last_note_in_higher_row = (pk->octave + 7) * 12 + 4;

	gdk_draw_line(GTK_WIDGET(pk)->window, gc, pk->notes[first_note_in_lower_row].x + 3,
	    h - 6, pk->notes[last_note_in_lower_row].x + w - 3, h - 6);

	gdk_draw_line(GTK_WIDGET(pk)->window, gc, pk->notes[first_note_in_higher_row].x + 3,
	    h - 9, pk->notes[last_note_in_higher_row].x + w - 3, h - 9);
}

static void 
draw_note(PianoKeyboard *pk, int note)
{
	if (note < pk->min_note)
		return;
	if (note > pk->max_note)
		return;
	int is_white, x, w, h;

	GdkColor black = {0, 0, 0, 0};
	GdkColor white = {0, 65535, 65535, 65535};

	GdkGC *gc = GTK_WIDGET(pk)->style->fg_gc[0];
	GtkWidget *widget;

	is_white = pk->notes[note].white;

	x = pk->notes[note].x;
	w = pk->notes[note].w;
	h = pk->notes[note].h;

	if (pk->notes[note].pressed || pk->notes[note].sustained)
		is_white = !is_white;

	if (is_white)
		gdk_gc_set_rgb_fg_color(gc, &white);
	else
		gdk_gc_set_rgb_fg_color(gc, &black);

	gdk_draw_rectangle(GTK_WIDGET(pk)->window, gc, TRUE, x, 0, w, h);
	gdk_gc_set_rgb_fg_color(gc, &black);
	gdk_draw_rectangle(GTK_WIDGET(pk)->window, gc, FALSE, x, 0, w, h);

	if (pk->enable_keyboard_cue)
		draw_keyboard_cue(pk);

	/* We need to redraw black keys that partially obscure the white one. */
	if (note < NNOTES - 2 && !pk->notes[note + 1].white)
		draw_note(pk, note + 1);

	if (note > 0 && !pk->notes[note - 1].white)
		draw_note(pk, note - 1);

	/*
	 * XXX: This doesn't really belong here.  Originally I wanted to pack PianoKeyboard into GtkFrame
	 * packed into GtkAlignment.  I failed to make it behave the way I want.  GtkFrame would need
	 * to adapt to the "proper" size of PianoKeyboard, i.e. to the useful_width, not allocated width;
	 * that didn't work.
	 */
	widget = GTK_WIDGET(pk);
	gtk_paint_shadow(widget->style, widget->window, GTK_STATE_NORMAL, GTK_SHADOW_IN, NULL,
	    widget, NULL, pk->widget_margin, 0, widget->allocation.width - pk->widget_margin * 2 + 1,
	    widget->allocation.height);
}

static int 
press_key(PianoKeyboard *pk, int key)
{
	assert(key >= 0);
	assert(key < NNOTES);

	pk->maybe_stop_sustained_notes = 0;

	/* This is for keyboard autorepeat protection. */
	if (pk->notes[key].pressed)
		return (0);

	if (pk->sustain_new_notes)
		pk->notes[key].sustained = 1;
	else
		pk->notes[key].sustained = 0;

	pk->notes[key].pressed = 1;

	g_signal_emit_by_name(GTK_WIDGET(pk), "note-on", key);
	draw_note(pk, key);

	return (1);
}

static int 
release_key(PianoKeyboard *pk, int key)
{
	assert(key >= 0);
	assert(key < NNOTES);

	pk->maybe_stop_sustained_notes = 0;

	if (!pk->notes[key].pressed)
		return (0);

	if (pk->sustain_new_notes)
		pk->notes[key].sustained = 1;

	pk->notes[key].pressed = 0;

	if (pk->notes[key].sustained)
		return (0);

	g_signal_emit_by_name(GTK_WIDGET(pk), "note-off", key);
	draw_note(pk, key);

	return (1);
}

static void 
stop_unsustained_notes(PianoKeyboard *pk)
{
	int i;

	for (i = 0; i < NNOTES; i++) {
		if (pk->notes[i].pressed && !pk->notes[i].sustained) {
			pk->notes[i].pressed = 0;
			g_signal_emit_by_name(GTK_WIDGET(pk), "note-off", i);
			draw_note(pk, i);
		}
	}
}

static void 
stop_sustained_notes(PianoKeyboard *pk)
{
	int i;

	for (i = 0; i < NNOTES; i++) {
		if (pk->notes[i].sustained) {
			pk->notes[i].pressed = 0;
			pk->notes[i].sustained = 0;
			g_signal_emit_by_name(GTK_WIDGET(pk), "note-off", i);
			draw_note(pk, i);
		}
	}
}

static int
key_binding(PianoKeyboard *pk, guint16 key)
{
	assert(pk->key_bindings != NULL);

	if (key >= pk->key_bindings->len)
		return (0);

	return (g_array_index(pk->key_bindings, int, key));
}

static void
bind_key(PianoKeyboard *pk, guint key, int note)
{
	assert(pk->key_bindings != NULL);

	if (key >= pk->key_bindings->len)
		g_array_set_size(pk->key_bindings, key + 1);
	g_array_index(pk->key_bindings, int, key) = note;
}

static void
clear_notes(PianoKeyboard *pk)
{
	assert(pk->key_bindings != NULL);

	g_array_set_size(pk->key_bindings, 0);
}

static void 
bind_keys_qwerty(PianoKeyboard *pk)
{
	clear_notes(pk);

	/* Lower keyboard row - "zxcvbnm". 52 is z, 38 is a. */
	bind_key(pk, 52, 12);	/* C0 */
	bind_key(pk, 39, 13);
	bind_key(pk, 53, 14);
	bind_key(pk, 40, 15);
	bind_key(pk, 54, 16);
	bind_key(pk, 55, 17);
	bind_key(pk, 42, 18);
	bind_key(pk, 56, 19);
	bind_key(pk, 43, 20);
	bind_key(pk, 57, 21);
	bind_key(pk, 44, 22);
	bind_key(pk, 58, 23);

	/* Map the remaining keys. This overlaps with qwe. */
	bind_key(pk, 59, 24);
	bind_key(pk, 46, 25);
	bind_key(pk, 60, 26);
	bind_key(pk, 47, 27);
	bind_key(pk, 61, 28);

	/* Upper keyboard row, first octave - "qwertyu". 24 is q, 10 is 1. */
	bind_key(pk, 24, 24);
	bind_key(pk, 11, 25);
	bind_key(pk, 25, 26);
	bind_key(pk, 12, 27);
	bind_key(pk, 26, 28);
	bind_key(pk, 27, 29);
	bind_key(pk, 14, 30);
	bind_key(pk, 28, 31);
	bind_key(pk, 15, 32);
	bind_key(pk, 29, 33);
	bind_key(pk, 16, 34);
	bind_key(pk, 30, 35);

	/* Upper keyboard row, the rest - "iop". */
	bind_key(pk, 31, 36);
	bind_key(pk, 18, 37);
	bind_key(pk, 32, 38);
	bind_key(pk, 19, 39);
	bind_key(pk, 33, 40);

	/* We might as well bind these too: "[=]\" */
	bind_key(pk, 34, 41);
	bind_key(pk, 21, 42);
	bind_key(pk, 35, 43);
	bind_key(pk, 51, 45); /* yes, really, at least here... */
}

static gint 
keyboard_event_handler(GtkWidget *mk, GdkEventKey *event, gpointer notused)
{
	int note;
	PianoKeyboard *pk = PIANO_KEYBOARD(mk);

	note = key_binding(pk, event->hardware_keycode);

	if (note <= 0) {
		/* Key was not bound.  Maybe it's one of the keys handled in jack-keyboard.c. */
		return (FALSE);
	}

	note += pk->octave * 12;

	assert(note >= 0);
	assert(note < NNOTES);

	if (event->type == GDK_KEY_PRESS) {
		press_key(pk, note);

	} else if (event->type == GDK_KEY_RELEASE) {
		release_key(pk, note);
	}

	return (TRUE);
}

static int 
get_note_for_xy(PianoKeyboard *pk, int x, int y)
{
	int height, note;

	height = GTK_WIDGET(pk)->allocation.height;

	if (y <= ((height * 2) / 3)) { /* might be a black key */
		for (note = 0; note <= pk->max_note; ++note) {
			if (pk->notes[note].white)
				continue;

			if (x >= pk->notes[note].x && x <= pk->notes[note].x + pk->notes[note].w)
				return (note);
		}
	}

	for (note = 0; note <= pk->max_note; ++note) {
		if (!pk->notes[note].white)
			continue;

		if (x >= pk->notes[note].x && x <= pk->notes[note].x + pk->notes[note].w)
			return (note);
	}

	return (-1);
}

static gboolean 
mouse_button_event_handler(PianoKeyboard *pk, GdkEventButton *event, gpointer notused)
{
	int x, y, note;

	x = event->x;
	y = event->y;

	note = get_note_for_xy(pk, x, y);

	if (event->button != 1)
		return (TRUE);

	if (event->type == GDK_BUTTON_PRESS) {
		/* This is possible when you make the window a little wider and then click
		   on the grey area. */
		if (note < 0) {
			return (TRUE);
		}

		if (pk->note_being_pressed_using_mouse >= 0)
			release_key(pk, pk->note_being_pressed_using_mouse);

		press_key(pk, note);
		pk->note_being_pressed_using_mouse = note;

	} else if (event->type == GDK_BUTTON_RELEASE) {
		if (note >= 0) {
			release_key(pk, note);

		} else {
			if (pk->note_being_pressed_using_mouse >= 0)
				release_key(pk, pk->note_being_pressed_using_mouse);
		}

		pk->note_being_pressed_using_mouse = -1;

	}

	return (TRUE);
}

static gboolean 
mouse_motion_event_handler(PianoKeyboard *pk, GdkEventMotion *event, gpointer notused)
{
	int note;

	if ((event->state & GDK_BUTTON1_MASK) == 0)
		return (TRUE);

	note = get_note_for_xy(pk, event->x, event->y);

	if (note != pk->note_being_pressed_using_mouse && note >= 0) {
		
		if (pk->note_being_pressed_using_mouse >= 0)
			release_key(pk, pk->note_being_pressed_using_mouse);
		press_key(pk, note);
		pk->note_being_pressed_using_mouse = note;
	}

	return (TRUE);
}

static gboolean
piano_keyboard_expose(GtkWidget *widget, GdkEventExpose *event)
{
	int i;
	PianoKeyboard *pk = PIANO_KEYBOARD(widget);

	for (i = 0; i < NNOTES; i++)
		draw_note(pk, i);

	return (TRUE);
}

static void 
piano_keyboard_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
	requisition->width = PIANO_KEYBOARD_DEFAULT_WIDTH;
	requisition->height = PIANO_KEYBOARD_DEFAULT_HEIGHT;
}

static int is_black(int key)
{
	int note_in_octave = key % 12;
	if(              note_in_octave == 1 || note_in_octave == 3 ||
	     note_in_octave == 6 || note_in_octave == 8 || note_in_octave == 10)
		return 1;
	return 0;
}

static double black_key_left_shift(int key)
{
	int note_in_octave = key % 12;
	switch (note_in_octave)
	{
	case 1:
		return 2.0/3.0;
	case 3:
		return 1.0/3.0;
	case 6:
		return 2.0/3.0;
	case 8:
		return 0.5;
	case 10:
		return 1.0/3.0;
	default:
		return 0;
	}
	return 0;
}

static void
recompute_dimensions(PianoKeyboard *pk)
{
	int number_of_white_keys = 0, skipped_white_keys = 0, key_width, black_key_width, useful_width, note,
	    white_key, width, height;

	for (note = pk->min_note; note <= pk->max_note; ++note)
		if (!is_black(note))
			++number_of_white_keys;
	for (note = 0; note < pk->min_note; ++note)
		if (!is_black(note))
			++skipped_white_keys;

	width = GTK_WIDGET(pk)->allocation.width;
	height = GTK_WIDGET(pk)->allocation.height;

	key_width = width / number_of_white_keys;
	black_key_width = key_width * 0.8;
	useful_width = number_of_white_keys * key_width;
	pk->widget_margin = (width - useful_width) / 2;

	for (note = 0, white_key = -skipped_white_keys; note < NNOTES; note++) {
		if (is_black(note)) {
			/* This note is black key. */
			pk->notes[note].x = pk->widget_margin + 
			    (white_key * key_width) -
			    (black_key_width * black_key_left_shift(note));
			pk->notes[note].w = black_key_width;
			pk->notes[note].h = (height * 2) / 3;
			pk->notes[note].white = 0;
			continue;
		}

		/* This note is white key. */
		pk->notes[note].x = pk->widget_margin + white_key * key_width;
		pk->notes[note].w = key_width;
		pk->notes[note].h = height;
		pk->notes[note].white = 1;

		white_key++;
	}
}

static void
piano_keyboard_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	/* XXX: Are these two needed? */
	g_return_if_fail(widget != NULL);
	g_return_if_fail(allocation != NULL);

	widget->allocation = *allocation;

	recompute_dimensions(PIANO_KEYBOARD(widget));

	if (GTK_WIDGET_REALIZED(widget))
		gdk_window_move_resize (widget->window, allocation->x, allocation->y, allocation->width, allocation->height);
}

static void
piano_keyboard_class_init(PianoKeyboardClass *klass)
{
	GtkWidgetClass *widget_klass;

	/* Set up signals. */
	piano_keyboard_signals[NOTE_ON_SIGNAL] = g_signal_new ("note-on",
	    G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
	    0, NULL, NULL, g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

	piano_keyboard_signals[NOTE_OFF_SIGNAL] = g_signal_new ("note-off",
	    G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
	    0, NULL, NULL, g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

	widget_klass = (GtkWidgetClass*)klass;	

	widget_klass->expose_event = piano_keyboard_expose;
	widget_klass->size_request = piano_keyboard_size_request;
	widget_klass->size_allocate = piano_keyboard_size_allocate;
}

static void
piano_keyboard_init(GtkWidget *mk)
{
	gtk_widget_add_events(mk, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);

	g_signal_connect(G_OBJECT(mk), "button-press-event", G_CALLBACK(mouse_button_event_handler), NULL);
	g_signal_connect(G_OBJECT(mk), "button-release-event", G_CALLBACK(mouse_button_event_handler), NULL);
	g_signal_connect(G_OBJECT(mk), "motion-notify-event", G_CALLBACK(mouse_motion_event_handler), NULL);
        g_signal_connect(G_OBJECT(mk), "key-press-event", G_CALLBACK(keyboard_event_handler), NULL);
        g_signal_connect(G_OBJECT(mk), "key-release-event", G_CALLBACK(keyboard_event_handler), NULL);
}

GType
piano_keyboard_get_type(void)
{
	static GType mk_type = 0;

	if (!mk_type) {
		static const GTypeInfo mk_info = {
			sizeof(PianoKeyboardClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) piano_keyboard_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (PianoKeyboard),
			0,    /* n_preallocs */
			(GInstanceInitFunc) piano_keyboard_init,
		};

		mk_type = g_type_register_static(GTK_TYPE_DRAWING_AREA, "PianoKeyboard", &mk_info, 0);
	}

	return (mk_type);
}

GtkWidget *
piano_keyboard_new(void)
{
	GtkWidget *widget;
	PianoKeyboard *pk;

	widget = gtk_type_new(piano_keyboard_get_type());
	pk = PIANO_KEYBOARD(widget);

	pk->maybe_stop_sustained_notes = 0;
	pk->sustain_new_notes = 0;
	pk->enable_keyboard_cue = 0;
	pk->octave = 4;
	pk->note_being_pressed_using_mouse = -1;
	memset((void *)pk->notes, 0, sizeof(struct Note) * NNOTES);
	/* 255 here is a random value larger than the highest key we bind. */
	pk->key_bindings = g_array_sized_new(FALSE, TRUE, sizeof(int), 255);
	pk->min_note = PIANO_MIN_NOTE;
	pk->max_note = PIANO_MAX_NOTE;
	bind_keys_qwerty(pk);

	return (widget);
}

void
piano_keyboard_set_keyboard_cue(PianoKeyboard *pk, int enabled)
{
	pk->enable_keyboard_cue = enabled;
}

void
piano_keyboard_sustain_press(PianoKeyboard *pk)
{
	if (!pk->sustain_new_notes) {
		pk->sustain_new_notes = 1;
		pk->maybe_stop_sustained_notes = 1;
	}
}

void	
piano_keyboard_sustain_release(PianoKeyboard *pk)
{
	if (pk->maybe_stop_sustained_notes)
		stop_sustained_notes(pk);

	pk->sustain_new_notes = 0;
}

void
piano_keyboard_set_note_on(PianoKeyboard *pk, int note)
{
	if (pk->notes[note].pressed == 0) {
		pk->notes[note].pressed = 1;
		draw_note(pk, note);
	}
}

void
piano_keyboard_set_note_off(PianoKeyboard *pk, int note)
{
	if (pk->notes[note].pressed || pk->notes[note].sustained) {
		pk->notes[note].pressed = 0;
		pk->notes[note].sustained = 0;
		draw_note(pk, note);
	}
}

void
piano_keyboard_set_octave(PianoKeyboard *pk, int octave)
{
	stop_unsustained_notes(pk);
	pk->octave = octave;
	gtk_widget_queue_draw(GTK_WIDGET(pk));
}

gboolean
piano_keyboard_set_keyboard_layout(PianoKeyboard *pk, const char *layout)
{
	assert(layout);

	if (!strcasecmp(layout, "QWERTY")) {
		bind_keys_qwerty(pk);

	} else {
		/* Unknown layout name. */
		return (TRUE);
	}

	return (FALSE);
}

void
piano_keyboard_enable_all_midi_notes(PianoKeyboard* pk)
{
	pk->min_note = 0;
	pk->max_note = NNOTES-1;
	recompute_dimensions(pk);
}

