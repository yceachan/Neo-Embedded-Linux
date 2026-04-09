#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>
#include <poll.h>
#include <sys/epoll.h>
#include <time.h>
#include <sys/wait.h>

#define MSG_SIZE 16

long get_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void child_producer(int fd1, int fd2) {
    long start = get_ms();
    int count1 = 0, count2 = 0;
    while (1) {
        long now = get_ms() - start;
        if (now > 3000) break; // run for 3 seconds
        
        if (now >= (count1 + 1) * 400) {
            count1++;
            write(fd1, "MSG1", 4);
        }
        if (now >= (count2 + 1) * 700) {
            count2++;
            write(fd2, "MSG2", 4);
        }
        usleep(50000);
    }
    close(fd1);
    close(fd2);
    exit(0);
}

void run_select(int fd1, int fd2) {
    printf("--- Using select ---\n");
    int max_fd = (fd1 > fd2) ? fd1 : fd2;
    fd_set readfds;
    char buf[MSG_SIZE];
    
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(fd1, &readfds);
        FD_SET(fd2, &readfds);
        
        struct timeval tv = {1, 0};
        int ret = select(max_fd + 1, &readfds, NULL, NULL, &tv);
        if (ret < 0) break;
        if (ret == 0) continue;
        
        if (FD_ISSET(fd1, &readfds)) {
            int n = read(fd1, buf, MSG_SIZE);
            if (n <= 0) break;
            buf[n] = 0;
            printf("[select] Read from fd1: %s\n", buf);
        }
        if (FD_ISSET(fd2, &readfds)) {
            int n = read(fd2, buf, MSG_SIZE);
            if (n <= 0) break;
            buf[n] = 0;
            printf("[select] Read from fd2: %s\n", buf);
        }
    }
}

void run_poll(int fd1, int fd2) {
    printf("--- Using poll ---\n");
    struct pollfd fds[2];
    fds[0].fd = fd1; fds[0].events = POLLIN;
    fds[1].fd = fd2; fds[1].events = POLLIN;
    char buf[MSG_SIZE];

    while (1) {
        int ret = poll(fds, 2, 1000);
        if (ret < 0) break;
        if (ret == 0) continue;

        if (fds[0].revents & (POLLIN | POLLHUP | POLLERR)) {
            int n = read(fd1, buf, MSG_SIZE);
            if (n <= 0) break;
            buf[n] = 0;
            printf("[poll] Read from fd1: %s\n", buf);
        }
        if (fds[1].revents & (POLLIN | POLLHUP | POLLERR)) {
            int n = read(fd2, buf, MSG_SIZE);
            if (n <= 0) break;
            buf[n] = 0;
            printf("[poll] Read from fd2: %s\n", buf);
        }
    }
}

void run_epoll(int fd1, int fd2) {
    printf("--- Using epoll ---\n");
    int epfd = epoll_create1(0);
    struct epoll_event ev1 = {.events = EPOLLIN, .data.fd = fd1};
    struct epoll_event ev2 = {.events = EPOLLIN, .data.fd = fd2};
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd1, &ev1);
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd2, &ev2);
    
    struct epoll_event events[2];
    char buf[MSG_SIZE];

    while (1) {
        int ret = epoll_wait(epfd, events, 2, 1000);
        if (ret < 0) break;
        if (ret == 0) continue;

        for (int i = 0; i < ret; i++) {
            if (events[i].events & (EPOLLIN | EPOLLHUP | EPOLLERR)) {
                int curr_fd = events[i].data.fd;
                int n = read(curr_fd, buf, MSG_SIZE);
                if (n <= 0) goto end_epoll;
                buf[n] = 0;
                printf("[epoll] Read from fd%d: %s\n", (curr_fd == fd1) ? 1 : 2, buf);
            }
        }
    }
end_epoll:
    close(epfd);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <select|poll|epoll>\n", argv[0]);
        return 1;
    }

    int pipe1[2], pipe2[2];
    if (pipe(pipe1) < 0 || pipe(pipe2) < 0) return 1;

    pid_t pid = fork();
    if (pid == 0) {
        close(pipe1[0]);
        close(pipe2[0]);
        child_producer(pipe1[1], pipe2[1]);
    } else {
        close(pipe1[1]);
        close(pipe2[1]);
        
        if (strcmp(argv[1], "select") == 0) run_select(pipe1[0], pipe2[0]);
        else if (strcmp(argv[1], "poll") == 0) run_poll(pipe1[0], pipe2[0]);
        else if (strcmp(argv[1], "epoll") == 0) run_epoll(pipe1[0], pipe2[0]);
        else printf("Unknown mode\n");

        close(pipe1[0]);
        close(pipe2[0]);
        wait(NULL);
        printf("[%s] grep_marker_success\n", argv[1]);
    }
    return 0;
}
