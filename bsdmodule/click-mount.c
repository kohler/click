#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/mount.h>

int
main(void)
{
    int ret;

    ret = mount("click", "/click", 0, 0);
    if (ret)
	perror("mounting");

    return 0;
}
