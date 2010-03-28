/* Eiwics most complex module */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eiwic.h"
#include "plugins.h"

EP_SETNAME("template");

int funny(char *param, OUTPUT *out, MSG *msg)
{
	if (param != NULL)
	{
		eiwic->output_printf(out, "LOL, %s is so funny. :)\n", param);
	}

	return 1;
}

int ep_help(OUTPUT *out)
{
	eiwic->output_print(out, "Template is just a template to write your own modules.\n");
	eiwic->output_print(out, "Try: !funny <something>\n");
}

int ep_unload(OUTPUT *out)
{
	return 1;
}

int ep_main(OUTPUT *out)
{
	eiwic->plug_trigger_reg("!funny", TRIG_PUBLIC, funny);
	eiwic->output_print(out, "Template module is running.\n");
	return 1;
}
