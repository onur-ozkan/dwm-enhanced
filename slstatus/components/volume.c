/* See LICENSE file for copyright and license details. */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../util.h"


const char *
vol_perc(void)
{
	char *cmd[3];
    cmd[0] = "amixer";
    cmd[1] = "get";
    cmd[2] = "Master";
    int pipe_fds[2],pid_amixer;
	int volume;
    char status[5]; // [on] - [off]

    pipe(pipe_fds);

    if ((pid_amixer=fork())==0)
    {
        close(pipe_fds[0]);
        dup2(pipe_fds[1], 1);
        execvp(cmd[0], cmd);
    }
    else
    {
        char buffer[1024];
        char *tok;
        const char *target="Front Left:";

        close(pipe_fds[1]);
        read(pipe_fds[0], buffer, 1024);
        close(pipe_fds[0]);

        if ((tok=strstr(buffer,target)))
        {
            sscanf(tok, "%*s %*s %*s %*s %*c%i%*s %5s", &volume, status);
        }
    }

    if (strcmp("[off]", status))
    {
        return bprintf("%s", "muted");
    }
    else
    {
        switch (volume)
        {
            case 0:
                return bprintf("%s", "muted");
                break;
            default:
                return bprintf("%d", volume);
                break;
        }
    }
}
