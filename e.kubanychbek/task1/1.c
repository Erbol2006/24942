/*
 * procattrs.c — вывод/изменение атрибутов процесса
 * Опции выполняются справа налево (последняя в командной строке — первая выполняется).
 *
 * -i            печатает real/effective uid/gid
 * -s            делает процесс лидером группы (setpgid)
 * -p            печатает pid/ppid/pgid
 * -u            печатает ulimit (UL_GETFSIZE)
 * -Unew_ulimit  меняет ulimit (UL_SETFSIZE)
 * -c            печатает лимит core-файла в байтах (getrlimit RLIMIT_CORE)
 * -Csize        меняет лимит core-файла в байтах (setrlimit RLIMIT_CORE)
 * -d            печатает текущую директорию
 * -v            печатает переменные среды
 * -Vname=value  добавляет/меняет переменную среды
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <ulimit.h>

extern char **environ;

struct optrec {
    int opt;
    char *arg; 
};

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [-i] [-s] [-p] [-u] [-Unew_ulimit] [-c] [-Csize] [-d] [-v] [-Vname=value]\n",
        prog
    );
}

static int parse_nonneg_long(const char *s, long *out) {
    char *end = NULL;
    long v;

    if (s == NULL || *s == '\0') return -1;
    errno = 0;
    v = strtol(s, &end, 10);
    if (errno != 0 || end == NULL || *end != '\0' || v < 0) return -1;

    *out = v;
    return 0;
}

static void print_ids(void) {
    printf("uid=%ld euid=%ld gid=%ld egid=%ld\n",
           (long)getuid(), (long)geteuid(), (long)getgid(), (long)getegid());
}

static void make_pgrp_leader(void) {
    if (setpgid(0, 0) == -1) perror("setpgid");
}

static void print_pids(void) {
    pid_t pid = getpid();
    pid_t ppid = getppid();
    pid_t pgid = getpgid(0);
    printf("pid=%ld ppid=%ld pgid=%ld\n", (long)pid, (long)ppid, (long)pgid);
}

static void print_ulimit(void) {
    errno = 0;
    long r = ulimit(UL_GETFSIZE);
    if (r == -1 && errno != 0) {
        perror("ulimit(UL_GETFSIZE)");
        return;
    }
    printf("%ld\n", r);
}

static void set_ulimit(const char *arg) {
    long v;
    if (parse_nonneg_long(arg, &v) != 0) {
        fprintf(stderr, "Bad value for -U: '%s'\n", arg ? arg : "(null)");
        return;
    }
    errno = 0;
    long r = ulimit(UL_SETFSIZE, v);
    if (r == -1 && errno != 0) {
        perror("ulimit(UL_SETFSIZE)");
        return;
    }
}

static void print_rlimit_bytes(int res) {
    struct rlimit rl;
    if (getrlimit(res, &rl) == -1) {
        perror("getrlimit");
        return;
    }
    if (rl.rlim_cur == RLIM_INFINITY) {
        printf("unlimited\n");
    } else {
        printf("%ld\n", (long)rl.rlim_cur);
    }
}

static void set_rlimit_bytes(int res, const char *arg) {
    struct rlimit rl;
    long v;

    if (parse_nonneg_long(arg, &v) != 0) {
        fprintf(stderr, "Bad value for limit: '%s'\n", arg ? arg : "(null)");
        return;
    }
    if (getrlimit(res, &rl) == -1) {
        perror("getrlimit");
        return;
    }

    /* Меняем только soft limit, max оставляем как есть */
    rl.rlim_cur = (rlim_t)v;

    if (setrlimit(res, &rl) == -1) {
        perror("setrlimit");
        return;
    }
}

static void print_cwd(void) {
    char buf[PATH_MAX];
    if (getcwd(buf, sizeof(buf)) == NULL) {
        perror("getcwd");
        return;
    }
    puts(buf);
}

static void print_env(void) {
    char **e;
    for (e = environ; e != NULL && *e != NULL; ++e) {
        puts(*e);
    }
}

static void set_env_nv(const char *arg) {
    const char *eq;
    size_t nlen;
    char *name, *value;

    if (arg == NULL) {
        fprintf(stderr, "-V requires name=value\n");
        return;
    }

    eq = strchr(arg, '=');
    if (eq == NULL || eq == arg) {
        fprintf(stderr, "Bad value for -V (need name=value): '%s'\n", arg);
        return;
    }

    nlen = (size_t)(eq - arg);
    name = (char*)malloc(nlen + 1);
    if (!name) {
        perror("malloc");
        return;
    }
    memcpy(name, arg, nlen);
    name[nlen] = '\0';

    value = (char*)eq + 1; 

    if (setenv(name, value, 1) == -1) {
        perror("setenv");
    }

    free(name);
}

int main(int argc, char *argv[]) {
    struct optrec ops[256];
    int nops = 0;
    int opt;
    int i;

    if (argc == 1) {
        /* “Нет аргументов” — просто выходим */
        return 0;
    }

    opterr = 0; 

    /* ':' после опции означает, что у неё есть аргумент (getopt) :contentReference[oaicite:5]{index=5} */
    while ((opt = getopt(argc, argv, "ispuU:cC:dvV:")) != -1) {
        if (opt == '?') {
            fprintf(stderr, "Unknown option: -%c\n", optopt);
            usage(argv[0]);
            return 1;
        }

        ops[nops].opt = opt;
        ops[nops].arg = NULL;

        if (opt == 'U' || opt == 'C' || opt == 'V') {
            ops[nops].arg = strdup(optarg ? optarg : "");
            if (ops[nops].arg == NULL) {
                perror("strdup");
                return 1;
            }
        }

        nops++;
        if (nops >= (int)(sizeof(ops)/sizeof(ops[0]))) {
            fprintf(stderr, "Too many options\n");
            return 1;
        }
    }

    /* Выполняем справа налево */
    for (i = nops - 1; i >= 0; --i) {
        switch (ops[i].opt) {
            case 'i': print_ids(); break;
            case 's': make_pgrp_leader(); break;
            case 'p': print_pids(); break;
            case 'u': print_ulimit(); break;
            case 'U': set_ulimit(ops[i].arg); break;
            case 'c': print_rlimit_bytes(RLIMIT_CORE); break;
            case 'C': set_rlimit_bytes(RLIMIT_CORE, ops[i].arg); break;
            case 'd': print_cwd(); break;
            case 'v': print_env(); break;
            case 'V': set_env_nv(ops[i].arg); break;
            default: break;
        }
    }

    for (i = 0; i < nops; ++i) {
        free(ops[i].arg);
    }

    return 0;
}
