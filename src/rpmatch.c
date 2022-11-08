/***********************************************************************************
 * Copyright (c) 2016, Jürgen Buchmüller
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***********************************************************************************/

#include "config.h"
#include "rpmatch.h"

#if HAVE_STDIO_H
#include <stdio.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#endif
#if HAVE_REGEX_H
#include <regex.h>
#endif
#if HAVE_LIBINTL_H
#include <libintl.h>
#endif
#if HAVE_LOCALE_H
#include <locale.h>
#endif

/**
 * The compare_t structure describes one entry in a list of english
 * strings with their (unitialized) translation, where the strings
 * may be regular expressions. The value to return if the pattern
 * matches is also defined here.
 */
typedef struct {
	int value;
        const char* string;
	const char* pattern;
	regex_t re;
}	compare_t;

static compare_t list[] = {
	{ 0, "no",    NULL, },
	{ 1, "yes",   NULL, },
	{ 0, "^[nN]", NULL, },
	{ 1, "^[yY]", NULL, },
	{ 0, NULL,    NULL, }
};

/**
 * Translate a compare_t string to pattern and see
 * if it changed or is new for this compare_t, in which
 * case the coressponding regular expression is compiled
 * and stored in the struct for further use.
 *
 * Finally use regexec() to compare the response.
 */
static int re_compare(const char* response, compare_t* compare)
{
        const char* pattern = gettext(compare->string);

	if (compare->pattern != pattern) {
		if (compare->pattern)
			regfree(&compare->re);
		compare->pattern = NULL;
		if (regcomp(&compare->re, pattern, REG_EXTENDED))
			return -1;
		compare->pattern = pattern;
	}
	return regexec(&compare->re, response, 0, NULL, 0) ? -1 : compare->value;
}

int rpmatch(const char* response)
{
	int result = -1;
	compare_t *compare = list;

	if (!response)
		return -1;
        while (compare->string) {
		result = re_compare(response, compare);
		if (result >= 0)
			return result;
		compare++;
	}
	return result;
}

#if defined(TEST)
int main(int argc, char** argv)
{
	if (argc < 2 || strcmp(argv[1], "--help") == 0) {
		fprintf(stderr, "%s [--lang] response\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	setlocale(LC_ALL, "");
	printf("rpmatch(\"%s\") returns: %d\n", argv[1], rpmatch(argv[1]));
	exit(EXIT_SUCCESS);
}
#endif
