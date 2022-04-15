/*
 * Copyright (c) 2012 Hanspeter Portner (agenthp@users.sf.net)
 * 
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 * 
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 * 
 *     1. The origin of this software must not be misrepresented; you must not
 *     claim that you wrote the original software. If you use this software
 *     in a product, an acknowledgment in the product documentation would be
 *     appreciated but is not required.
 * 
 *     2. Altered source versions must be plainly marked as such, and must not be
 *     misrepresented as being the original software.
 * 
 *     3. This notice may not be removed or altered from any source
 *     distribution.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <math.h>

#include <lo/lo.h>
#include <lo/lo_lowlevel.h>

struct timespec clk_step = {
	.tv_sec = 0,
	.tv_nsec = 1e4
};

static void
_error (int num, const char *msg, const char *where)
{
	fprintf (stderr, "lo server error #%i '%s' at %s\n", num, msg, where);
}

int
main (int argc, char **argv)
{
	lo_address addr = NULL;
	FILE *file = NULL;
	double delay = 0.0;

	int c;
	while ( (c = getopt (argc, argv, "d:i:o:")) != -1)
		switch (c)
		{
			case 'd':
			{
				delay = atof (optarg);
			}
			case 'i':
			{
				if (!strcmp (optarg, "-"))
					file = stdin;
				else
					file = fopen (optarg, "rb");
				break;
			}
			case 'o':
			{
				addr = lo_address_new_from_url (optarg);
				break;
			}
			case '?':
				if ( (optopt == 'd') || (optopt == 'i') || (optopt == 'o') )
					fprintf (stderr, "Option `-%c' requires an argument.\n", optopt);
				else if (isprint (optopt))
					fprintf (stderr, "Unknown option `-%c'.\n", optopt);
				else
					fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
				return 1;
			default:
				return (1);
		}
		
	if (!addr || !file)
	{
		fprintf (stderr, "usage: %s [-d DELAY] -i FILE|- -o PROTOCOL://HOST:PORT\n\n", argv[0]);
		return (1);
	}

	int first = 1;
	lo_timetag offset;
	lo_timetag last = LO_TT_IMMEDIATE;

	uint8_t buf[1024];

	while (!feof (file))
	{
		size_t read;
		read = fread (&buf[0], 1, 16, file); // read #bundle and timestamp
		if (read != 16)
			break;

		// check whether message is a bundle
		if (strcmp (&buf[0], "#bundle"))
			fprintf (stderr, "message is not a bundle\n");

		lo_timetag *tt_ptr = (lo_timetag *)&buf[8];
		lo_timetag tt = *tt_ptr;
		tt.sec = lo_otoh32 (tt.sec);
		tt.frac = lo_otoh32 (tt.frac);

		//printf ("%x:%x\n", tt.sec, tt.frac);

		if (first)
		{
			lo_timetag_now (&offset);
			offset.sec -= tt.sec;
			if (offset.frac - tt.frac > offset.frac)
				offset.sec -= 1;
			offset.frac -= tt.frac;

			if (delay > 0.0)
			{
				uint32_t dsec = floor (delay);
				uint32_t dfrac = (delay - dsec) * 0xffffffff;

				offset.sec += dsec;
				if (offset.frac + dfrac < offset.frac)
					offset.sec += 1;
				offset.frac += dfrac;
			}

			first = 0;
		}

		tt.sec += offset.sec;
		if (tt.frac + offset.frac < tt.frac)
			tt.sec += 1;
		tt.frac += offset.frac;

		size_t ptr = 16;
		char c;

		lo_bundle bundle = lo_bundle_new (tt);

		while ( (c = fgetc (file)) != '#') // while not next bundle
		{
			ungetc (c, file);

			read = fread (&buf[ptr], 1, 4, file);
			if (read != 4)
				break;
			int32_t *size_ptr = (int32_t *)&buf[ptr];
			int32_t size = *size_ptr;
			size = lo_otoh32 (size);

			read = fread (&buf[ptr+4], 1, size, file);
			if (read != size)
				break;
			char *path = lo_get_path (&buf[ptr+4], size);

			lo_message msg = lo_message_deserialise (&buf[ptr+4], size, NULL);
			lo_bundle_add_message (bundle, path, msg);

			ptr += 4 + size;
		}

		ungetc (c, file);

		// our own schedular
		lo_timetag now;
		lo_timetag_now (&now);
		double diff = lo_timetag_diff (tt, now);
		if (diff > 0)
		{
			clk_step.tv_sec = floor (diff);
			clk_step.tv_nsec = (diff - clk_step.tv_sec) * 1e9;
			nanosleep (&clk_step, NULL);
		}

		lo_send_bundle (addr, bundle);
		lo_bundle_free_messages (bundle);

		// we cannot send everything in one bulk, as the receiver needs time to acutally process the messages, if not, half of them will be lost, we therefore have to wait
		//usleep (400);
	}

	if (file != stdin)
		fclose (file);
	
	return 0;
}
