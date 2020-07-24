/*
 * libifupdown/interface-file.h
 * Purpose: /etc/network/interfaces parser
 *
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "libifupdown/interface-file.h"
#include "libifupdown/fgetline.h"
#include "libifupdown/tokenize.h"

bool
lif_interface_file_parse(struct lif_dict *collection, const char *filename)
{
	lif_interface_collection_init(collection);
	struct lif_interface *cur_iface = NULL;

	FILE *f = fopen(filename, "r");
	if (f == NULL)
		return false;

	char linebuf[4096];
	while (lif_fgetline(linebuf, sizeof linebuf, f) != NULL)
	{
		char *bufp = linebuf;
		char *token = lif_next_token(&bufp);

		if (!*token || !isalpha(*token))
			continue;

		if (!strcmp(token, "source"))
		{
			char *source_filename = lif_next_token(&bufp);
			if (!*source_filename)
				goto parse_error;

			if (!strcmp(filename, source_filename))
			{
				fprintf(stderr, "%s: attempt to source %s would create infinite loop\n",
					filename, source_filename);
				goto parse_error;
			}

			lif_interface_file_parse(collection, source_filename);
		}
		else if (!strcmp(token, "auto"))
		{
			char *ifname = lif_next_token(&bufp);
			if (!*ifname && cur_iface == NULL)
				goto parse_error;
			else
			{
				cur_iface = lif_interface_collection_find(collection, ifname);
				if (cur_iface == NULL)
					goto parse_error;
			}

			cur_iface->is_auto = true;
		}
		else if (!strcmp(token, "iface"))
		{
			char *ifname = lif_next_token(&bufp);
			if (!*ifname)
				goto parse_error;

			cur_iface = lif_interface_collection_find(collection, ifname);
			if (cur_iface == NULL)
				goto parse_error;

			/* in original ifupdown config, we can have "inet loopback"
			 * or "inet dhcp" or such to designate hints.  lets pick up
			 * those hints here.
			 */
			char *inet_type = lif_next_token(&bufp);
			if (!*inet_type)
				continue;

			char *hint = lif_next_token(&bufp);
			if (!*hint)
				continue;

			if (!strcmp(hint, "dhcp"))
				cur_iface->is_dhcp = true;
			else if (!strcmp(hint, "loopback"))
				cur_iface->is_loopback = true;
		}
		else if (!strcmp(token, "use"))
		{
			char *executor = lif_next_token(&bufp);

			/* pass requires as compatibility env vars to appropriate executors (bridge, bond) */
			if (!strcmp(executor, "dhcp"))
				cur_iface->is_dhcp = true;
			else if (!strcmp(executor, "loopback"))
				cur_iface->is_loopback = true;
			else if (!strcmp(executor, "bridge"))
				cur_iface->is_bridge = true;
			else if (!strcmp(executor, "bond"))
				cur_iface->is_bond = true;

			lif_dict_add(&cur_iface->vars, token, strdup(executor));
		}
		else if (!strcmp(token, "address"))
		{
			char *addr = lif_next_token(&bufp);

			if (cur_iface == NULL)
			{
				fprintf(stderr, "%s: address '%s' without interface\n", filename, addr);
				goto parse_error;
			}

			lif_interface_address_add(cur_iface, addr);
		}
		else if (cur_iface != NULL)
		{
			lif_dict_add(&cur_iface->vars, token, strdup(bufp));
		}
	}

	fclose(f);
	return true;

parse_error:
	fprintf(stderr, "libifupdown: %s: failed to parse line \"%s\"\n",
		filename, linebuf);
	fclose(f);
	return false;
}
