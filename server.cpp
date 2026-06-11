#include <asm-generic/socket.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>

void die(const char* err_str) {
    fprintf(stderr, "%s\n", err_str);
    exit(EXIT_FAILURE);
}

static void set_fd_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
}

static int32_t read_full(int fd, char* buf, size_t n) {
    while(n > 0) {
        ssize_t bytes_read = read(fd, buf, n);
        if(bytes_read < 0) return -1;
        assert((size_t)bytes_read <= n);
        n -= bytes_read;
        buf += bytes_read;
    }
    return 0;
}

static int32_t write_all(int fd, char* buf, size_t n) {
    while(n > 0) {
        ssize_t bytes_written = write(fd, buf, n);
        if(bytes_written < 0) return -1;
        assert((size_t)bytes_written <= n);
        n -= bytes_written;
        buf += bytes_written;
    }
    return 0;
}

static int32_t handle_one_request(int conn_fd) {
    const size_t max_msg_len = 4096;

    char read_buf[4 + max_msg_len];
    errno = 0;
    int32_t err = read_full(conn_fd, read_buf, 4);
    if(err) {
        fprintf(stderr, "%s", errno == 0 ? "EOF\n" : "read_full()\n");
        return err;
    }
    uint32_t len = 0;
    memcpy(&len, read_buf, 4);
    if(len > max_msg_len) {
        fprintf(stderr, "msg exceeds max len: %d\n", (int)max_msg_len);
        return -1;
    }

    err = read_full(conn_fd, &read_buf[4], len);
    if(err) {
        perror("read_full()");
        return err;
    }

    printf("client[%d]: %s\n", len, &read_buf[4]);

    const char reply[] = "hello from server";
    char write_buf[4 + strlen(reply)];
    len = (uint32_t)strlen(reply);
    memcpy(&write_buf, &len, 4);
    memcpy(&write_buf[4], reply, strlen(reply));
    return write_all(conn_fd, write_buf, 4 + len);
}

/**
 * while running:
 *      want_read  = [...]
 *      want_write = [...]
 *      can_read, can_write = wait_for_readiness(want_read, want_write) # blocks until there is one r/w conn.
 *      for fd in can_read:
 *          data = read_nb(fd) # non blocking
 *          handle_data(fd, data) # application logic w/o IO
 *      for fd in can_write:
 *          data = pending_data(fd) # data to write produced by application
 *          write_nb(fd, data) # non blocking - append to buffer
 */

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    set_fd_nonblock(fd);
    // allow socket to reuse address after restart
    int allow_reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &allow_reuse, sizeof(allow_reuse));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234); // port: 1234
    addr.sin_addr.s_addr = htonl(0); // 0.0.0.0
    int rv = bind(fd, (const struct sockaddr*)&addr, sizeof(addr));
    if(rv) { die("bind()"); }

    rv = listen(fd, SOMAXCONN);
    if(rv) { die("listen()"); }

    while(true) {
        struct sockaddr_in client_addr = {};
        socklen_t client_addr_len = sizeof(client_addr);
        int conn_fd = accept(fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if(conn_fd < 0) { continue; }

        while(true) {
            int err = handle_one_request(conn_fd);
            if(err) break;
        }

        close(conn_fd);
    }

}
