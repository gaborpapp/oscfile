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
#include <signal.h>

#include <lo/lo.h>
#include <lo/lo_lowlevel.h>

volatile done = 0;

static void
_quit (int signal)
{
	done = 1;
}

static void
_error (int num, const char *msg, const char *where)
{
	fprintf (stderr, "lo server error #%i '%s' at %s\n", num, msg, where);
}

static int
_handler (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *data)
{
	FILE *file = data;
	uint8_t *buf;
	size_t size;
	lo_bundle bundle;
	lo_timetag now;

	now = lo_message_get_timestamp (msg);
	if ( (now.sec == LO_TT_IMMEDIATE.sec) && (now.frac == LO_TT_IMMEDIATE.frac) )
		lo_timetag_now (&now);
	bundle = lo_bundle_new (now);
	lo_bundle_add_message (bundle, path, msg);
	buf = lo_bundle_serialise (bundle, NULL, &size);
	//lo_bundle_free_messages (bundle);
	lo_bundle_free (bundle);

	fwrite (buf, size, sizeof (uint8_t), file);
	fflush (file);

	free (buf);

	return 0;
}

int
main (int argc, char **argv)
{
	lo_server_thread serv = NULL;
	FILE *file = NULL;

	int c;
	while ( (c = getopt (argc, argv, "i:o:")) != -1)
		switch (c)
		{
			case 'i':
			{
				int proto = lo_url_get_protocol_id (optarg);
				if (proto == -1)
					fprintf (stderr, "protocol not supported\n");
				char *port = lo_url_get_port (optarg);
				serv = lo_server_thread_new_with_proto (port, proto, _error);
				free (port);
				break;
			}
			case 'o':
			{
				if (!strcmp (optarg, "-"))
					file = stdout;
				else
					file = fopen (optarg, "wb");
				break;
			}
			case '?':
				if ( (optopt == 'i') || (optopt == 'o') )
					fprintf (stderr, "Option `-%c' requires an argument.\n", optopt);
				else if (isprint (optopt))
					fprintf (stderr, "Unknown option `-%c'.\n", optopt);
				else
					fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
				return 1;
			default:
				return (1);
		}
		
	if (!serv || !file)
	{
		fprintf (stderr, "usage: %s -i PROTOCOL://HOST:PORT -o FILE|-\n\n", argv[0]);
		return (1);
	}

	lo_server_thread_add_method (serv, NULL, NULL, _handler, file);
	lo_server_thread_start (serv);

	signal (SIGHUP, _quit);
	signal (SIGQUIT, _quit);
	signal (SIGTERM, _quit);
	signal (SIGINT, _quit);

	while (!done)
		usleep (10);

	fprintf (stderr, "cleaning up\n");

	lo_server_thread_stop (serv);
	lo_server_thread_free (serv);

	if (file != stdout)
		fclose (file);
	
	return 0;
}
