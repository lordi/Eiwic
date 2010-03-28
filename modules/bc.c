/*
 * bc - bc module for eiwic
 *
 * by lordi@styleliga.org | http://lordi.styleliga.org/ 
 *
 * 'bc' is copyrighted 
 * 1991-1994, 1997, 1998, 2000 Free Software Foundation, Inc.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <ctype.h>
#include <dlfcn.h>
#include <time.h>

#include "eiwic.h"
#include "plugins.h"

EP_SETNAME("bc");

/* two defines which make the working with pipes more easier ;) */
#define R 0
#define W 1

#define BC_TIMEOUT 2 /* bc timeout in seconds */
#define BC_MAXLINES 4 /* lines of bc result that are printed out maximally */

/* function to be called on '!bc' or '!calc' */
int on_bc(char *a, OUTPUT *out, MSG *m)
{
	FILE *f;
	char buf[1025]; /* read buffer */
	int version = 0;
	int lines = 0, success = 0;
	int pid = 0; /* child pid */
	int fdin[2], fdout[2];	/* our pipes */
	fd_set readfd;
	struct timeval tv;

	if (a == NULL) /* if no parameter is given */
	{
		version = 1;
	}
	else
	{
		if (strchr(a, '\"') || strchr(a, '\\'))
			return 0;

		if (strcasecmp(a, "version") == 0)
			version = 1;
	}

	/* create the in and out pipes for the execution of 'bc' */
	if ((pipe(fdin) < 0) || (pipe(fdout) < 0))
	{
		eiwic->log(LOG_ERROR, "Unable to create pipe");
		return 0;
	}

	eiwic->log(LOG_DEBUG, "Forking..");

	if ((pid = fork()) == 0)
	{
		/* set up stdin/out/err and execute bc */
		
		close(fdout[R]);
		close(fdin[W]);
		
		dup2(fdin[R], STDIN_FILENO);
		dup2(fdout[W], STDOUT_FILENO);
		dup2(fdout[W], STDERR_FILENO);
		
		if (version)
			execlp("bc", "bc", "--version", NULL);
		else
			execlp("bc", "bc", "-q", "-l", NULL);

		/* we should never come here */

		close(fdout[W]);
		close(fdin[R]);
		
		exit(1);
	}

	if (pid == -1)
	{
		eiwic->log(LOG_ERROR, "fork() failed");
		return 0;
	}

	/* set up in and out pipes */
	close(fdout[W]);
	close(fdin[R]);

	/* write the parameter to bc */
	if (version == 0)
	{
		f = fdopen(fdin[W], "w");
		fprintf(f, "%s\n", a);
		fclose(f);
	}
	
	close(fdin[W]);

	if (fcntl(fdout[R], F_SETFL, O_NONBLOCK) == -1)
	{
		eiwic->log(LOG_ERROR, "fcntl() failed");
		return 0;
	}

	f = fdopen(fdout[R], "r");

	FD_ZERO(&readfd);
	FD_SET(fdout[R], &readfd);
	
	tv.tv_sec = BC_TIMEOUT;
	tv.tv_usec = 0;

	/* select loop */
	while (select(FD_SETSIZE, &readfd, NULL, NULL, &tv) > 0 && lines < BC_MAXLINES)
	{
		if (FD_ISSET(fdout[R], &readfd))
		{
			if (fgets(buf, 1024, f) == NULL)
			{
				success = 1;
				break;
			}

			if (lines == 0 && a != NULL)
			{
				eiwic->output_printf(out, "%s = %s\n", a, strtok(buf, "\r\n"));
			}
			else
			{
				eiwic->output_printf(out, "| %s\n", strtok(buf, "\r\n"));
			}
			
			lines++;
		}

		FD_ZERO(&readfd);
		FD_SET(fdout[R], &readfd);		

		tv.tv_sec = BC_TIMEOUT;
		tv.tv_usec = 0;
	}
	
	fclose(f);
	close(fdout[R]);

	eiwic->log(LOG_DEBUG, "Finished.");

	kill(pid, 9);
	waitpid(pid, NULL, 0);

	if (success == 0)
	{
		if (lines >= BC_MAXLINES)
		{
			eiwic->output_print(out, "(..)\n");
		}
		else
		{
			eiwic->output_print(out, "bc: Timeout waiting for a response :(\n");
		}
	}

	return 1;
}
	
int ep_help(OUTPUT *out)
{
	eiwic->output_print(out, "bc is a module which brings the output of the\n");
	eiwic->output_print(out, "GNU bc program to IRC. It can calculate complex\n");
	eiwic->output_print(out, "mathematical tasks.\n");
	eiwic->output_print(out, "More information: http://www.gnu.org/software/bc/\n");
	eiwic->output_print(out, "Just call '!bc <something>', where <something> is\n");
	eiwic->output_print(out, "a mathematical task (eg. '!bc (2.75/43)^8').\n");
}

int ep_main(OUTPUT *out)
{
	/* TODO: return 0; if command line tool 'bc' is not available */

	/* register triggers */
	eiwic->plug_trigger_reg("!bc", TRIG_PUBLIC, on_bc);

	return 1;
}
