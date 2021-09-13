#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <wait.h>

int proc_main(int *entries, int n, int proc)
{
    int fd = open("/proc/partb_1_18CS30051", O_RDWR);
    int out = -1;

    char c = (char)n;
    write(fd, &c, sizeof(char));

    for (int i = 0; i <= n; i++)
    {
        errno = 0;
        int ret = write(fd, &entries[i], sizeof(int));
        printf("[Proc %d] Write: %d %d %d\n", proc, getpid(), ret, errno);
        usleep(100);
    }

    for (int i = 0; i <= n; i++)
    {
        errno = 0;
        int ret = read(fd, &out, sizeof(int));
        printf("[Proc %d] Read : %d %d %d %d\n", proc, getpid(), out, ret, errno);
        entries[i] = out;
        usleep(100);
    }

    close(fd);

    return 0;
}

int main()
{
    int parent[] = {3, 2, 5};
    int child[] = {1, 2, 3, 4};

    int pid = fork();

    if (pid != 0)
    {
        pid = fork();

        if (pid != 0)
        {
            proc_main(parent, 3, 1);

            int status;
            wait(&status);

            printf("Proc1: [ ");
            for (int i = 0; i < sizeof(parent) / sizeof(int); i++)
            {
                printf("%d ", parent[i]);
            }
            printf("]\n");
        }
        else
        {
            proc_main(child, 4, 2);
            printf("Proc2: [ ");
            for (int i = 0; i < sizeof(child) / sizeof(int); i++)
            {
                printf("%d ", child[i]);
            }
            printf("]\n");
        }
    }
    else
    {
        pid = fork();

        if (pid != 0)
        {
            proc_main(parent, 3, 3);

            printf("Proc3: [ ");
            for (int i = 0; i < sizeof(parent) / sizeof(int); i++)
            {
                printf("%d ", parent[i]);
            }
            printf("]\n");
        }
        else
        {
            proc_main(child, 4, 4);
            printf("Proc4: [ ");
            for (int i = 0; i < sizeof(child) / sizeof(int); i++)
            {
                printf("%d ", child[i]);
            }
            printf("]\n");
        }
    }
    return 0;
}