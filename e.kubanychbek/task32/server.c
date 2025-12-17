#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include <signal.h>
#include <aio.h>
#include <errno.h>

#define PATH "/tmp/uds_upper_aio.sock"
#define MAXC 256
#define BUFSZ 4000

/* структура "клиент" хранит всё, что нужно для AIO */
struct client {
    int used;            /* 0 - слот свободен, 1 - занят */
    int fd;              /* сокет клиента */
    struct aiocb cb;     /* управляющий блок AIO (обязан жить долго!) */
    char buf[BUFSZ];     /* буфер чтения (обязан жить долго!) */
};

static struct client clients[MAXC];

/*
 * Обработчик SIGIO.
 * В POSIX AIO мы можем попросить ядро прислать сигнал и передать указатель
 * через sigev_value.sival_ptr :contentReference[oaicite:2]{index=2}.
 * Мы как раз передаём указатель на struct client.
 */
static void sigiohandler(int signo, siginfo_t *info, void *ucontext) {
    (void)ucontext;

    if (signo != SIGIO) return;

    struct client *c = (struct client *)info->si_value.sival_ptr;
    if (!c || !c->used) return;

    /* проверяем завершение запроса */
    if (aio_error(&c->cb) != 0) {      /* ошибка */
        close(c->fd);
        c->used = 0;
        return;
    }

    /* забираем результат: сколько реально прочитано */
    ssize_t n = aio_return(&c->cb);    /* :contentReference[oaicite:3]{index=3} */
    if (n <= 0) {                      /* 0 = EOF (клиент закрылся), <0 = ошибка */
        close(c->fd);
        c->used = 0;
        return;
    }

    /* переводим в верхний регистр */
    for (ssize_t i = 0; i < n; i++)
        c->buf[i] = (char)toupper((unsigned char)c->buf[i]);

    /* выводим на stdout */
    write(1, c->buf, (size_t)n);

    /* ставим следующий асинхронный read */
    aio_read(&c->cb);                  /* :contentReference[oaicite:4]{index=4} */
}

int main() {
    int listenfd, cfd;
    struct sockaddr_un addr;

    /* ставим обработчик SIGIO с SA_SIGINFO, чтобы получать siginfo_t :contentReference[oaicite:5]{index=5} */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = sigiohandler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGIO, &sa, NULL);

    /* серверный сокет */
    listenfd = socket(AF_UNIX, SOCK_STREAM, 0);

    unlink(PATH);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, PATH, sizeof(addr.sun_path) - 1);

    bind(listenfd, (struct sockaddr*)&addr, sizeof(addr));
    listen(listenfd, 32);

    /* очистим слоты */
    for (int i = 0; i < MAXC; i++) clients[i].used = 0;

    for (;;) {
        /* accept может быть прерван сигналом от AIO => EINTR, тогда просто повторяем */
        cfd = accept(listenfd, NULL, NULL);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            continue; /* минимально: без разбирательств */
        }

        /* найдём свободный слот */
        int idx = -1;
        for (int i = 0; i < MAXC; i++) {
            if (!clients[i].used) { idx = i; break; }
        }
        if (idx == -1) { close(cfd); continue; }

        /* чтобы обработчик SIGIO не схватил "полузаполненную" структуру — блокируем SIGIO на время настройки */
        sigset_t set, old;
        sigemptyset(&set);
        sigaddset(&set, SIGIO);
        sigprocmask(SIG_BLOCK, &set, &old);

        clients[idx].used = 1;
        clients[idx].fd = cfd;

        memset(&clients[idx].cb, 0, sizeof(clients[idx].cb));
        clients[idx].cb.aio_fildes = cfd;
        clients[idx].cb.aio_buf    = clients[idx].buf;
        clients[idx].cb.aio_nbytes = sizeof(clients[idx].buf);

        /* просим уведомлять сигналом SIGIO и передать нам указатель на client :contentReference[oaicite:6]{index=6} */
        clients[idx].cb.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
        clients[idx].cb.aio_sigevent.sigev_signo  = SIGIO;
        clients[idx].cb.aio_sigevent.sigev_value.sival_ptr = &clients[idx];

        /* запускаем асинхронное чтение */
        aio_read(&clients[idx].cb);

        sigprocmask(SIG_SETMASK, &old, NULL);

        /* дальше снова ждём accept() новых клиентов; данные от старых придут сигналами */
    }

    return 0;
}
