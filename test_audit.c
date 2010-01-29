#include <stddef.h>

#include "audit.h"

static int call_depth;
static int need_newline;

int
audit_this_function(const char *name)
{
	return 1;
}

#define RET_TYPE_VOID 'v'
#define RET_TYPE_DEC_INT 'i'
#define RET_TYPE_HEX_INT 'h'
#define RET_TYPE_CHAR 'c'
#define RET_TYPE_STRING 's'

struct function_prototype {
	const char *name;
	const char *args;
	const char ret_type;
};

static struct function_prototype funcs[] = {
{ "realloc", "hi", 'i' },
{ "malloc", "i", 'h' },
{ "calloc", "ii", 'h' },
{ "free", "h", 'v' },

{ "fopen64", "ss", 'h' },
{ "open", "sii", 'i' },
{ "statfs64", "sh", 'i' },
{ "isatty", "i", 'i' },
{ "opendir", "s", 'h' },
{ "readdir", "h", 'h' },
{ "closedir", "h", 'i' },
{ "fchdir", "i", 'i' },

{ "getenv", "s", 's' },
{ "getrlimit", "ih", 'i' },
{ "sysconf", "i", 'i' },
{ "__sysconf", "i", 'i' },

{ "__asprintf_chk", "his", 'i' },
{ "__getdelim", "hhch", 'i' },
{ "strchr", "sc", 's' },
{ "strrchr", "sc", 's' },
{ "strlen", "s", 'i' },
{ "memcpy", "hhi", 'v' },
{ "memmove", "hhi", 'v' },
{ "strcoll", "ss", 'i' },
{ "strcmp", "ss", 'i' },

{ "getopt_long", "ihshh", 'c' },

{ "setlocale", "is", 's' },
{ "bindtextdomain", "ss", 's' },
{ "textdomain", "s", 's' },
{ "dcgettext", "ssi", 's' },
};

static struct function_prototype *
find_function(const char *name)
{
	unsigned x;
	for (x = 0; x < sizeof(funcs) / sizeof(funcs[0]); x++)
		if (!strcmp(name, funcs[x].name))
			return &funcs[x];
	return NULL;
}

static void
putarg(unsigned long val, unsigned char typ)
{
	switch (typ) {
	case 'v':
		putstr("void");
		break;
	case 'i':
		putdecint(val);
		break;
	case 'h':
		putstr("0x");
		putint(val);
		break;
	case 'c': {
		char c[2];
		putstr("'");
		c[0] = val;
		c[1] = 0;
		putstr(c);
		putstr("'");
		break;
	}
	case 's':
		if (val == 0) {
			putstr("<null>");
		} else {
			putstr("\"");
			putstr((const char *)val);
			putstr("\"");
		}
		break;
	}
}

int
pre_func_audit(const char *name, unsigned long *args, unsigned long *res,
	       unsigned long val)
{
	struct function_prototype *proto;
	unsigned x;

	if (need_newline) {
		putstr("\n");
	}

	need_newline = 1;

	put_n_strs(call_depth, "    ");
	putstr(name);
	putstr("(");

	call_depth++;
	proto = find_function(name);
	if (proto) {
		for (x = 0; proto->args[x]; x++) {
			putarg(args[x], proto->args[x]);
			if (proto->args[x+1])
				putstr(", ");
		}
		putstr(")");
		call_func(val, res, 266, args);
		if (proto->ret_type != RET_TYPE_VOID) {
			putstr(" -> ");
			putarg(res[0], proto->ret_type);
		}
		putstr(";\n");
		call_depth--;
		need_newline = 0;
		return 1;
	} else {
		putstr(");\n");
		need_newline = 0;
		return 0;
	}
}

void
post_func_audit(const char *name, unsigned long *rv)
{
	put_n_strs(call_depth, "    ");
	putstr(" -> ");
	putint(rv[0]);
	putstr("\n");
	call_depth--;
	need_newline = 0;
}

