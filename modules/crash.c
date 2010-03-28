#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eiwic.h"
#include "plugins.h"

EP_SETNAME("crash");

int crash(char *param, OUTPUT *out, MSG *msg)
{
	char *x;
	x = NULL;
	*x = 42;
	return 1;
}

int outputtest(char *param, OUTPUT *out, MSG *msg)
{
	eiwic->output_print(out, "AAAAAAAA AAAAAAAA AAAAAAAA AAAAAAAA ");
	eiwic->output_print(out, "AAAAAAAA AAAAAAAA AAAAAAAA AAAAAAAA ");
	eiwic->output_print(out, "AAAAAAAA AAAAAAAA AAAAAAAA AAAAAAAA ");
	eiwic->output_print(out, "AAAAAAAA AAAAAAAA AAAAAAAA AAAAAAAA ");

	eiwic->output_print(out, "AAAAAAAA AAAAAAAA AAAAAAAA AAAAAAAA ");
	eiwic->output_print(out, "AAAAAAAA AAAAAAAA AAAAAAAA AAAAAAAA ");
	eiwic->output_print(out, "AAAAAAAA AAAAAAAA AAAAAAAA AAAAAAAA ");
	eiwic->output_print(out, "AAAAAAAA AAAAAAAA AAAAAAAA AAAAAAAA ");

	eiwic->output_print(out, "AAAAAAAA AAAAAAAA AAAAAAAA AAAAAAAA ");
	eiwic->output_print(out, "AAAAAAAA AAAAAAAA AAAAAAAA AAAAAAAA ");

	eiwic->output_print(out, "\n");

	eiwic->output_print(out, "BBBBBBBB BBBBBBBB BBBBBBBB BBBBBBBB\n");
	eiwic->output_print(out, "BBBBBBBB BBBBBBBB BBBBBBBB BBBBBBBB\nBBBBBBBB ");
	eiwic->output_print(out, "BBBBBBBB BBBBBBBB BBBBBBBB BBBBBBBB\n");
	eiwic->output_print(out, "BBBBBBBB BBBBBBBB BBBBBBBB BBBBBBBB\nBBBBBBBB ");

	eiwic->output_print(out, "\n");
}

int ep_main(OUTPUT *out)
{
	eiwic->plug_trigger_reg("!outputtest", TRIG_ADMIN, outputtest);
	eiwic->plug_trigger_reg("!crash", TRIG_ADMIN, crash);
	return 1;
}

