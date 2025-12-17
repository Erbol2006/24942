//cc server.c -o server -lsocket -lnsl

#include <sys/types.h> 

#include <sys/socket.h> 
#include <sys/un.h>
#include <sys/select.h>
#include <unistd.h> 
#include <string.h> 
#include <ctype.h>

#include <stdio.h> 
#include <sys/time.h>
#define PATH "/tmp/uds_upper_e_kubanychbek.sock"

int main(){
    int listenfd; //слушающий сокет сервера
    int cfd; //сокет конкретного клиента
    struct sockaddr_un addr;
    int clients[FD_SETSIZE]; //список клиентских fd 
    int i; //индекс для циклов
    int maxfd; // макс. fd в наборе 
    int n; //сколько байт прочитали 
    char buf[4000];

    hrtime_t start_ns[FD_SETSIZE];
    long long total_bytes[FD_SETSIZE];

    listenfd = socket(AF_UNIX, SOCK_STREAM, 0);

    unlink(PATH);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX; 

    //копируем путь в sun_path
    strncpy(addr.sun_path, PATH, sizeof(addr.sun_path) - 1);

    bind(listenfd, (struct sockaddr*)&addr, sizeof(addr));
    listen(listenfd, 32);

    //у нас изначально клиентов нет
    for (i = 0; i < FD_SETSIZE; i++){
        clients[i] = -1;//<- поэтому слот пустой
        start_ns[i] = 0;
        total_bytes[i] = 0;
    }
    for (;;) {//запускакм бесконечный цико обслуживания
        fd_set rfds; //набор fd за которыми следим для чтения

        FD_ZERO(&rfds); //очистили набор

        FD_SET(listenfd, &rfds);
        maxfd = listenfd; 

        for (i = 0; i < FD_SETSIZE; i++){ //всех активных клиентов добавляем в набор
            if (clients[i] != -1){ //если слот занят (клиент подключен)
                FD_SET(clients[i], &rfds); //следим за готовностью клиента на чтение
                if (clients[i] > maxfd){ //обновляем maxfd
                    maxfd = clients[i];
                }
            }
        }
        
        select(maxfd + 1, &rfds, NULL, NULL, NULL);
        //select - блокируется пока: 
        // * не появится новый клиент 
        // * или кто то из клиентов не пришлет данные 

        if (FD_ISSET(listenfd, &rfds)){ //то есть если listenfd готов - есть входящее подключение
            cfd = accept(listenfd, NULL, NULL);

            for (i = 0; i < FD_SETSIZE; i++){
                if (clients[i] == -1){
                    clients[i] = cfd;//запоминаем fd клиента 
                    start_ns[i] = gethrtime(); 
                    
                    total_bytes[i] = 0; 
                    dprintf(2, "[fd=%d] connect\n", cfd);
                    break;
                }
            }

            //если свободных слотов не нашли, то просто закрываем клиента
            if (i == FD_SETSIZE){
                close(cfd);
            }
        }

        //ну и наконец, начинаем проверять всех клиентов, кто готов - того читаем 
        for (i = 0; i < FD_SETSIZE; i++){
            int fd = clients[i];

            if (fd != -1 && FD_ISSET(fd, &rfds)) { //если клиент существует и готов к чтению
                n = read(fd, buf, sizeof(buf));

                if (n <= 0){ //n == 0 -> клиент щакрылся, а если n < 0 то это просто ошибка
                    hrtime_t end = gethrtime();
                    dprintf(2, "[fd=%d] disconnect, bytes=%lld, conn_time=%lld ns\n",
                            fd, total_bytes[i], (long long)(end - start_ns[i]));
                    close (fd);
                    clients[i] = -1; 
                } else {
                    int k; //индекс по байтам полученного блока
                    hrtime_t t0 = gethrtime();
                    total_bytes[i] += n;
                    for (k = 0; k < n; k++){
                        buf[k] = (char)toupper((unsigned char)buf[k]);
                    }
                    write(1, buf, n);
                    hrtime_t t1 = gethrtime();
                    dprintf(2, "[fd=%d] chunk=%d bytes, work=%lld ns\n",
                            fd, n, (long long)(t1 - t0));
                }
            }
        }
    }
    return 0;
}