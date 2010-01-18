#include "audit.h"

static int call_depth;

int
audit_this_function(const char *name)
{
	return 1;
}

int
pre_func_audit(const char *name, unsigned long *args, unsigned long *res)
{
	put_n_strs(call_depth, "    ");
	putstr(name);
	putstr("(");
	if (!strcmp(name, "__asprintf_chk")) {
		putint(args[0]);
		putstr(", ");
		putint(args[1]);
		putstr(", ");
		putstr((const char *)args[2]);
	} else if (!strcmp(name, "realloc")) {
		putint(args[0]);
		putstr(", ");
		putint(args[1]);
	} else if (!strcmp(name, "fopen64")) {
		putstr((const char *)args[0]);
		putstr(", ");
		putstr((const char *)args[1]);
	} else if (!strcmp(name, "malloc") ||
		   !strcmp(name, "free")) {
		putint(args[0]);
	} else if (!strcmp(name, "getenv")) {
		putstr((const char *)args[0]);
	}
	putstr(");\n");
	call_depth++;
	return 0;
}

void
post_func_audit(const char *name, unsigned long *rv)
{
	put_n_strs(call_depth, "    ");
	putstr(" -> ");
	if (!strcmp(name, "free"))
		putstr("<void>");
	else
		putint(rv[0]);
	putstr("\n");
	call_depth--;
}

