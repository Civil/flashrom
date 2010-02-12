/*
 * This file is part of the flashrom project.
 *
 * Copyright (C) 2009,2010 Michael Karcher
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "flash.h"

enum dmi_strings {
	DMI_SYS_MANUFACTURER,
	DMI_SYS_PRODUCT,
	DMI_SYS_VERSION,
	DMI_BB_MANUFACTURER,
	DMI_BB_PRODUCT,
	DMI_BB_VERSION,
	DMI_ID_INVALID /* This must always be the last entry */
};

/* The short_id for baseboard starts with "m" as in mainboard to leave
   "b" available for BIOS */
struct {
	const char *dmidecode_name;
	char short_id[3];
} dmi_properties[DMI_ID_INVALID] = {
	{"system-manufacturer", "sm"},
	{"system-product-name", "sp"},
	{"system-version", "sv"},
	{"baseboard-manufacturer", "mm"},
	{"baseboard-product-name", "mp"},
	{"baseboard-version", "mv"}
};

#define DMI_COMMAND_LEN_MAX 260
const char *dmidecode_command = "dmidecode";

int has_dmi_support = 0;
char *dmistrings[DMI_ID_INVALID];

/* strings longer than 4096 in DMI are just insane */
#define DMI_MAX_ANSWER_LEN 4096

void dmi_init(void)
{
	FILE *dmidecode_pipe;
	int i;
	char *answerbuf = malloc(DMI_MAX_ANSWER_LEN);
	if(!answerbuf)
	{
		fprintf(stderr, "DMI: couldn't alloc answer buffer\n");
		return;
	}
	for (i = 0; i < DMI_ID_INVALID; i++)
	{
		char commandline[DMI_COMMAND_LEN_MAX+40];
		snprintf(commandline, sizeof(commandline),
		         "%s -s %s", dmidecode_command,
		         dmi_properties[i].dmidecode_name);
		dmidecode_pipe = popen(commandline, "r");
		if (!dmidecode_pipe)
		{
			printf_debug("DMI pipe open error\n");
			goto out_free;
		}
		if (!fgets(answerbuf, DMI_MAX_ANSWER_LEN, dmidecode_pipe) &&
		    ferror(dmidecode_pipe))
		{
			printf_debug("DMI pipe read error\n");
			pclose(dmidecode_pipe);
			goto out_free;
		}
		/* Toss all output above DMI_MAX_ANSWER_LEN away to prevent
		   deadlock on pclose. */
		while (!feof(dmidecode_pipe))
			getc(dmidecode_pipe);
		if (pclose(dmidecode_pipe) != 0)
		{
			printf_debug("DMI pipe close error\n");
			goto out_free;
		}

		/* chomp trailing newline */
		if (answerbuf[0] != 0 &&
		    answerbuf[strlen(answerbuf) - 1] == '\n')
			answerbuf[strlen(answerbuf) - 1] = 0;
		printf_debug("DMI string %d: \"%s\"\n", i, answerbuf);
		dmistrings[i] = strdup(answerbuf);
	}
	has_dmi_support = 1;
out_free:
	free(answerbuf);
}

/**
 * Does an substring/prefix/postfix/whole-string match.
 *
 * The pattern is matched as-is. The only metacharacters supported are '^'
 * at the beginning and '$' at the end. So you can look for "^prefix",
 * "suffix$", "substring" or "^complete string$".
 *
 * @param value The string to check.
 * @param pattern The pattern.
 * @return Nonzero if pattern matches.
 */
static int dmi_compare(const char *value, const char *pattern)
{
	int anchored = 0;
	int patternlen;
	printf_debug("matching %s against %s\n", value, pattern);
	/* The empty string is part of all strings */
	if (pattern[0] == 0)
		return 1;

	if (pattern[0] == '^') {
		anchored = 1;
		pattern++;
	}

	patternlen = strlen(pattern);
	if (pattern[patternlen - 1] == '$') {
		int valuelen = strlen(value);
		patternlen--;
		if(patternlen > valuelen)
			return 0;

		/* full string match: require same length */
		if(anchored && (valuelen != patternlen))
			return 0;

		/* start character to make ends match */
		value += valuelen - patternlen;
		anchored = 1;
	}

	if (anchored)
		return strncmp(value, pattern, patternlen) == 0;
	else
		return strstr(value, pattern) != NULL;
}

int dmi_match(const char *pattern)
{
	int i;
	if (!has_dmi_support)
		return 0;

	for (i = 0;i < DMI_ID_INVALID; i++)
		if(dmi_compare(dmistrings[i], pattern))
			return 1;

	return 0;
}