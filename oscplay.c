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

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <stdint.h>

#include <lo/lo.h>
#include <lo/lo_lowlevel.h>

const uint32_t MAX_FRAC = 0xffffffff;

static void
_error (int num, const char *msg, const char *where)
{
	fprintf (stderr, "lo server error #%i '%s' at %s\n", num, msg, where);
}

static int
_handler (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *data)
{
	lo_address addr = data;

	lo_send_message (addr, path, msg);

	return 0;
}

int
main (int argc, char **argv)
{
	lo_server serv = NULL;
	lo_server_thread servT = NULL;
	lo_address addr = NULL;
	FILE *file = NULL;
	double delay = 0;

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

	servT = lo_server_thread_new_with_proto (NULL, LO_UDP, _error);
	serv = lo_server_thread_get_server (servT);
	lo_server_thread_add_method (servT, NULL, NULL, _handler, addr);
	lo_server_thread_start (servT);

	int first = 1;
	lo_timetag offset;

	uint8_t buf[1024];

	while (!feof (file))
	{
		size_t read;
		read = fread (&buf[0], 1, 20, file);
		if (read != 20)
			break;

		// check whether message is a bundle
		if (strcmp (&buf[0], "#bundle"))
			fprintf (stderr, "message is not a bundle\n");

		lo_timetag *tt_ptr = (lo_timetag *)&buf[8];
		lo_timetag tt = *tt_ptr;
		tt.sec = lo_otoh32 (tt.sec);
		tt.frac = lo_otoh32 (tt.frac);

		if (first)
		{
			lo_timetag_now (&offset);
			offset.sec -= tt.sec;
			if (offset.frac - tt.frac > offset.frac)
				offset.sec -= 1;
			offset.frac -= tt.frac;

			if (delay > 0.0)
			{
				uint32_t dsec = delay;
				uint32_t dfrac = (delay - dsec) * MAX_FRAC;

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
		tt_ptr->sec = lo_htoo32 (tt.sec);
		tt_ptr->frac = lo_htoo32 (tt.frac);

		int32_t *size_ptr = (int32_t *)&buf[16];
		int32_t size = *size_ptr;
		size = lo_otoh32 (size);

		read = fread (&buf[20], 1, size, file);

		lo_server_dispatch_data (serv, buf, 20+size);
	}

	while (lo_server_thread_events_pending (servT))
		;

	lo_server_thread_stop (servT);
	lo_server_thread_free (servT);

	if (file != stdin)
		fclose (file);
	
	return 0;
}
