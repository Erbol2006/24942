#include <sys/types.h>
#include <sys/socket.h> 
#include <sys/un.h>
#include <unistd.h>
#include <string.h>

#define PATH "/tmp/uds_upper.sock"

int main(){
    int fd;
    struct sockaddr_un addr;
    char buf [4000];
    int n; 

    fd = socket(AF_UNIX, SOCK_STREAM, 0);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, PATH, sizeof(addr.sun_path) - 1);
    

    connect(fd, (struct sockaddr*)&addr, sizeof(addr));
    
    while ((n = read(0, buf, sizeof(buf))) > 0){
        write(fd, buf, n);
    }
    close(fd);
    return 0; 
}