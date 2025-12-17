/*
Нужно написать программу, которая:
 1. Разбирает аргументы командной строки через getopt(3C).
 2. Поддерживает опции:
 • -i — печатает реальные и эффективные uid/gid.
 • -s — делает текущий процесс лидером группы (через setpgid).
• -p — печатает PID, PPID и идентификатор группы процессов (PGID).
 • -u — печатает значение ulimit (лимит размера создаваемого файла).
 • -U new_ulimit — изменяет ulimit.
 • -c — печатает лимит размера core-файла.
 • -C size — изменяет лимит core-файла.
 • -d — печатает текущую рабочую директорию.
 • -v — печатает все переменные среды и их значения.
 • -V name=value — добавляет/меняет переменную среды.
 3. Опции могут повторяться (одну и ту же можно указывать несколько раз).
 4. Порядок выполнения опций — в обратном порядке их появления: «справа налево».
 5. Нужно протестировать:
 • Без аргументов (программа просто ничего не делает или выводит usage — на твой вкус).
 • Недопустимую опцию — должна быть показана ошибка.
 • Опции «склеенные» после одного -: -ipv.
 • Неверное значение для -U (например, -Uabc) — нужно сообщить об ошибке.
*/

#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <limits.h>
#include <errno.h>

extern char **environ; //массив указателей на строки среды

//нам нужна получается структура чтобы запоминать 
//все опции в порядке, как их вернул getopt
struct optrec{int opt; char *arg;};

//печать текущего лимимта ресурса res, через RLIMIT_FSIZE или RLIMIT_CORE
static void print_limit(int res){
    struct rlimit rl; //стр-ра лимитов
    if (getrlimit(res, &rl)==-1){  //получаем текущие лимиты 
        perror("getrlimit");return;
    }
    if (rl.rlim_cur == RLIM_INFINITY){
        printf("бесконечный\n");
    } else {
        printf("%ld\n", (long)rl.rlim_cur);
    }
}

//устанавливаем лимита ресурса res из строки s 
static int set_limit(int res, const char *s){
    struct rlimit rl; 
    char *end;  //указатель на конец числа при разборе 
    errno = 0;  //обнуляем 
    long v = strtol(s, &end, 10); //парсим десятичное число из строки 
    if (errno != 0 || *end != '\0' || v < 0){
        fprintf(stderr, "плохое значение лимита: %s\n", s );
        return -1; 
    }
    rl.rlim_cur = (rlim_t)v; //устанавливаем новое soft-значение 
    if (setrlimit(res, &rl) == -1){ //применяем лимит 
        perror("setrlimit");
        return -1;
    }
    return 0;
}
int main(int argc, char *argv[]){
    extern char *optarg;
    struct optrec ops[128];
    int nops = 0; //сколько опций запомнили 
    int opt;      //текущая опция 

    //ТЕПЕРЬ НАЧИАНЕМ разбирать опции командной строки с помощью getopt
    while ((opt=getopt(argc, argv, "ispuUcC:dvV:")) != -1){
        if (nops<(int)(sizeof ops/sizeof ops[0])){
            ops[nops].opt=opt; //сохраняем код опции 
            ops[nops].arg=optarg;//сохраняем указатель на ее аргумент если есть
            nops++;
        }
        if (opt=="?"){ //не та опция
            return 1;
        }

    }
    for (int i = nops - 1; i>=0; i--){
        switch(ops[i].opt){

            //i = uid/gid
            case 'i':{
                //получаем реальный и эффективнй UID
                uid_t uid=getuid(), euid=geteuid();
                //получаем реал.и эффек. GID 
                gid_t gid = getgid(), egid=getegid();
                printf("uid=%ld euid=%ld gid=%ld egid=%ld\n", (long)uid, (long)euid, (long)gid, (long)egid);
                break;
            }
            //s - сделать процесс лидером группы 
            case 's':{
                if (setpgid(0, 0)==-1) perror("setpgid");
                break;
            }

            //p - печать pid, ppid, pgid
            case 'p':{
                //получаем идентиф. процесса и родиетля 
                pid_t pid = getpid(), ppid = getppid();
                //получаем идент. группы процессов 
                pid_t pgid = getpgid(0);
                printf("pid=%ld ppid=%ld pgid=%ld\n", (long)pid, (long)ppid, (long)pgid);
                break;
            }
            
            //u - печать лимита ulimit (RLIMIT_FSIZE)
            case 'u':{
                print_limit(RLIMIT_FSIZE);
                break;
            }

            //U - изменяем ulimit
            case 'U':{
                if(set_limit(RLIMIT_FSIZE, ops[i].arg)<0){
                    fprintf(stderr, "-U не получилось");
                }
                break;
            }

            //-с = печать лимита core-файла
            case 'c':{
                print_limit(RLIMIT_CORE);
                break;
            }

            //-C = изменить размер кор-файла
            case 'C':{
                if(set_limit(RLIMIT_CORE, ops[i].arg) < 0){
                    fprintf(stderr, "-C не получилось");
                }
                break;
            }

            //-d = печать текущей рабочей директории 
            case 'd':{
                char *cwd = getcwd(NULL, 0);
                if (cwd == NULL){
                    perror("getcwd");
                } else {
                    printf("%s\n", cwd);
                    free (cwd);
                }
                break;
            }

            //-v = печать всех переменных среды
            case 'v':{
                //иднм по массиву environ до NULL
                for(char **e=environ; *e; e++){
                    puts(*e);//печатаем КААЖДУЮ строку "NAME+VALUE"
                }
                break;
            }  
            
            //-V = изменяем или устанавливаем переменную среы
            case 'V':{
                char *arg=ops[i].arg; //аргумент опции это строка типа name=value
                char *eq=strchr(arg, '='); //ищем символ = 
                if (!eq){
                    fprintf(stderr, "-V: не нашли =\n");
                    break;
                }
                //разделяем строку, теперь arg - name, eq+1 - value
                *eq = '\0';
                //устанавливаем переменную среды
                if (setenv(arg, eq+1, 1)==-1) perror("setenv");
                break;   
            }
            default:
                break;
        }
    }
    return 0; 
}
