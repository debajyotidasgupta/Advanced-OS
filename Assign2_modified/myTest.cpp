#include <bits/stdc++.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <wait.h>
#include <semaphore.h>

using namespace std;

int main(int argc, char *argv[])
{

    pid_t pid;
    for (int i = 0; i < stoi(argv[1]); i++)
        pid = fork();

    vector<vector<int>> a = {
        {1024376998, 731027477, 902031461},
        {-90876909, -190985907, -8685909, 120985907},
        {-123456789, -987654321, 100456780, 98005430}};

    string file = "/proc/cs60038_a2_" + string(argv[2]);

    if (pid == 0)
    {
        int fd = open(file.c_str(), O_RDWR);
        deque<int> q;

        int len = 4;
        write(fd, (char *)&len, sizeof(int));
        for (int i = 0; i < 20; i++)
        {
            errno = 0;
            int v = a[getpid() % 3][i % a[getpid() % 3].size()];
            if (q.size() < len)
                v & 1 ? q.push_front(v) : q.push_back(v);
            int ret = write(fd, (char *)&v, sizeof(int));
            printf("Write [%d] : Num [%d] Err[%d] ret[%d]\n", getpid(), v, errno, ret);
            fflush(stdout);
            usleep(500);
        }

        printf("[%d] : [", getpid());
        for (auto &i : q)
            printf("%d ", i);
        printf("]\n");
        fflush(stdout);

        for (int i = 0; i < 20; i++)
        {
            errno = 0;
            int v = (q.empty() ? -1 : q.front()), u;
            if (!q.empty())
                q.pop_front();
            int ret = read(fd, (char *)&u, sizeof(int));
            printf("Read  [%d] : Expected [%d] Found [%d] Err [%d] ret[%d]\n", getpid(), v, u, errno, ret);
            fflush(stdout);

            if (!errno and u != v)
            {
                printf("\033[1;31mError Reading PID : [%d]\033[0m\n", getpid());
                fflush(stdout);
            }

            usleep(500);
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