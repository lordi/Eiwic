/* Eiwics decision module */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eiwic.h"
#include "plugins.h"

EP_SETNAME("decide");

void usage(OUTPUT *out)
{
	eiwic->output_printf(out, "usage: !decide option1 option2 option3 (...)\n");
}

int decide(char *param, OUTPUT *out, MSG *msg)
{
	if (param)
	{
		char option[21][40], *p;
		int options = 0,result;
		
		if (p = strtok(param, " "))
		do {
			strncpy(option[options], p, 40);
			option[options][40] = 0;
			options++;
		} while ((p = strtok(NULL, " ")) && options < 20);
		
		eiwic->log(LOG_DEBUG, "decide: Deciding from %d options", options);

		if (options > 1)
		{
			result = rand() % options;
			if (msg) eiwic->output_printf(out, "%s: ", msg->user->nick);
			eiwic->output_printf(out, "hmm... %s!\n", option[result]);
		}
		else
		{
			usage(out);
			
		}
	}
	else
	{
		usage(out);
	}

	return 1;
}

int ep_help(OUTPUT *out)
{
	eiwic->output_print(out, "Decide is a small fun module, so you don't have to waste your\n");
	eiwic->output_print(out, "brain energy anymore for decisions.\n");
	usage(out);
}

int ep_main(OUTPUT *out)
{
	eiwic->plug_trigger_reg("!decide", TRIG_PUBLIC, decide);
	return 1;
}

