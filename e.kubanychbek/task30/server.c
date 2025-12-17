#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define PATH "/tmp/uds_upper_e_kubanychbek.sock"

int main(){
    int fd, cfd;
    struct sockaddr_un addr; 
    char buf[4000];
    int n, i;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);

    unlink(PATH);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX; 
    strncpy(addr.sun_path, PATH, sizeof(addr.sun_path) - 1);

    bind(fd, (struct sockaddr*)&addr, sizeof(addr)); 
    listen(fd, 1);

    cfd = accept(fd, NULL, NULL);
    while ((n = read(cfd, buf, sizeof(buf))) > 0){
        for (i = 0; i < n; i++){
            buf[i] = (char)toupper((unsigned char)buf[i]);
        }
        write(1, buf, n);
    }
    close(cfd);
    close(fd);
    unlink(PATH);
    return 0; 
}