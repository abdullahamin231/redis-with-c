#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>


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


static int32_t query(int fd, const char* text) {
    const size_t max_msg_len = 4096;
    uint32_t len = strlen(text);
    char write_buf[4 + max_msg_len];
    memcpy(write_buf, &len, 4);
    memcpy(&write_buf[4], text, len);

    int32_t err = write_all(fd, write_buf, len + 4);
    if(err) {
        perror("write_all()\n");
        return err;
    }

    char read_buf[4 + max_msg_len];
    err = read_full(fd, read_buf, 4);
    if(err) {
        fprintf(stderr, "%s", err == 0 ? "EOF\n" : "read_full()\n");
        return err;
    }
    memcpy(&len, read_buf, 4);
    if(len > (max_msg_len)) {
        perror("server response length exceeds 4096");
        return -1;
    }

    err = read_full(fd, &read_buf[4], len);
    if(err) {
        perror("read_full()");
        return err;
    }

    printf("server: %.*s \n", len, &read_buf[4]);
    return 0;
}

int main(){
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) {
        perror("socket()");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK); // 127.0.0.1
    int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if(rv) {
        perror("connect()");
        exit(EXIT_FAILURE);
    }

    int32_t err = query(fd, "helloaasodo");
    if(err) goto DONE;
    err = query(fd, "yo mama");


    DONE:
    close(fd);
}
