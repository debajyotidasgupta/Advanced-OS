#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/types.h>

int main()
{
    int fd, fd2, pid;
    char filename[100] = "/proc/partb_1_18CS30051";
    pid = fork();
    fork(); //uncomment this to check for >2 processes
    if (pid == 0)
    {
        printf("%d Opening File\n", getpid());
        fd = open(filename, O_RDWR);
        if (fd < 0)
        {
            printf("%d Cannot open file...\n", getpid());
            printf("%d Error= %d\n", getpid(), errno);
            return 0;
        }
        fd2 = open(filename, O_RDWR);
        //check reopening for file
        if (fd2 < 0)
        {
            printf("%d Cannot open file again...\n", getpid());
            printf("%d Error= %d\n", getpid(), errno);
            printf("%d fd2=%d\n", getpid(), fd2);
        }
        //7 9 10 12-> write order
        //9 7 10 12-> expected deque structure
        int a = 4;
        u_int8_t b = 4;
        int x;
        if ((x = write(fd, &b, sizeof(b))) < 0)
        {
            printf("%d error %d\n", getpid(), errno);
            return 0;
        }
        a = 7;
        x = write(fd, &a, sizeof(int));
        a = 9;
        x = write(fd, &a, sizeof(int));
        //sleep(2); //uncomment this for proper printing
        a = 10;
        x = write(fd, &a, sizeof(int));
        a = 12;
        x = write(fd, &a, sizeof(int));

        //to check for error on excess write
        x = write(fd, &a, sizeof(int));
        if (x < 0)
        {
            printf("%d performed excess write\n", getpid());
        }

        read(fd, &a, sizeof(int));
        printf("%d read %d expected 9\n", getpid(), a); //expected 9
        read(fd, &a, sizeof(int));
        printf("%d read %d expected 7\n", getpid(), a); //expected 7
        //sleep(2); //uncomment this for proper printing
        read(fd, &a, sizeof(int));
        printf("%d read %d expected 10\n", getpid(), a); //expected 10
        read(fd, &a, sizeof(int));
        printf("%d read %d expected 12\n", getpid(), a); //expected 12

        printf("%d Closing file\n", getpid());
        close(fd);

        //check if we can reopen after close
        fd = open(filename, O_RDWR);
        if (fd >= 0)
        {
            printf("%d successfully reopened after close\n", getpid());
            close(fd);
        }
        else
        {
            printf("%d could not reopen after close\n", getpid());
        }
        return 0;
    }
    else if (pid > 0)
    {
        printf("%d Opening File\n", getpid());
        fd = open(filename, O_RDWR);
        if (fd < 0)
        {
            printf("%d Cannot open file...\n", getpid());
            printf("%d Error= %d\n", getpid(), errno);
            return 0;
        }
        int a = 4;
        u_int8_t b = 4;
        int x;
        char str[4] = "abc";
        if ((x = write(fd, &b, sizeof(b))) < 0)
        {
            printf("%d Size error %d\n", getpid(), errno);
            return 0;
        }
        //this should return error on writing 3 bytes
        if (x = write(fd, str, strlen(str)) < 0)
        {
            printf("%d %d write error on string of length 3\n", getpid(), errno);
        }
        //11 19 20 17 -> write order
        //19 11 17 20 -> expected read order
        a = 11;
        x = write(fd, &a, sizeof(int));
        a = 19;
        x = write(fd, &a, sizeof(int));
        //sleep(2); //uncomment this for proper printing
        a = 20;
        x = write(fd, &a, sizeof(int));

        read(fd, &a, sizeof(int));
        printf("%d read %d expected 19\n", getpid(), a);
        read(fd, &a, sizeof(int));
        printf("%d read %d expected 11\n", getpid(), a);

        a = 17;
        x = write(fd, &a, sizeof(int));
        //sleep(2); //uncomment this for proper printing
        read(fd, &a, sizeof(int));
        printf("%d read %d expected 17\n", getpid(), a);
        read(fd, &a, sizeof(int));
        printf("%d read %d expected 20\n", getpid(), a);

        //to check for error on excess read
        x = read(fd, &a, sizeof(int));
        if (x < 0)
        {
            printf("%d performed excess read\n", getpid());
        }

        printf("%d Closing file\n", getpid());
        close(fd);

        return 0;
    }
    return 0;
}