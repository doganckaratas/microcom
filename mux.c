/***************************************************************************
** File: mux.c
** Description: the main program loop
**
** Copyright (C)1999 Anca and Lucian Jurubita <ljurubita@hotmail.com>.
** All rights reserved.
****************************************************************************
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details at www.gnu.org
****************************************************************************
** Rev. 1.0 - Feb. 2000
****************************************************************************/
#include "microcom.h"

#define SCRIPT_DELAY 1
#define BUFSIZE 1024

extern int script;
extern char scr_name[];
extern FILE* flog;
extern unsigned int options;

void mux_clear_sflag(void)
{
	script = 0;
}

static inline char *PrintableBuffer(const char *buffer,const size_t l, size_t *size)
{
	static char printable[BUFSIZE+1];
	const char *limit = buffer + l;
	const char *i = buffer;
	char *j = printable;
	for(i=buffer; i<limit; i++) {
		if (isprint(*i)) {
			*j = *i;
			j++;
		}
	}
	*size = j - printable;
	*j = '\0';
	return printable;
}

int logger_dump_with_timestamp(FILE *stream, char *buffer, int length, enum logger_timestamp_mode m)
{
	int char_pos = 0, ret = 0;
	time_t raw;
	struct tm *t;
	struct timeval tv;
	time(&raw);
	t = localtime(&raw);
	gettimeofday(&tv, NULL);


	for (char_pos = 0; char_pos < length; char_pos++) {
		/* TODO: fix timestamp later. */
		switch(buffer[char_pos]) {
		case '\n':
			if (m == LOGGER_TIMESTAMP_COMPLEX)
			ret += fprintf(stream, "\n[%04d-%02d-%02d %02d:%02d:%02d.%03i] ", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, tv.tv_usec / 1000);
			else if (m == LOGGER_TIMESTAMP_SIMPLE)
			ret += fprintf(stream, "\n[%02d:%02d:%02d] ", t->tm_hour, t->tm_min, t->tm_sec);
			else
			ret += fprintf(stream, "\n");
			break;
		default:
			ret += fprintf(stream, "%c", buffer[char_pos]);
		}
	}

	return ret;
}

/* main program loop */
void mux_loop(int pf)
{
	fd_set ready;	/* used for select */
	int i = 0;	/* used in the multiplex loop */
	// int char_pos = 0;
	int done = 0;
	char buf[BUFSIZE];
	// time_t raw;
	// struct tm *t;
	struct timeval tv;

	tv.tv_sec = SCRIPT_DELAY;
	tv.tv_usec = 0;

	if (script) {
		script_init(scr_name);
	}

	do { /* forever */
		FD_ZERO(&ready);
		FD_SET(STDIN_FILENO, &ready);
		FD_SET(pf, &ready);

		if (script) {
			if (!select(pf+1, &ready, NULL, NULL, &tv)) {
				i = script_process(S_TIMEOUT, buf, BUFSIZE);
				if (i > 0) {
					cook_buf(pf, buf, i);
				}
				/* restart timer */
				tv.tv_sec = SCRIPT_DELAY;
				tv.tv_usec = 0;
			}
		} /* if */
		else {
			select(pf+1, &ready, NULL, NULL, NULL);
		}

		if (FD_ISSET(pf, &ready)) {
			/* pf has characters for us */
			i = read(pf, buf, BUFSIZE);
			// time(&raw);
			// t = localtime(&raw);
			if (i > 0) {
				if (options & OPTION_LOG_FILTER) {
					/* only printable characters */
					size_t size = 0;
					char *printable = PrintableBuffer(buf,i,&size);
					DEBUG_MSG("received printable buffer = %s",printable);
					if (flog != NULL) {
						printable[size] = '\n';
						printable[size+1] = '\0';
						const size_t written = logger_dump_with_timestamp(flog, printable, size, logger_timestamp_e);
						if (written != size) {
							const int error = errno;
							DEBUG_MSG("error writing log file, only %u characters written, errno = %d",written,error);
						}
					}
				} else {
					/* raw memory dump */
					DEBUG_DUMP_MEMORY(buf,i);
					if (flog != 0) {
						logger_dump_with_timestamp(flog, buf, i, logger_timestamp_e);
					}
				}

				/* XXX: DO NOT use stdout, hence it is buffered. */
				logger_dump_with_timestamp(stderr, buf, i, console_timestamp_e);

				if (script) {
					i = script_process(S_DCE, buf, i);
					if (i > 0) {
						cook_buf(pf, buf, i);
					}
				}
			} else {
				done = 1;
			}
		} /* if */

		if (FD_ISSET(STDIN_FILENO, &ready)) {
			/* standard input has characters for us */
			i = read(STDIN_FILENO, buf, BUFSIZE);
			if (i > 0) {
				cook_buf(pf, buf, i);
			} else {
				done = 1;
			}
		} /* if */
	} while (!done); /* do */
}
