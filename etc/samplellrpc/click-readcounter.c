/* -*- c-basic-offset: 4 -*- */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <inttypes.h>
#include <click/llrpc.h>

static void complain(const char *, char *);

static const char *clickfs_prefix;

int
main(int argc, const char *argv[])
{
    char *buf;
    FILE *f;
    int val = 0;

    /* This simple program demonstrates how to use ioctl() to access a Click
       kernel configuration's LLRPCs from user level. Most of its bulk is
       error checking code. */

    /* Check argument list */
    if (argc != 2 && argc != 3) {
	fprintf(stderr, "Usage: click-readcounter ELEMENTNAME [COUNTERID]\n");
	exit(1);
    } else if (argc == 3 && sscanf(argv[2], "%d", &val) != 1) {
	fprintf(stderr, "click-readcounter: Bad counter ID; expected an integer\nUsage: click-readcounter ELEMENTNAME [COUNTERID]\n");
	exit(1);
    }

    /* Find the Click prefix */
    if (access("/click/config", F_OK) >= 0)
	clickfs_prefix = "/click";
    else if (errno != ENOENT) {
	fprintf(stderr, "click-readcounter: /click/config: %s\n", strerror(errno));
	exit(1);
    } else if (access("/proc/click/config", F_OK) >= 0)
	clickfs_prefix = "/proc/click";
    else if (errno != ENOENT) {
	fprintf(stderr, "click-readcounter: /proc/click/config: %s\n", strerror(errno));
	exit(1);
    } else {
	fprintf(stderr, "click-readcounter: the Click file system does not exist\n  (Have you installed Click yet?)\n");
	exit(1);
    }
    
    /* Open the handler file `/click/ELEMENT/name'. */
    /* To call an LLRPC on some Click element, you must first open one of its
       handler files (to get a file descriptor you can ioctl() on). It
       currently doesn't matter which handler you choose, or what access mode
       you use. Every element has a `name' handler, so we open that. */
    buf = malloc(strlen(argv[1]) + 50);
    if (!buf)
	abort();
    sprintf(buf, "%s/%s/name", clickfs_prefix, argv[1]);
    f = fopen(buf, "r");
    if (!f) {
	/* Try to narrow down the error message. */
	complain(argv[1], buf);
	exit(1);
    }
    
    /* Now, we can actually make the ioctl()! */
    /* CLICK_LLRPC_GET_COUNT has the this specification: Its argument is a
       pointer to a 4-byte integer. The value of that integer identifies the
       counter to return; we default to 0, but let people specify which
       counter with an optional argument. (The Counter element supports two
       counter IDs: 0 is the packet count, 1 the byte count.) On return frm
       ioctl, the corresponding count is stored in the integer. */
    if (ioctl(fileno(f), CLICK_LLRPC_GET_COUNT, &val) < 0) {
	fprintf(stderr, "click-readcounter: `%s' llrpc: %s\n", argv[1], strerror(errno));
	exit(1);
    }

    /* Print the count we got! */
    printf("%d\n", val);
    exit(0);
}


static void
complain(const char *element, char *buf)
{
    int old_errno = errno;
    char *nbuf;
    
    /* Generate friendly error messages for common mistakes */
    buf = malloc(strlen(element) + 50);
    if (!buf)
	abort();

    /* Check /click/ELEMENT */
    nbuf = malloc(strlen(element) + 50);
    if (!nbuf)
	abort();
    sprintf(nbuf, "%s/%s", clickfs_prefix, element);
    if (access(nbuf, F_OK) < 0) {
	if (errno == ENOENT)
	    fprintf(stderr, "click-readcounter: `%s' does not exist\n  (Does the configuration have an element named `%s'?)\n", nbuf, element);
	else
	    fprintf(stderr, "click-readcounter: %s: %s\n", nbuf, strerror(errno));
	free(nbuf);
	return;
    }

    /* Otherwise, just report the original error */
    fprintf(stderr, "click-readcounter: %s: %s\n", buf, strerror(old_errno));
    free(nbuf);
}
