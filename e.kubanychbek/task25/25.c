#include <stdio.h> 
#include <unistd.h>
#include <ctype.h> 
#include <string.h> 

int main(void){
    int fd[2]; 
    pipe(fd); 
    
    if (fork() == 0){
        close(fd[1]); /
        
        char buf[256];
        int n;

        while ((n = read(fd[0], buf, sizeof(buf))) > 0){
            //переводим каждый символ в верхний регистр
            for (int i = 0; i < n; i++){
                buf[i] = toupper((unsigned char) buf[i]);
            }
            write(1, buf, n);  
        }
        close(fd[0]);
        return 0; 
    }

    //родительский процесс
    close(fd[0]); // закрываем конец для чтения 
    const char *text = "Hello world, today I'm handing in assignments on operating systems.\n";
    write(fd[1], text, strlen(text));
    close(fd[1]); //закрываем pipe, то есть дочерний процеес увидит EOF
    return 0;
}