#include <bits/stdc++.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <wait.h>

using namespace std;

int main(int argc, char *argv[])
{
    pid_t pid;
    for (int i = 0; i < stoi(argv[1]); i++)
        pid = fork();

    vector<vector<int>> a = {
        {1, 2, 3},
        {4, 5, 6, 7},
        {8, 9, 10, 11, 12},
        {13, 14, 15, 16, 17, 18},
    };

    string file = "/proc/partb_1_" + string(argv[2]);

    if (pid == 0)
    {
        int fd = open(file.c_str(), O_RDWR);
        deque<int> q;

        int len = 4;
        write(fd, (char *)&len, sizeof(char));
        for (int i = 0; i < 5; i++)
        {
            errno = 0;
            int v = a[getpid() % 4][i % a[getpid() % 4].size()];
            if (q.size() < len)
                v & 1 ? q.push_front(v) : q.push_back(v);
            write(fd, (char *)&v, sizeof(int));
            printf("Write [%d] : Num [%d] Err[%d]\n", getpid(), v, errno);
            fflush(stdout);
            usleep(100);
        }

        printf("[%d] : [", getpid());
        for (auto &i : q)
            printf("%d ", i);
        printf("]\n");
        fflush(stdout);

        for (int i = 0; i < 6; i++)
        {
            errno = 0;
            int v = (q.empty() ? -1 : q.front()), u;
            if (!q.empty())
                q.pop_front();
            read(fd, (char *)&u, sizeof(int));
            printf("Read  [%d] : Expected [%d] Found [%d] Err [%d]\n", getpid(), v, u, errno);
            fflush(stdout);

            if (!errno and u != v)
            {
                printf("\033[1;31mError Reading PID : [%d]\033[0m\n", getpid());
                fflush(stdout);
            }

            usleep(100);
        }

        close(fd);
    }
    else
    {
        waitpid(pid, NULL, 0);
        printf("Closed Process: [%u]\n", pid);
        fflush(stdout);
    }
}