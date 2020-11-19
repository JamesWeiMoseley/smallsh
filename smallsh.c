//James Moseley
//CS 344
//Assignment 3
// Fall 2020

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h> 

#define BUFFER 2048
#define MAX_ARGS 512

int backgroundCounter = 0;                       //keeps track of background processes
pid_t backgroundPIDS[MAX_ARGS];                          //array of background pids
int childStatus;
int back = 0;                               //background flag
int fore = 0;                               //foreground flag
struct sigaction SIGINT_action = {0};       //struct for sigint
struct sigaction SIGSTP_action = {0};

// the execute function is for shell functions that have no arguments or just 1 (exampel: sleep 5 or ls)
void execute(char* str ,char *token) {
    int pid = fork();
            if(pid == -1) {
                printf("something is wrong\n");
                fflush(stdout);
                exit(1);
            }
            if (pid == 0) { 
                char *newargv[] = {str, token, NULL};
                if(back == 0) {
                    SIGINT_action.sa_handler = SIG_DFL;             //this the sig handler
                    sigaction(SIGINT, &SIGINT_action, NULL);
                }
                backgroundPIDS[backgroundCounter] = pid;
                backgroundCounter++;
                execvp(str, newargv);                          //if it gets to here it will exec in the child
                perror(str);
                fflush(stdout);
                exit(2);
            } else {
        
                if(back==0) {
                    waitpid(pid, &childStatus, 0);
                    
                } if (back == 1) { 
                    
                    pid = waitpid(-1, &childStatus, WNOHANG);
                    while(pid>0) {
                        if(WIFEXITED(childStatus)) {
                            printf("background pid %d is done: exit value %d\n", pid, childStatus);
                            fflush(stdout);
                            back = 0;
                        } else {
                            printf("background pid %d is done: terminated by signal %d\n", pid, childStatus);
                            fflush(stdout);
                        }
                        pid = waitpid(-1, &childStatus, WNOHANG);
                        back = 0;
                    }
                    

                }
               
            }
}

//function if we encounter < or >: is similar to the above function
//but adds the dup2 and can open files
void duplicateIn(char* str, char* input, char* output) {
    int pid = fork();
        if (pid == -1) {
            printf("cannot open %s\n", str);
            fflush(stdout);
            exit(1);
        }
        if(pid ==0) {
            char *newargv[] = {str, NULL};
            if(back ==0) {
                SIGINT_action.sa_handler = SIG_DFL;
                sigaction(SIGINT, &SIGINT_action, NULL);
            }
            //this is if the parameters contains an input
            if(input!=NULL) {
                int targetFD = open(input, O_RDONLY);           //open file for read only
                if(targetFD == -1) {
                    printf("cannot open %s for input\n", input);
                    fflush(stdout);
                    exit(1);
                }
                int howMany = dup2(targetFD, 0);                //using duplicate function
                close(targetFD);
                //here is if the input is null
            } if(output!=NULL) {
                int targetFD = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0640);
                if(targetFD == -1) {
                    printf("cannot open %s for output\n", output);
                    fflush(stdout);
                    exit(1);
                }
                int howMany = dup2(targetFD, 1);
                close(targetFD);
            }
                backgroundPIDS[backgroundCounter] = pid;
                backgroundCounter++;
                execvp(str, newargv);                   //run the exec
                perror(str);
                fflush(stdout);
        } else {
            if(back==0) {
                    waitpid(pid, &childStatus, 0);
                } if (back == 1) { 
                    pid = waitpid(pid, &childStatus, WNOHANG);
                    while(pid>0) {
                        if(WIFEXITED(childStatus)) {
                            printf("background pid %d is done: exit value %d\n", pid, childStatus);
                            fflush(stdout);
                            back = 0;
                        } else {
                            printf("background pid %d is done: terminated by signal %d\n", pid, childStatus);
                            fflush(stdout);
                        }
                        pid = waitpid(-1, &childStatus, WNOHANG);
                        back = 0;
                    }
                    

                }
        }
}

// this signal handles if we see an &
void handle_SIGSTP(int signo) {
    if(fore == 0) {
        fore = 1;                                       //flag will set as 1 so it can exit foregroud mode
        char* message = "\nEntering foreground-only mode (& is now ignored)\n : ";
        write(STDOUT_FILENO, message, 54);
        fflush(stdout);
    } else {
        fore = 0;
        char* message = "\nExiting foreground-only mode\n : ";
        write(STDOUT_FILENO, message, 34);
        fflush(stdout);
    }
    
}

// this function is for the $$, it will replace them with an %d and use sprintf
void replaceLetters(char* str, char** temp) {
    for(int i = 0; i < strlen(str); i++) {
        if(str[i]=='\0') {
            break;
        }
        if(str[i]=='$' && str[i+1]=='$'){
            str[i]='%';
            str[i+1] = 'd';
        }
    }
    *temp = calloc(strlen(str)+1, sizeof(char));
    sprintf(*temp, str, getpid());
    strcpy(str, *temp);
}

void getStatus() {

}


int main(int argc, char const *argv[])
{
    int status = 0;
    char *buffer;
    size_t bufsize = BUFFER;
    size_t inputLine;
    const char s[2] = " \n";           
    char str[MAX_ARGS];
    char *token;
    char *temp;
    int run = 0;
    char command[50];
    char input [50];
    char output[50];
    char newarg[50];
    
    
    //SIGINT : the rest (sig_dfl) is in the execute functions
    SIGINT_action.sa_handler = SIG_IGN;
    sigaction(SIGINT, &SIGINT_action, NULL);

    //this will handle the sigtstop
    SIGSTP_action.sa_handler = handle_SIGSTP;
    sigfillset(&SIGSTP_action.sa_mask);
    SIGSTP_action.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &SIGSTP_action, NULL);

    //setting buffer for user input
    buffer = (char *)malloc(bufsize * sizeof(char)+1);
    printf("smallsh\n");
    while(1){
        printf(" : ");
        fflush(stdout);
        inputLine = getline(&buffer,&bufsize,stdin);        //gets the user input so that we can tokenize it
        replaceLetters(buffer, &temp);
        
        if(inputLine == 1) {                            //this is if the user inputed nothing
            continue;
        }
        token = strtok(buffer, s);                      //tokenizing the user input
        strcpy(command, token);
            if(strcmp(token, "cd")==0) {                //if the user input is cd
                strcpy(command, token);
                char* dir;
                token = strtok(NULL, s);
                if(token==NULL) {
                    dir = getenv("HOME");               //default cd
                    chdir(dir);
                } else {
                        chdir(token);      //change to a specific dir
                }
            } else if(strcmp(token, "status")==0) {         //if user input is status
                if(WIFEXITED(childStatus)) {
                    printf("exit value %d\n", WIFEXITED(childStatus));
                    fflush(stdout);
                } else if (WIFSIGNALED(childStatus)) {
                    printf("terminated by signal %d\n", WTERMSIG(childStatus));
                    fflush(stdout);
                    
                }
            } else if(strstr(token, "$$")!=NULL) {              //if user input is $$
                pid_t pid = getpid();
                printf("background pid is %d\n", pid);
                fflush(stdout);
            } else if(strcmp(token, "exit")==0) {               //exit case
                int i = 0;
                while(i<backgroundCounter) {
                    kill(backgroundPIDS[i], SIGTERM);
                    i++;
                }
                exit(EXIT_SUCCESS);
            } else if(token[0]=='#'){
                printf("\n");
                continue;
            } else {                                //this handles cases such as > or <
                token = strtok(NULL, s);
                if(token==NULL) {
                execute(command, NULL);
                } else {    
                    if(strcmp(token, ">")==0) {
                        token = strtok(NULL, s);
                        duplicateIn(command, NULL, token);
                    } else if (strcmp(token, "<")==0) {
                        token = strtok(NULL, s);
                        strcpy(input, token);
                        token = strtok(NULL, s);
                        if(token == NULL) {
                            duplicateIn(command, input, NULL);
                        } else {
                            token = strtok(NULL, s);
                            strcpy(output, token);
                            duplicateIn(command, input, output);
                        }
                    } else {
                        strcpy(newarg, token);
                        token = strtok(NULL, s);
                        if(token == NULL) {
                                execute(command, newarg);
                        } else if (strcmp(token, "&")==0) {
                            back = 1; 
                            execute(command, newarg);           //this way it will only execute before the &
                        } else {
                            while(token!=NULL) {
                                strcat(newarg, " ");            //cat all the arguments to the string
                                strcat(newarg, token);
                                token = strtok(NULL, s);
                            }
                            execute(command, newarg);
                        }
                    }
            }  
        }
        

        
    }
    return 0;
}