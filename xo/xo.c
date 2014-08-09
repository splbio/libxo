/*
 * Copyright (c) 2014, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 * Phil Shafer, July 2014
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "xo.h"
#include "xoversion.h"

#ifndef UNUSED
#define UNUSED __attribute__ ((__unused__))
#endif /* UNUSED */

#ifndef HAVE_STREQ
static inline int
streq (const char *red, const char *blue)
{
    return (red && blue && *red == *blue && strcmp(red + 1, blue + 1) == 0);
}
#endif /* HAVE_STREQ */

static int opt_warn;		/* Enable warnings */

static char *
check_arg (const char *name, char ***argvp)
{
    char *opt = NULL, *arg;

    opt = **argvp;
    *argvp += 1;
    arg = **argvp;

    if (arg == NULL) {
	xo_error("missing %s argument for '%s' option", name, opt);
	exit(1);
    }

    return arg;
}

static char **save_argv;
static char **checkpoint_argv;

static char *
next_arg (void)
{
    char *cp = *save_argv;

    if (cp == NULL) {
	xo_error("missing argument\n");
	exit(1);
    }

    save_argv += 1;
    return cp;
}

static void
prep_arg (char *fmt)
{
    char *cp, *fp;

    for (cp = fp = fmt; *cp; cp++, fp++) {
	if (*cp != '\\') {
	    if (cp != fp)
		*fp = *cp;
	    continue;
	}

	switch (*++cp) {
	case 'n':
	    *fp = '\n';
	    break;

	case 'r':
	    *fp = '\r';
	    break;

	case 'b':
	    *fp = '\b';
	    break;

	case 'e':
	    *fp = '\e';
	    break;

	default:
	    *fp = *cp;
	}
    }

    *fp = '\0';
}

static void
checkpoint (xo_handle_t *xop UNUSED, va_list vap UNUSED, int restore)
{
    if (restore)
	save_argv = checkpoint_argv;
    else
	checkpoint_argv = save_argv;
}

/*
 * Our custom formatter is responsible for combining format string pieces
 * with our command line arguments to build strings.  This involves faking
 * some printf-style logic.
 */
static int
formatter (xo_handle_t *xop, char *buf, int bufsiz,
	   const char *fmt, va_list vap UNUSED)
{
    int lflag = 0, hflag = 0, jflag = 0, tflag = 0,
	zflag = 0, qflag = 0, star1 = 0, star2 = 0;
    int rc = 0;
    int w1 = 0, w2 = 0;
    const char *cp;

    for (cp = fmt + 1; *cp; cp++) {
	if (*cp == 'l')
	    lflag += 1;
	else if (*cp == 'h')
	    hflag += 1;
	else if (*cp == 'j')
	    jflag += 1;
	else if (*cp == 't')
	    tflag += 1;
	else if (*cp == 'z')
	    zflag += 1;
	else if (*cp == 'q')
	    qflag += 1;
	else if (*cp == '*') {
	    if (star1 == 0)
		star1 = 1;
	    else
		star2 = 1;
	} else if (strchr("diouxXDOUeEfFgGaAcCsSp", *cp) != NULL)
	    break;
	else if (*cp == 'n' || *cp == 'v') {
	    if (opt_warn)
		xo_error_h(xop, "unsupported format: '%s'", fmt);
	    return -1;
	}
    }

    char fc = *cp;

    /* Handle "%*.*s" */
    if (star1)
	w1 = strtol(next_arg(), NULL, 0);
    if (star2 > 1)
	w2 = strtol(next_arg(), NULL, 0);

    if (fc == 'D' || fc == 'O' || fc == 'U')
	lflag = 1;

    if (strchr("diD", fc) != NULL) {
	long long value = strtoll(next_arg(), NULL, 0);
	if (star1 && star2)
	    rc = snprintf(buf, bufsiz, fmt, w1, w2, value);
	else if (star1)
	    rc = snprintf(buf, bufsiz, fmt, w1, value);
	else
	    rc = snprintf(buf, bufsiz, fmt, value);

    } else if (strchr("ouxXOUp", fc) != NULL) {
	unsigned long long value = strtoull(next_arg(), NULL, 0);
	if (star1 && star2)
	    rc = snprintf(buf, bufsiz, fmt, w1, w2, value);
	else if (star1)
	    rc = snprintf(buf, bufsiz, fmt, w1, value);
	else
	    rc = snprintf(buf, bufsiz, fmt, value);

    } else if (strchr("eEfFgGaA", fc) != NULL) {
	double value = strtold(next_arg(), NULL);
	if (star1 && star2)
	    rc = snprintf(buf, bufsiz, fmt, w1, w2, value);
	else if (star1)
	    rc = snprintf(buf, bufsiz, fmt, w1, value);
	else
	    rc = snprintf(buf, bufsiz, fmt, value);

    } else if (fc == 'C' || fc == 'c' || fc == 'S' || fc == 's') {
	char *value = next_arg();
	if (star1 && star2)
	    rc = snprintf(buf, bufsiz, fmt, w1, w2, value);
	else if (star1)
	    rc = snprintf(buf, bufsiz, fmt, w1, value);
	else
	    rc = snprintf(buf, bufsiz, fmt, value);
    }

    return rc;
}

static void
print_version (void)
{
    fprintf(stderr, "libxo version %s%s\n",
	    LIBXO_VERSION, LIBXO_VERSION_EXTRA);
}

static void
print_help (void)
{
    fprintf(stderr,
"Usage: xo [options] format [fields]\n"
"    --close <path>        Close tags for the given path\n"
"    --depth <num>         Set the depth for pretty printing\n"
"    --help                Display this help text\n"
"    --html OR -H          Generate HTML output\n"
"    --json OR -J          Generate JSON output\n"
"    --leading-xpath <path> OR -l <path> "
	    "Add a prefix to generated XPaths (HTML)\n"
"    --open <path>         Open tags for the given path\n"
"    --pretty OR -p        Make 'pretty' output (add indent, newlines)\n"
"    --style <style> OR -s <style>  "
	    "Generate given style (xml, json, text, html)\n"
"    --text OR -T          Generate text output (the default style)\n"
"    --version             Display version information\n"
"    --warn OR -W          Display warnings in text on stderr\n"
"    --warn-xml            Display warnings in xml on stdout\n"
"    --wrap <path>         Wrap output in a set of containers\n"
"    --xml OR -X           Generate XML output\n"
"    --xpath               Add XPath data to HTML output\n");
}

int
main (int argc UNUSED, char **argv)
{
    char *fmt = NULL, *cp, *np;
    char *opt_opener = NULL, *opt_closer = NULL, *opt_wrapper = NULL;
    int opt_depth = 0;
    int opt_not_first = 0;

    for (argv++; *argv; argv++) {
	cp = *argv;

	if (*cp != '-')
	    break;

	if (streq(cp, "--"))
	    break;

	if (streq(cp, "--close") || streq(cp, "-c")) {
	    opt_closer = check_arg("open", &argv);
	    xo_set_flags(NULL, XOF_IGNORE_CLOSE);

	} else if (streq(cp, "--depth")) {
	    opt_depth = atoi(check_arg("open", &argv));

	} else if (streq(cp, "--help")) {
	    print_help();
	    return 1;

	} else if (streq(cp, "--html") || streq(cp, "-H")) {
	    xo_set_style(NULL, XO_STYLE_HTML);

	} else if (streq(cp, "--json") || streq(cp, "-J")) {
	    xo_set_style(NULL, XO_STYLE_JSON);

	} else if (streq(cp, "--leading-xpath") || streq(cp, "-l")) {
	    xo_set_leading_xpath(NULL, check_arg("leading xpath", &argv));

	} else if (streq(cp, "--not-first") || streq(cp, "-N")) {
	    opt_not_first = 1;

	} else if (streq(cp, "--open") || streq(cp, "-o")) {
	    opt_opener = check_arg("close", &argv);

        } else if (streq(cp, "--pretty") || streq(cp, "-p")) {
	    xo_set_flags(NULL, XOF_PRETTY);

	} else if (streq(cp, "--style") || streq(cp, "-s")) {
	    np = check_arg("style", &argv);

	    if (xo_set_style_name(NULL, np) < 0) {
		xo_error("unknown style: %s", np);
		exit(1);
	    }

	} else if (streq(cp, "--text") || streq(cp, "-T")) {
	    xo_set_style(NULL, XO_STYLE_TEXT);

	} else if (streq(cp, "--xml") || streq(cp, "-X")) {
	    xo_set_style(NULL, XO_STYLE_XML);

	} else if (streq(cp, "--xpath")) {
	    xo_set_flags(NULL, XOF_XPATH);

	} else if (streq(cp, "--version")) {
	    print_version();
	    return 0;

	} else if (streq(cp, "--warn") || streq(cp, "-W")) {
	    opt_warn = 1;
	    xo_set_flags(NULL, XOF_WARN);

	} else if (streq(cp, "--warn-xml")) {
	    opt_warn = 1;
	    xo_set_flags(NULL, XOF_WARN_XML);

	} else if (streq(cp, "--wrap") || streq(cp, "-w")) {
	    opt_wrapper = check_arg("wrapper", &argv);
	}
    }

    xo_set_formatter(NULL, formatter, checkpoint);
    xo_set_flags(NULL, XOF_NO_VA_ARG);
    xo_set_flags(NULL, XOF_NO_TOP);

    if (opt_not_first)
	xo_set_flags(NULL, XOF_NOT_FIRST);

    if (opt_closer) {
	opt_depth += 1;
	for (cp = opt_closer; cp && *cp; cp = np) {
	    np = strchr(cp, '/');
	    if (np == NULL)
		break;
	    np += 1;
	    opt_depth += 1;
	}
    }

    if (opt_depth > 0)
	xo_set_depth(NULL, opt_depth);

    if (opt_opener) {
	for (cp = opt_opener; cp && *cp; cp = np) {
	    np = strchr(cp, '/');
	    if (np)
		*np = '\0';
	    xo_open_container(cp);
	    if (np)
		*np++ = '/';
	}
    }

    if (opt_wrapper) {
	for (cp = opt_wrapper; cp && *cp; cp = np) {
	    np = strchr(cp, '/');
	    if (np)
		*np = '\0';
	    xo_open_container(cp);
	    if (np)
		*np++ = '/';
	}
    }

    fmt = *argv++;
    if (fmt && *fmt) {
	save_argv = argv;
	prep_arg(fmt);
	xo_emit(fmt);
    }

    while (opt_wrapper) {
	np = strrchr(opt_wrapper, '/');
	xo_close_container(np ? np + 1 : opt_wrapper);
	if (np)
	    *np = '\0';
	else
	    opt_wrapper = NULL;
    }

    while (opt_closer) {
	np = strrchr(opt_closer, '/');
	xo_close_container(np ? np + 1 : opt_closer);
	if (np)
	    *np = '\0';
	else
	    opt_closer = NULL;
    }

    xo_finish();

    return 0;
}
