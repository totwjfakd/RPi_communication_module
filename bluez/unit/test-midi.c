// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2015,2016 Felipe F. Tonello <eu@felipetonello.com>
 *  Copyright (C) 2016 ROLI Ltd.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

#define NUM_WRITE_TESTS 100

#include "src/shared/tester.h"
#include "profiles/midi/libmidi.h"

struct ble_midi_packet {
	const uint8_t *data;
	size_t size;
};

#define BLE_MIDI_PACKET_INIT(_packet) \
	{ \
		.data = (_packet), \
		.size = sizeof(_packet), \
	}

struct midi_read_test {
	const struct ble_midi_packet *ble_packet;
	size_t ble_packet_size;
	const snd_seq_event_t *event;
	size_t event_size;
};

#define BLE_READ_TEST_INIT(_ble_packet, _event) \
	{ \
		.ble_packet = (_ble_packet), \
		.ble_packet_size = G_N_ELEMENTS(_ble_packet), \
		.event = (_event), \
		.event_size = G_N_ELEMENTS(_event), \
	}

struct midi_write_test {
	const snd_seq_event_t *event;
	size_t event_size;
	const snd_seq_event_t *event_expect;
	size_t event_expect_size;
};

#define BLE_WRITE_TEST_INIT(_event, _event_expect)	  \
	{ \
		.event = (_event), \
		.event_size = G_N_ELEMENTS(_event), \
		.event_expect = (_event_expect), \
		.event_expect_size = G_N_ELEMENTS(_event_expect), \
	}

#define BLE_WRITE_TEST_INIT_BASIC(_event) BLE_WRITE_TEST_INIT(_event, _event)

#define NOTE_EVENT(_event, _channel, _note, _velocity) \
	{ \
		.type = SND_SEQ_EVENT_##_event, \
		.data = { \
			.note = { \
				.channel = (_channel), \
				.note = (_note), \
				.velocity = (_velocity), \
			}, \
		}, \
	}

#define CONTROL_EVENT(_event, _channel, _value, _param) \
	{ \
		.type = SND_SEQ_EVENT_##_event, \
		.data = { \
			.control = { \
				.channel = (_channel), \
				.value = (_value), \
				.param = (_param), \
			}, \
		}, \
	}

#define SYSEX_EVENT_RAW(_message, _size, _offset)	  \
	{ \
		.type = SND_SEQ_EVENT_SYSEX, \
		.data = { \
			.ext = { \
				.ptr = (void *)(_message) + (_offset), \
				.len = (_size), \
			}, \
		}, \
	}

#define SYSEX_EVENT(_message) SYSEX_EVENT_RAW(_message, sizeof(_message), 0)

/* Multiple messages in one packet */
static const uint8_t packet1_1[] = {
	0xa6, 0x88, 0xe8, 0x00, 0x40, 0x88, 0xb8, 0x4a,
	0x3f, 0x88, 0x98, 0x3e, 0x0e
};

/* Several one message per packet */
static const uint8_t packet1_2[] = {
	0xa6, 0xaa, 0xd8, 0x71
};

static const uint8_t packet1_3[] = {
	0xa6, 0xb7, 0xb8, 0x4a, 0x43
};

/* This message contains a running status message */
static const uint8_t packet1_4[] = {
	0xa6, 0xc4, 0xe8, 0x7e, 0x3f, 0x7d, 0x3f, 0xc4,
	0x7c, 0x3f
};

/* This message contain a running status message misplaced */
static const uint8_t packet1_5[] = {
	0xa6, 0xd9, 0x3e, 0x00, 0x88, 0x3e, 0x00
};

static const struct ble_midi_packet packet1[] = {
	BLE_MIDI_PACKET_INIT(packet1_1),
	BLE_MIDI_PACKET_INIT(packet1_2),
	BLE_MIDI_PACKET_INIT(packet1_3),
	BLE_MIDI_PACKET_INIT(packet1_4),
	BLE_MIDI_PACKET_INIT(packet1_5),
};

static const snd_seq_event_t event1[] = {
	CONTROL_EVENT(PITCHBEND, 8, 0, 0),    /* Pitch Bend */
	CONTROL_EVENT(CONTROLLER, 8, 63, 74), /* Control Change */
	NOTE_EVENT(NOTEON, 8, 62, 14),        /* Note On */
	CONTROL_EVENT(CHANPRESS, 8, 113, 0),  /* Channel Aftertouch */
	CONTROL_EVENT(CONTROLLER, 8, 67, 74), /* Control Change*/
	CONTROL_EVENT(PITCHBEND, 8, -2, 0),   /* Pitch Bend */
	CONTROL_EVENT(PITCHBEND, 8, -3, 0),   /* Pitch Bend */
	CONTROL_EVENT(PITCHBEND, 8, -4, 0),   /* Pitch Bend */
	NOTE_EVENT(NOTEOFF, 8, 62, 0),        /* Note Off */
};

static const struct midi_read_test midi1 = BLE_READ_TEST_INIT(packet1, event1);

/* Basic SysEx in one packet */
static const uint8_t packet2_1[] = {
	0xa6, 0xda, 0xf0, 0x01, 0x02, 0x03, 0xda, 0xf7
};

/* SysEx across two packets */
static const uint8_t packet2_2[] = {
	0xa6, 0xda, 0xf0, 0x01, 0x02, 0x03, 0x04, 0x05
};

static const uint8_t packet2_3[] = {
	0xa6, 0x06, 0x07, 0x08, 0x09, 0x0a, 0xdb, 0xf7
};

/* SysEx across multiple packets */
static const uint8_t packet2_4[] = {
	0xa6, 0xda, 0xf0, 0x01, 0x02, 0x03, 0x04, 0x05
};

static const uint8_t packet2_5[] = {
	0xa6, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c
};
static const uint8_t packet2_6[] = {
	0xa6, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13
};

static const uint8_t packet2_7[] = {
	0xa6, 0x14, 0x15, 0x16, 0x17, 0x18, 0xdb, 0xf7
};

/* Two SysEx interleaved in two packets */
static const uint8_t packet2_8[] = {
	0xa6, 0xda, 0xf0, 0x01, 0x02, 0x03, 0xda, 0xf7,
	0xda, 0xf0
};

static const uint8_t packet2_9[] = {
	0xa6, 0x06, 0x07, 0x08, 0x09, 0x0a, 0xdb, 0xf7
};


static const struct ble_midi_packet packet2[] = {
	BLE_MIDI_PACKET_INIT(packet2_1),
	BLE_MIDI_PACKET_INIT(packet2_2),
	BLE_MIDI_PACKET_INIT(packet2_3),
	BLE_MIDI_PACKET_INIT(packet2_4),
	BLE_MIDI_PACKET_INIT(packet2_5),
	BLE_MIDI_PACKET_INIT(packet2_6),
	BLE_MIDI_PACKET_INIT(packet2_7),
	BLE_MIDI_PACKET_INIT(packet2_8),
	BLE_MIDI_PACKET_INIT(packet2_9),
};

static const uint8_t sysex2_1[] = {
	0xf0, 0x01, 0x02, 0x03, 0xf7
};

static const uint8_t sysex2_2[] = {
	0xf0, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0xf7
};

static const uint8_t sysex2_3[] = {
	0xf0, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0xf7
};

static const uint8_t sysex2_4[] = {
	0xf0, 0x01, 0x02, 0x03, 0xf7
};

static const uint8_t sysex2_5[] = {
	0xf0, 0x06, 0x07, 0x08, 0x09, 0x0a, 0xf7
};

static const snd_seq_event_t event2[] = {
	SYSEX_EVENT(sysex2_1),
	SYSEX_EVENT(sysex2_2),
	SYSEX_EVENT(sysex2_3),
	SYSEX_EVENT(sysex2_4),
	SYSEX_EVENT(sysex2_5),
};

static const struct midi_read_test midi2 = BLE_READ_TEST_INIT(packet2, event2);

static void compare_events(const snd_seq_event_t *ev1,
                           const snd_seq_event_t *ev2)
{
	g_assert_cmpint(ev1->type, ==, ev2->type);

	switch (ev1->type) {
	case SND_SEQ_EVENT_NOTEON:
	case SND_SEQ_EVENT_NOTEOFF:
	case SND_SEQ_EVENT_KEYPRESS:
		g_assert_cmpint(ev1->data.note.channel,
		                ==,
		                ev2->data.note.channel);
		g_assert_cmpint(ev1->data.note.note,
		                ==,
		                ev2->data.note.note);
		g_assert_cmpint(ev1->data.note.velocity,
		                ==,
		                ev2->data.note.velocity);
		break;
	case SND_SEQ_EVENT_CONTROLLER:
		g_assert_cmpint(ev1->data.control.param,
		                ==,
		                ev2->data.control.param);
		break;
	case SND_SEQ_EVENT_PITCHBEND:
	case SND_SEQ_EVENT_CHANPRESS:
	case SND_SEQ_EVENT_PGMCHANGE:
		g_assert_cmpint(ev1->data.control.channel,
		                ==,
		                ev2->data.control.channel);
		g_assert_cmpint(ev1->data.control.value,
		                ==,
		                ev2->data.control.value);
		break;
	case SND_SEQ_EVENT_SYSEX:
		g_assert_cmpint(ev1->data.ext.len,
		                ==,
		                ev2->data.ext.len);
		g_assert(memcmp(ev1->data.ext.ptr,
		                ev2->data.ext.ptr,
		                ev2->data.ext.len) == 0);
		break;
	default:
		g_assert_not_reached();
	}
}

static void test_midi_reader(gconstpointer data)
{
	const struct midi_read_test *midi_test = data;
	struct midi_read_parser midi;
	int err;
	size_t i; /* ble_packet counter */
	size_t j; /* ble_packet length counter */
	size_t k = 0; /* event counter */

	err = midi_read_init(&midi);
	g_assert_cmpint(err, ==, 0);

	for (i = 0; i < midi_test->ble_packet_size; i++) {
		const size_t length = midi_test->ble_packet[i].size;
		j = 0;
		midi_read_reset(&midi);
		while (j < length) {
			snd_seq_event_t ev;
			const snd_seq_event_t *ev_expect = &midi_test->event[k];
			size_t count;

			g_assert_cmpint(k, <, midi_test->event_size);

			snd_seq_ev_clear(&ev);

			count = midi_read_raw(&midi,
			                      midi_test->ble_packet[i].data + j,
			                      length - j,
			                      &ev);

			g_assert_cmpuint(count, >, 0);

			if (ev.type == SND_SEQ_EVENT_NONE)
				goto _continue_loop;
			else
				k++;

			compare_events(ev_expect, &ev);

		_continue_loop:
			j += count;
		}
	}

	midi_read_free(&midi);

	tester_test_passed();
}

static const snd_seq_event_t event3[] = {
	CONTROL_EVENT(PITCHBEND, 8, 0, 0),    /* Pitch Bend */
	CONTROL_EVENT(CONTROLLER, 8, 63, 74), /* Control Change */
	NOTE_EVENT(NOTEON, 8, 62, 14),        /* Note On */
	CONTROL_EVENT(CHANPRESS, 8, 113, 0),  /* Channel Aftertouch */
	CONTROL_EVENT(CONTROLLER, 8, 67, 74), /* Control Change*/
	CONTROL_EVENT(PITCHBEND, 8, -2, 0),   /* Pitch Bend */
	CONTROL_EVENT(PITCHBEND, 8, -3, 0),   /* Pitch Bend */
	CONTROL_EVENT(PITCHBEND, 8, -4, 0),   /* Pitch Bend */
	NOTE_EVENT(NOTEOFF, 8, 62, 0),        /* Note Off */
};

static const struct midi_write_test midi3 = BLE_WRITE_TEST_INIT_BASIC(event3);

static const uint8_t sysex4_1[] = {
	0xf0, 0x01, 0x02, 0x03, 0xf7
};

static const uint8_t sysex4_2[] = {
	0xf0, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0xf7
};

static const uint8_t sysex4_3[] = {
	0xf0, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0xf7
};

static const uint8_t sysex4_4[] = {
	0xf0, 0x01, 0x02, 0x03, 0xf7
};

static const uint8_t sysex4_5[] = {
	0xf0, 0x06, 0x07, 0x08, 0x09, 0x0a, 0xf7
};

static const snd_seq_event_t event4[] = {
	SYSEX_EVENT(sysex4_1),
	SYSEX_EVENT(sysex4_2),
	SYSEX_EVENT(sysex4_3),
	SYSEX_EVENT(sysex4_4),
	SYSEX_EVENT(sysex4_5),
};

static const struct midi_write_test midi4 = BLE_WRITE_TEST_INIT_BASIC(event4);

/* Sysex split in multiple events (256 bytes each),
   it's common for ALSA to split big sysexes into 256 bytes events */
static const uint8_t sysex5_1[] = {
	0xf0, 0x1b, 0x46, 0x52, 0x68, 0x45, 0x19, 0x3d,
	0x70, 0x3c, 0x2b, 0x41, 0x09, 0x09, 0x28, 0x2d,
	0x66, 0x00, 0x4f, 0x06, 0x22, 0x30, 0x77, 0x0d,
	0x5d, 0x0e, 0x30, 0x21, 0x64, 0x5f, 0x72, 0x3c,
	0x31, 0x3a, 0x03, 0x37, 0x3f, 0x00, 0x66, 0x52,
	0x61, 0x6d, 0x03, 0x1e, 0x24, 0x2d, 0x33, 0x20,
	0x69, 0x17, 0x77, 0x36, 0x58, 0x53, 0x11, 0x25,
	0x2f, 0x51, 0x70, 0x4f, 0x00, 0x73, 0x6c, 0x3e,
	0x66, 0x62, 0x53, 0x20, 0x0e, 0x41, 0x0b, 0x0e,
	0x22, 0x27, 0x37, 0x14, 0x75, 0x31, 0x6f, 0x4b,
	0x3e, 0x4b, 0x55, 0x71, 0x33, 0x2d, 0x0d, 0x3e,
	0x58, 0x74, 0x4c, 0x44, 0x42, 0x2d, 0x47, 0x15,
	0x3c, 0x75, 0x5f, 0x10, 0x26, 0x54, 0x5a, 0x1e,
	0x07, 0x5a, 0x4a, 0x55, 0x55, 0x31, 0x40, 0x5d,
	0x7f, 0x25, 0x32, 0x0c, 0x74, 0x1a, 0x05, 0x22,
	0x66, 0x33, 0x5d, 0x38, 0x70, 0x15, 0x77, 0x35,
	0x52, 0x09, 0x3e, 0x63, 0x76, 0x37, 0x3c, 0x25,
	0x4c, 0x5c, 0x1e, 0x7d, 0x47, 0x0f, 0x2d, 0x67,
	0x6e, 0x68, 0x3c, 0x4e, 0x08, 0x6c, 0x16, 0x58,
	0x3d, 0x47, 0x19, 0x6d, 0x56, 0x12, 0x57, 0x56,
	0x5e, 0x2d, 0x0d, 0x0d, 0x43, 0x25, 0x75, 0x70,
	0x6a, 0x59, 0x3a, 0x7a, 0x41, 0x54, 0x03, 0x17,
	0x2d, 0x7a, 0x67, 0x06, 0x78, 0x53, 0x0f, 0x43,
	0x53, 0x4c, 0x02, 0x42, 0x68, 0x59, 0x0f, 0x51,
	0x74, 0x40, 0x1d, 0x64, 0x6f, 0x46, 0x3f, 0x77,
	0x71, 0x56, 0x2a, 0x24, 0x17, 0x25, 0x7f, 0x1f,
	0x60, 0x19, 0x1d, 0x75, 0x4e, 0x43, 0x3d, 0x0d,
	0x0d, 0x2e, 0x53, 0x44, 0x6c, 0x73, 0x7c, 0x6a,
	0x12, 0x02, 0x11, 0x38, 0x6c, 0x2b, 0x2c, 0x5d,
	0x4a, 0x48, 0x70, 0x7d, 0x44, 0x20, 0x41, 0x1e,
	0x15, 0x4c, 0x43, 0x33, 0x1b, 0x7e, 0x43, 0x2f,
	0x60, 0x6a, 0x61, 0x71, 0x21, 0x12, 0x32, 0x77,
	0x3c, 0x21, 0x7a, 0x5f, 0x58, 0x6c, 0x1f, 0x3a,
	0x68, 0x1c, 0x5d, 0x57, 0x1b, 0x0d, 0x77, 0x01,
	0x10, 0x31, 0x4a, 0x73, 0x03, 0x48, 0x18, 0x0a,
	0x32, 0x69, 0x38, 0x3f, 0x4d, 0x1a, 0x6e, 0x2f,
	0x30, 0x56, 0x4c, 0x66, 0x76, 0x16, 0x3c, 0x7a,
	0x31, 0x42, 0x40, 0x5d, 0x05, 0x33, 0x46, 0x53,
	0x5f, 0x2c, 0x4d, 0x0d, 0x39, 0x53, 0x20, 0x6e,
	0x61, 0x58, 0x12, 0x38, 0x25, 0x56, 0x22, 0x5b,
	0x27, 0x44, 0x27, 0x44, 0x59, 0x16, 0x77, 0x26,
	0x53, 0x35, 0x6e, 0x05, 0x70, 0x0f, 0x31, 0x30,
	0x23, 0x2c, 0x65, 0x16, 0x2d, 0x05, 0x3e, 0x22,
	0x6d, 0x22, 0x44, 0x3d, 0x18, 0x05, 0x10, 0x25,
	0x6b, 0x66, 0x69, 0x14, 0x63, 0x63, 0x1b, 0x04,
	0x41, 0x34, 0x6c, 0x09, 0x37, 0x6a, 0x63, 0x2e,
	0x70, 0x72, 0x44, 0x41, 0x33, 0x01, 0x05, 0x05,
	0x0b, 0x2a, 0x1a, 0x71, 0x55, 0x7e, 0x6e, 0x59,
	0x47, 0x7d, 0x2f, 0x44, 0x03, 0x52, 0x6e, 0x6b,
	0x4e, 0x11, 0x60, 0x1e, 0x0a, 0x71, 0x3d, 0x54,
	0x02, 0x1c, 0x73, 0x0b, 0x76, 0x32, 0x48, 0x66,
	0x36, 0x47, 0x6f, 0x5b, 0x6b, 0x3b, 0x14, 0x47,
	0x0c, 0x16, 0x6c, 0x27, 0x2a, 0x73, 0x17, 0x1d,
	0x16, 0x60, 0x63, 0x7b, 0x1d, 0x4f, 0x61, 0x5b,
	0x13, 0x20, 0x46, 0x0c, 0x71, 0x7d, 0x27, 0x43,
	0x49, 0x48, 0x7f, 0x3e, 0x4b, 0x7b, 0x27, 0x7b,
	0x73, 0x53, 0x57, 0x68, 0x05, 0x2a, 0x2f, 0x36,
	0x3b, 0x31, 0x11, 0x4e, 0x4c, 0x13, 0x2e, 0x06,
	0x06, 0x7c, 0x40, 0x37, 0x27, 0x0f, 0x01, 0x67,
	0x06, 0x09, 0x4b, 0x17, 0x0f, 0x4e, 0x51, 0x44,
	0x66, 0x6c, 0x70, 0x2a, 0x55, 0x62, 0x6d, 0x3b,
	0x16, 0x1b, 0x79, 0x08, 0x08, 0x77, 0x4a, 0x17,
	0x15, 0x47, 0x58, 0x5c, 0x5d, 0x3d, 0x12, 0x36,
	0x48, 0x5e, 0x51, 0x19, 0x6e, 0x5f, 0x64, 0x3c,
	0x62, 0x0b, 0x00, 0x15, 0x15, 0x2e, 0x4d, 0x5c,
	0x1b, 0x0a, 0x51, 0x1b, 0x13, 0x68, 0x14, 0x28,
	0x26, 0x69, 0x27, 0x52, 0x13, 0x1e, 0x19, 0x31,
	0x42, 0x0e, 0x3a, 0x29, 0x07, 0x41, 0x27, 0x40,
	0x4e, 0x68, 0x68, 0x78, 0x64, 0x36, 0x52, 0x7a,
	0x07, 0x6e, 0x46, 0x63, 0x4a, 0x6c, 0x5b, 0x4c,
	0x74, 0x14, 0x14, 0x76, 0x15, 0x2d, 0x79, 0x10,
	0x65, 0x48, 0x60, 0x6a, 0x1c, 0x65, 0x74, 0x73,
	0x56, 0x3c, 0x4b, 0x34, 0x20, 0x24, 0x36, 0x0d,
	0x3c, 0x59, 0x0f, 0x46, 0x47, 0x4a, 0x53, 0x62,
	0x63, 0x44, 0x22, 0x39, 0x15, 0x68, 0x60, 0x7b,
	0x73, 0x0f, 0x34, 0x79, 0x6a, 0x76, 0x4e, 0x0f,
	0x02, 0x5d, 0x09, 0x73, 0x76, 0x18, 0x48, 0x4f,
	0x72, 0x19, 0x71, 0x3c, 0x6e, 0x0b, 0x3b, 0x45,
	0x1c, 0x3e, 0x1b, 0x46, 0x74, 0x03, 0x5d, 0x0a,
	0x01, 0x62, 0x04, 0x2f, 0x6f, 0x03, 0x4c, 0x36,
	0x5f, 0x6a, 0x0c, 0x79, 0x34, 0x4f, 0x42, 0x6c,
	0x66, 0x21, 0x26, 0x21, 0x4a, 0x0e, 0x3e, 0x73,
	0x45, 0x43, 0x5e, 0x2a, 0x63, 0x32, 0x0b, 0x66,
	0x09, 0x46, 0x15, 0x46, 0x1c, 0x46, 0x10, 0x5b,
	0x09, 0x75, 0x67, 0x7f, 0x51, 0x6d, 0x12, 0x65,
	0x0d, 0x52, 0x06, 0x28, 0x61, 0x0f, 0x4e, 0x51,
	0x61, 0x75, 0x1f, 0x26, 0x31, 0x66, 0x34, 0x67,
	0x5d, 0x59, 0x2e, 0x18, 0x40, 0x63, 0x16, 0x12,
	0x49, 0x60, 0x1c, 0x62, 0x30, 0x21, 0x5c, 0x69,
	0x2c, 0x29, 0x1c, 0x3b, 0x3d, 0x13, 0x49, 0x4d,
	0x1f, 0x5f, 0x1d, 0x0a, 0x54, 0x1e, 0x52, 0x27,
	0x79, 0x79, 0x31, 0x03, 0x67, 0x02, 0x6a, 0x63,
	0x36, 0x5d, 0x38, 0x48, 0x1b, 0x4e, 0x5b, 0x63,
	0x7b, 0x7b, 0x4e, 0x71, 0x45, 0x37, 0x34, 0x44,
	0x03, 0x51, 0x31, 0x23, 0x0b, 0x18, 0x6d, 0x7f,
	0x76, 0x21, 0x17, 0x27, 0x45, 0x09, 0x0c, 0x2e,
	0x69, 0x74, 0x59, 0x2b, 0x75, 0x0c, 0x34, 0x0a,
	0x3a, 0x27, 0x25, 0x7b, 0x45, 0x0d, 0x59, 0x2f,
	0x2b, 0x57, 0x7e, 0x1f, 0x05, 0x62, 0x28, 0x79,
	0x7e, 0x1d, 0x58, 0x30, 0x35, 0x06, 0x67, 0x5b,
	0x7a, 0x00, 0x34, 0x32, 0x33, 0x2f, 0x68, 0x4b,
	0x76, 0x38, 0x7e, 0x58, 0x50, 0x56, 0x6d, 0x1f,
	0x14, 0x6f, 0x77, 0x39, 0x71, 0x35, 0x08, 0x44,
	0x3b, 0x09, 0x16, 0x19, 0x13, 0x1c, 0x67, 0x7d,
	0x7f, 0x56, 0x7e, 0x31, 0x6b, 0x67, 0x44, 0x76,
	0x53, 0x55, 0x6d, 0x3d, 0x13, 0x3b, 0x37, 0x1c,
	0x0b, 0x21, 0x58, 0x03, 0x31, 0x2d, 0x3d, 0x6c,
	0x01, 0x6d, 0x08, 0x1d, 0x03, 0x4d, 0x6e, 0x63,
	0x4c, 0x21, 0x2c, 0x57, 0x48, 0x07, 0x52, 0x2a,
	0x6d, 0x64, 0x0d, 0x56, 0x7e, 0x08, 0x3c, 0x1b,
	0x28, 0x04, 0x0f, 0x58, 0x0d, 0x6a, 0x73, 0x70,
	0x28, 0x0c, 0x6e, 0x1e, 0x09, 0x39, 0x46, 0x3e,
	0x62, 0x08, 0x72, 0x52, 0x42, 0x02, 0x78, 0x62,
	0x31, 0x73, 0x0d, 0x4d, 0x5c, 0x07, 0x64, 0x17,
	0x55, 0x29, 0x60, 0x07, 0x67, 0x59, 0x63, 0x78,
	0x73, 0x17, 0x42, 0x27, 0x0e, 0x77, 0x15, 0x60,
	0x07, 0x46, 0x53, 0x6a, 0x05, 0x38, 0x12, 0x14,
	0x1f, 0x1b, 0x11, 0x6a, 0x1d, 0x02, 0x3c, 0x05,
	0x75, 0x6d, 0x51, 0x16, 0x10, 0x6f, 0x02, 0x46,
	0x39, 0x2e, 0x37, 0x47, 0x7a, 0x5b, 0x39, 0x15,
	0x14, 0x4b, 0x77, 0x0b, 0x19, 0x24, 0x4d, 0x36,
	0x33, 0x4c, 0x6a, 0x53, 0x79, 0x69, 0x57, 0x17,
	0x10, 0x75, 0x1f, 0x72, 0x08, 0x71, 0x58, 0x14,
	0x46, 0x4a, 0x6f, 0x3c, 0x30, 0x34, 0x5b, 0x36,
	0x42, 0x13, 0x11, 0x45, 0x78, 0x5a, 0x57, 0x68,
	0x33, 0x4b, 0x21, 0x00, 0x06, 0x6b, 0x3d, 0x17,
	0x0e, 0x6a, 0x2b, 0x2a, 0x32, 0x3a, 0x2a, 0x46,
	0x79, 0x1f, 0x56, 0x40, 0x43, 0x36, 0x18, 0xf7,
};

static const snd_seq_event_t event5[] = {
	/* SysEx over 4 events */
	SYSEX_EVENT_RAW(sysex5_1, 256, 0),
	SYSEX_EVENT_RAW(sysex5_1, 256, 256),
	SYSEX_EVENT_RAW(sysex5_1, 256, 512),
	SYSEX_EVENT_RAW(sysex5_1, 256, 768),
};

static const snd_seq_event_t event5_expect[] = {
	SYSEX_EVENT(sysex5_1),
};

static const struct midi_write_test midi5 = BLE_WRITE_TEST_INIT(event5, event5_expect);

struct midi_data {
	size_t events_tested;
	const struct midi_write_test *midi_test;
	struct midi_read_parser *midi_in;
} midi_data;

static void compare_events_cb(const struct midi_write_parser *parser,
						void *user_data)
{
	struct midi_data *midi_data = user_data;
	const struct midi_write_test *midi_test = midi_data->midi_test;
	struct midi_read_parser *midi_in = midi_data->midi_in;
	size_t i = 0;

	midi_read_reset(midi_in);

	while (i < midi_write_data_size(parser)) {
		snd_seq_event_t ev;
		size_t count;

		snd_seq_ev_clear(&ev);

		count = midi_read_raw(midi_in, midi_write_data(parser) + i,
					midi_write_data_size(parser) - i, &ev);

		g_assert_cmpuint(count, >, 0);

		if (ev.type != SND_SEQ_EVENT_NONE){
			g_assert_cmpint(midi_data->events_tested, <,
						midi_test->event_expect_size);
			compare_events(&midi_test->event_expect
				       [midi_data->events_tested],
				       &ev);
			midi_data->events_tested++;
		}

		i += count;
	}
};

static void test_midi_writer(gconstpointer data)
{
	const struct midi_write_test *midi_test = data;
	struct midi_write_parser midi_out;
	struct midi_read_parser midi_in;
	size_t i; /* event counter */
	size_t j; /* test counter */
	struct midi_data midi_data;

	midi_read_init(&midi_in);

	for (j = 0; j < NUM_WRITE_TESTS; j++) {

		/* Range of test for different MTU sizes. The spec specifies
		   sizes of packet as MTU - 3 */
		midi_write_init(&midi_out, g_random_int_range(5, 512));

		midi_data.events_tested = 0;
		midi_data.midi_test = midi_test;
		midi_data.midi_in = &midi_in;

		for (i = 0; i < midi_test->event_size; i++)
			midi_read_ev(&midi_out, &midi_test->event[i],
			             compare_events_cb, &midi_data);

		if (midi_write_has_data(&midi_out))
			compare_events_cb(&midi_out, &midi_data);

		g_assert_cmpint(midi_data.events_tested,
		                ==,
		                midi_test->event_expect_size);

		midi_write_free(&midi_out);
	}
	midi_read_free(&midi_in);

	tester_test_passed();
}

int main(int argc, char *argv[])
{
	tester_init(&argc, &argv);

	tester_add("Raw BLE packets read",
	           &midi1, NULL, test_midi_reader, NULL);
	tester_add("Raw BLE packets SysEx read",
	           &midi2, NULL, test_midi_reader, NULL);
	tester_add("ALSA Seq events to Raw BLE packets",
	           &midi3, NULL, test_midi_writer, NULL);
	tester_add("ALSA SysEx events to Raw BLE packets",
	           &midi4, NULL, test_midi_writer, NULL);
	tester_add("Split ALSA SysEx events to raw BLE packets",
	           &midi5, NULL, test_midi_writer, NULL);

	return tester_run();
}