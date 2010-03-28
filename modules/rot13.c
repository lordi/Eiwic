/* Tiny Rot13 Module */
#include "eiwic.h"
#include "plugins.h"

EP_SETNAME("rot13");

/* Algo taken from */
/* http://ucsub.colorado.edu/~kominek/rot13/c/rot13.c */
int rot13(char *param, OUTPUT *out, MSG *msg)
{
	register char cap;
	char *p;

	if (!param)
	{
		eiwic->output_printf(out, "usage: !rot13 <string>\n");
		return 0;
	}

	for(p=param;*p;++p)
	{
		cap = *p & 32;
		*p &= ~cap;
		*p = ((*p >= 'A') && (*p <= 'Z') ? 
				((*p - 'A' + 13) % 26 + 'A') : *p)
				 | cap;
	}		

	eiwic->output_printf(out, "rot13 result: %s\n", param);
	return 1;
}

int ep_help(OUTPUT *out)
{
	eiwic->output_print(out, "Rot13 is a simple module which offers the trigger !rot13.\n");
	return 1;
}

int ep_main(OUTPUT *out)
{
	eiwic->plug_trigger_reg("!rot13", TRIG_PUBLIC, rot13);
	return 1;
}

