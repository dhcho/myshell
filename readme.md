# myShell Implementation

\- pipe 시스템 콜을 이용해 실제 Shell에서 pipe(|)가 내부적으로 어떻게 구현되어 동작하는지 실제 구현을 통해 알아본다.

\- UNIX Shell의 기능 중에서 redirection(>, <, >>)과 pipe의 기능을 구현하면서 파일 관련 시스템 콜(open, close, dup2)의 사용법을 익힌다.

\- 여러 파일을 목적에 따라 분할하여 컴파일 하는 분리 컴파일 방법을 습득한다.

 

\- UNIX에서는 모든 것을 File로 취급한다. 그러므로 어떠한 device도 UNIX 운영체제 내에서는 File로 취급되기 때문에 Shell에서는 리다이렉션(redirection)을 통해 사용자가 입출력의 방향을 설정할 수 있다. 출력을 터미널이 아닌 파일로 바꾸기 위해서는 ">" 리다이렉션을 이용하면 가능하며 키보드가 아닌 파일로부터 입력 받기 위해서는 "<" 리다이렉션을 이용하면 가능하다. 이 외에도 파일 출력 리다이렉션을 두 개 연속으로 사용(">>")하게 되면 기존 파일의 내용 끝에 Append가 가능하다. 입출력 리다이렉션의 사용 예를 몇 개 살펴보자면 "ls -al > foo"라는 명령을 Shell상에서 입력하면 터미널상에는 아무것도 출력되지 않는다. 하지만 "ls -al"의 출력결과가 "foo"라는 파일에 저장되는 것을 cat foo 명령을 통해 확인할 수 있다. 이것은 출력 리다이렉션(">")에 의해 출력의 방향이 파일로 바뀐 것이다. 만약 "foo"라는 파일이 이미 존재할 경우의 예외 처리는 해당 파일의 내용을 overwrite하는 식으로 구현한다. 에러에 대한 예외처리는 출력파일을 명세하지 않았을 때 (예: "ls -al > ")와 출력 파일 여러 개를 동시에 지정했을 때(예: "ls > out1 out2, ls > out1 out2 out3")이다. 하지만 다음의 경우(예: "ls > out1 > out2")는 에러 처리를 하지 않을 것이다. 실제 Shell상에서 "ls > out1 > out2" 명령을 입력하게 되면 ls의 출력결과가 "out2" 파일에 저장되는 것을 확인 할 수 있다. 리다이렉션(redirection)을 구현하기 위해서는 다음과 같은 함수를 사용하면 가능하다. open() 함수를 사용해 지정한 파일과 옵션을 인자 값으로 주게 되면 해당 파일의 파일 지정자(file descriptor)가 반환된다. 이 반환된 파일 디스크립터를 이용하여 execvp() 함수와 close() 함수, dup2() 함수를 적절하게 사용하면 구현이 가능하다. 이 함수들에 대해서는 프로그램 구현 설명부분에서 다뤄보도록 하겠다. 

간단한 입출력 리다이렉션(예: ls -al > foo)에서부터 파이프와 입출력 리다이렉션을 복합적으로 사용하는 기능(예: ls -al | sort -r | head -n 5 | wc > bar)이 동작 가능하도록 구현하는 것을 목표로 해본다.

파이프(pipe)는 여러 UNIX System IPC(Inter Process Communication) 수단들 중 가장 오래된 것으로, 모든 UNIX 시스템들이 지원한다. 파이프에는 두 가지 한계가 존재한다.

1. 예전부터 파이프는 반이중 방식(즉, 자료가 한 방향으로만 흐른다)이었다. 요즘에는 전이중 파이프를 지원하는 시스템들도 있으나, 최대의 이식성을 위해서는 파이프가 전이중 방식이라는 가정을 하지 않아야 한다.

2. 파이프는 공통의 조상을 가진 프로세스들에서만 쓰일 수 있다. 일반적인 용법은 한 프로세스가 파이프를 생성하고 fork로 자식을 만들고 부모와 자식이 그 파이프를 이용해서 통신하는 것이다.

파이프의 특징을 그림을 통해 살펴보면 다음과 같다.

​                               

 ![01](https://raw.githubusercontent.com/dhcho/myshell/master/images/01.jpg)

![02](https://raw.githubusercontent.com/dhcho/myshell/master/images/02.jpg)

파이프를 생성하게 되면 두 개의 파일 디스크립터가 생성(fd[2]) 된다. fd는 파일 디스크립터를 말한다. 그림에 설명된 것처럼 fd[0]는 파이프 출구, fd[1]은 파이프 입구 즉, 파이프로부터 데이터를 받고자 할 때는 fd[0], 파이프로부터 데이터를 출력하고자 할 때 fd[1]이다.

 원래 목표는 fork(), exec(), pipe(), dup2(), waitpid(), close() 시스템 콜을 적절하게 사용하여 depth 1 파이프를 지원하도록 구현하는 것(예: "ls -al | wc -l")이지만 기능을 확장하여 depth 20 파이프도 동작 가능하도록 구현하는 것을 목표로 한다.

 

### Pseudocode
```
int main(int argc, char **argv)

{

​      while(1)

​      {

​           // let's use GNU readline()

​           while(1) { // readline() 함수와 while문을 이용해 제대로 된 명령이 입력될 때까지 무한루프. 사용자가 Enter키만 입력하면 다시 명령 입력 창이 반복 출력됨. 

​                cmdLine = readline(printPrompt()); // read a line

​                info = parseCommand(cmdLine); // 입력 받은 명령어를 parsing

​                if(info == NULL)

​                     continue;

​                add_history(cmdLine); // 사용자가 입력한 명령어를 history에 저장

​                break;

​           }

​           

​           if(isBuiltInCommand(info)) { // 사용자로부터 입력 받은 명령어가 직접 구현한 내장 명령어인지 판별. 아니면 else문으로 이동

​                if(info->flagPipe) // 사용자로부터 입력 받은 명령어 중 pipe(|) 기호가 있으면 cmdPipe() 함수 실행

​                     cmdPipe(info);

​                else if((info->flagInfile) || (info->flagOutfile) || (info->flagAppendOutfile)) // 사용자로부터 입력 받은 명령어 중 입출력 리다이렉션(">", ">>", "<")이 있으면 cmdRedirection()함수 실행

​                     cmdRedirection(info);

​                else

​                     executeBuiltInCommand(info); // 입력 받은 명령어가 직접 구현한 내장 명령어일 경우 executeBuiltInCommand() 함수 실행

​           }

​           else { // 입력 받은 명령어가 직접 구현한 명령이 아닐 경우 else 아래의 코드 실행

​                childPid = vfork(); // 자식프로세스가 먼저 실행되는 것을 보장하고 부모프로세스와의 변수를 공유하기 위해 fork() 함수가 아닌 vfork() 함수 사용

​                if(childPid == 0) { // 자식 프로세스

​                     executeCommand(info); // 이미 구현되어 있는 명령을 exec() 함수의 사용을 통해 내부적으로 실행

​                }

​                else { // 부모 프로세스

​                     if(isBackgroundJob(info)) { // 사용자로부터 입력 받은 명령어가background process(&)인지 판별

​                           waitpid(childPid, &status, WNOHANG);

​                     }

​                     else { // no backgrounds

​                           waitpid(childPid, &status, 0);

​                     }

​      } // end of while

}

 

int isBuiltInCommand(parseInfo *info)

{ 

// 사용자로부터 입력 받은 명령어 중 입출력 리다이렉션(">", ">>", "<")이 있거나 직접 구현한 내장 명령어("cd", "kill", "exit", "jobs")일 경우 1(BUILTIN_CMD)을 반환, 아니면 0(NO_SUCH_BUILTIN)을 반환

​      if ((info->flagInfile) || (info->flagOutfile) || (info->flagAppendOutfile)) {

​           return BUILTIN_CMD; // 입출력 리다이렉션인가?

​      }

​      if (info->flagPipe) { // pipe인가?

​           return BUILTIN_CMD;

​      }

​      if (strncmp(cmd, "cd", strlen("cd")) == 0){ // cd인가?

​           return BUILTIN_CMD;

​      }

​      if (strcmp(cmd, "kill") == 0){ .// kill인가?

​           return BUILTIN_CMD;

​      }

​      if (strncmp(cmd, "exit", strlen("exit")) == 0){ // exit인가?

​           return BUILTIN_CMD;

​      }

​      if (strncmp(cmd, "jobs", strlen("jobs")) == 0){ // jobs인가?

​           return BUILTIN_CMD;

​      }

​      return NO_SUCH_BUILTIN;

}

 

void executeBuiltInCommand(parseInfo *info) {

// 사용자로부터 입력 받은 명령어가 직접 구현한 내장 명령어("cd", "kill", "exit", "jobs")일 경우 조건문(if)에 따라 알맞은 명령어 수행

​      if(!(strcmp(cmd, "exit"))) { // 명령어가 exit인가?

​           if(backgroundNum != 0) { // 백그라운드 프로세스가 수행 중일 경우 exit 명령 실행 불가

​                puts("Running background process"); 

​                return;

​           }

​           clear_history(); // history를 비운다.

​           puts("Bye bye. MyShell is finished.");

​           exit(0);

​      }

​      else if(!(strcmp(cmd, "jobs"))) { // 명령어가 jobs인가?

​           if(backgroundNum != 0) { // 백그라운드 프로세스가 있을 경우

​                for(i=0; i < backgroundNum; i++) // 백그라운드 프로세스명 출력

​                     printf("[%d] %s\n", i+1, bStr[i]);

​                return;

​           }

​           else{ // 백그라운드 프로세스가 없을 경우

​                printf("No background jobs.\n"); 

​                return;

​           }

​      }

​      else if(!(strcmp(cmd, "cd"))) { // 명령어가 cd인가?

​           if(comm->VarList[1] == NULL) { 

​                     chdir(getenv("OLDPWD"));

​                     getcwd(buff, MAX);

​                     setenv("PWD", buff, 1);

​                     return;

​           }

​                getcwd(buff, MAX);

​                setenv("OLDPWD", buff, 1); // 환경 변수 OLDPWD에 이전의 실행 경로를 저장

​                chdir(comm->VarList[1]); // 출력 경로(path)를 변경

​                getcwd(buff, MAX); // 현재의 실행 경로(path)를 받아 buff에 저장

​                setenv("PWD", buff, 1); // 현재의 실행 경로(path)를 환경 변수 PWD에 저장

​                return;

​      }

​      else if(!(strcmp(cmd, "kill"))) { // 명령어가 kill인가?

​           i = comm->VarList[1][1] - ASCII;

​           i--;

​           printf("[%d] %s terminated\n",jobNum, bStr[i]); // 종료된 백그라운드 프로세스명 출력

​           backgroundNum--;

​            jobNum--;

​           if(backgroundNum == 0)

​                info->flagBackground = 0;

​           kill(bPid[i], SIGTERM); // 해당 백그라운드 프로세스를 kill

​           waitpid(bPid[i], &status, 0); // 좀비 프로세스가 되는 것을 방지

​      }

}

 

void cmdRedirection(parseInfo *info)

{

​      if((pid=fork()) < 0) // fork() 함수 실행

​           err_sys("fork error!");

​      else if(pid != 0){ // 부모 프로세스

​           waitpid(pid, &status, 0);

​      }

​      else { // 자식 프로세스

​           if(info->flagOutfile){ // 입력 받은 명령어 중 (">")가 있을 경우

​               if(info->flagInfile){ // 입력 받은 명령어 중 ("<")가 있을 경우

​                   fd2 = open(info->inFile, O_RDONLY); //open() 시스템 콜을 이용해 해당 파일을 지정된 옵션(O_RDONLY: 읽기 전용)으로 open하고 해당 파일 디스크립터를 fd2에 반환

​                   if(fd2 < 0){

​                        err_sys(info->inFile);

​                        exit(1);

​                   }

​                  if(dup2(fd2, STDIN_FILENO) == -1) // fd2을 표준 입력으로 redirection

​                       err_sys("dup2 error!");

​                   close(fd2); // 열려진 파일을 닫음

​               }

​                if((fd1 = open(info->outFile, O_WRONLY | O_CREAT | O_TRUNC, 0644))== -1){ // open() 시스템 콜을 이용해 해당 파일을 지정된 옵션(O_WRONLY | O_CREAT | O_TRUNC : 쓰기 전용, 파일 생성, 덮어 쓰기, 0644는 해당 파일 접근 권한(permission) 계산을 위해 사용)으로 open하고 해당 파일 디스크립터를 fd1에 반환

​                     err_sys("File error!");

​                     exit(-1);

​                }

​                if(dup2(fd1, 1)==-1) // fd1을 표준 출력으로 redirection

​                     err_sys("Redirection error!");

​                execvp(cmd, comm->VarList); // execvp() 함수를 통해 명령어 실행

​                close(fd1); // 열려진 파일을 닫음

​                exit(-1);

​           }

​           else if(info->flagInfile){ // 입력 받은 명령어 중 ("<")가 있을 경우

​                fd2 = open(info->inFile, O_RDONLY); //open() 시스템 콜을 이용해 해당 파일을 지정된 옵션(O_RDONLY: 읽기 전용)으로 open하고 해당 파일 디스크립터를 fd2에 반환

​                if(fd2 < 0){

​                     err_sys(info->inFile);

​                     exit(1);

​                }

​                if(dup2(fd2, STDIN_FILENO) == -1) // fd2을 표준 입력으로 redirection

​                     err_sys("dup2 error!");

​                execvp(cmd, comm->VarList); // execvp() 함수를 통해 명령어 실행

​                close(fd2); // 열려진 파일을 닫음

​                exit(-1);

​           }

​           else if(info->flagAppendOutfile){ // 입력 받은 명령어 중 (">>")가 있을 경우

​               if(info->flagInfile){ // 입력 받은 명령어 중 ("<")가 있을 경우

​                   fd2 = open(info->inFile, O_RDONLY); //open() 시스템 콜을 이용해 해당 파일을 지정된 옵션(O_RDONLY: 읽기 전용)으로 open하고 해당 파일 디스크립터를 fd2에 반환

​                   if(fd2 < 0){

​                        err_sys(info->inFile);

​                        exit(1);

​                   }

​                  if(dup2(fd2, STDIN_FILENO) == -1) // fd2을 표준 입력으로 redirection

​                       err_sys("dup2 error!");

​                   close(fd2); // 열려진 파일을 닫음

​               }

​                if((fd1 = open(info->outFile, O_WRONLY | O_CREAT | O_APPEND, 0644))== -1){ // open() 시스템 콜을 이용해 해당 파일을 지정된 옵션(O_WRONLY | O_CREAT | O_APPEND : 쓰기 전용, 파일 생성, 파일의 끝에 Append, 0644는 해당 파일 접근 권한(permission) 계산을 위해 사용)으로 open하고 해당 파일 디스크립터를 fd1에 반환

​                     err_sys("File error!");

​                     exit(-1);

​                }

​                if(dup2(fd1, 1) < 0) // fd1을 표준 출력으로 redirection

​                     err_sys("Redirection Error!");

​                execvp(cmd, comm->VarList); // execvp() 함수를 통해 명령어 실행

​                close(fd1); // 열려진 파일을 닫음

​                exit(-1);

​           }

}

 

void cmdPipe(parseInfo *info)

{     

​      if((pid=fork()) < 0) // fork() 함수 실행

​           err_sys("fork error!");

​      else if(pid != 0) { // 부모 프로세스

​           waitpid(pid, &status, 0);

​      }     

​      else { // 자식 프로세스

​           for(i = 0; i < info->numPipe; i++){ // 파이프의 개수 만큼 loop

​                comm=&(info->CommArray[i]);

​                cmd = comm->command;

​                pipe(pipe_id); // pipe() 함수 호출

​                switch(fork()) { // fork() 함수 호출

​                     default : // 부모 프로세스

​                           dup2(pipe_id[1], STDOUT_FILENO); // pipd_id[1]을 표준 출력으로 redirection

​                           close(pipe_id[0]); // 열려진 파일을 닫음

​                           execvp(cmd, comm->VarList); // execvp() 함수를 통해 해당 명령어 실행

​                           exit(-1);

​                     case 0 : // 자식 프로세스

​                           dup2(pipe_id[0], STDIN_FILENO); // pipd_id[0]를 표준 입력으로 redirection

​                           close(pipe_id[1]); // 열려진 파일을 닫음

​                           break;

​                     case -1 : 

​                           err_sys("Pipe error!");

​                }

​           }

​           comm=&(info->CommArray[info->numPipe]);

​           cmd = comm->command;

​           execvp(cmd, comm->VarList); // 파이프의 마지막 명령어 실행

​           exit(-1);

}

 

void executeCommand(parseInfo *info) {

​      if(info->flagBackground == 1) { // 백그라운드 프로세스일 경우

​           printf("[%d] %d\n", jobNum+1, getpid()); // 백그라운드 프로세스의 pid 출력

​           execlp(cmd, cmd, (char*)0); // 백그라운드 프로세스 실행

​            printf("command not found\n"); // 해당 명령어가 존재하지 않을 경우 에러

​           exit(-1);

​      }

​                

​      if(info->flagBackground == 0) { // 백그라운드 프로세스가 아닐 경우

​           execvp(cmd, comm->VarList); // 해당 명령어 실행

​           printf("command not found\n"); // 해당 명령어가 존재하지 않을 경우 에러  

​           exit(-1);

​      }

}

 

int isBackgroundJob(parseInfo *info) {

​      if(info->flagBackground == 1 || backgroundNum != 0) { // 백그라운드 프로세스 일 경우

​           strcpy(bStr[backgroundNum], cmd); // 백그라운드 프로세스명 copy

​           return 1; // 백그라운드 프로세스라면 1을 반환

​      }

​      else // 백그라운드 프로세스가 아니라면 0을 반환

​           return 0;

}
```
 

 

### 중요 모듈(함수)에 대한 설명

\- int isBuiltInCommand(parseInfo *info);

\* 기능 : 사용자가 입력한 명령이 직접 구현한 내장 명령어, 파이프, 입출력 리다이렉션의 여부 판단한다.

\* 입력 값 : "parse.h"에 정의되어 있는 parseInfo 구조체 포인터 변수(parseInfo *info)

\* 출력 값 : 참이면 1(BUILTIN_CMD)을 리턴, 거짓이면 0(NO_SUCH_BUILTIN)을 리턴

 

\- void executeBuiltInCommand(parseInfo *info);

\* 기능 : 사용자가 입력한 명령이 직접 구현한 내장 명령어일 경우 이 함수를 통해서 실행된다..

\* 입력 값 : "parse.h"에 정의되어 있는 parseInfo 구조체 포인터 변수(parseInfo *info)

\* 출력 값 : void

 

\- void cmdRedirection(parseInfo *info);

\* 기능 : 입출력 리다이렉션(">", "<", ">>")을 실행한다.

\* 입력 값 : "parse.h"에 정의되어 있는 parseInfo 구조체 포인터 변수(parseInfo *info)

\* 출력 값 : void

 

\- void cmdPipe(parseInfo *info);

\* 기능 : 파이프(|, pipe)를 실행한다. 최대 depth 20 pipe까지 실행 가능 하다.

\* 입력 값 : "parse.h"에 정의되어 있는 parseInfo 구조체 포인터 변수(parseInfo *info)

\* 출력 값 : void

 

\- void executeCommand(parseInfo *info);

\* 기능 : 입출력 리다이렉션이나 파이프가 없을 경우의 명령을 실행한다. 백그라운드 프로세스도 실행한다.

\* 입력 값 : "parse.h"에 정의되어 있는 parseInfo 구조체 포인터 변수(parseInfo *info)

\* 출력 값 : void

 

\- int isBackgroundJob(parseInfo *info);

\* 기능 : 사용자가 입력한 명령이 백그라운드 프로세스인지의 여부를 판단한다.

\* 입력 값 : "parse.h"에 정의되어 있는 parseInfo 구조체 포인터 변수(parseInfo *info)

\* 출력 값 : 백그라운드 프로세스이면 1을 리턴, 백그라운드 프로세스가 아니면 0을 리턴

 

 

\- 그 외의 사용된 라이브러리 함수들 설명
 \#include <unistd.h>

pid_t fork(void)

\* 기능 : 기존 프로세스가 새 프로세스를 생성한다.

\* 입력 값 : void

\* 출력 값 : 자식 프로세스의 경우 0, 부모 프로세스의 경우 자식 프로세스 ID, 오류 시 -1

 

\#include <unistd.h>

pid_t getpid(void)

\* 기능 : 호출한 프로세스의 pid(프로세스 ID)를 반환한다.

\* 입력 값 : void

\* 출력 값 : 호출한 프로세스의 프로세스 ID

 

\#include <sys/wait.h>

pid_t waitpid(pid_t pid, int *statloc, int options)

\* 기능 : waitpid 함수의 경우에는 여러 자식 프로세스 중에서 특정 자시의 종료 상태 값을 얻을 수 있다. WNOHANG 옵션을 사용해 단순 대기(blocking)을 방지 할 수 있고 작업 제어를 지원(WUNTRACED 옵션) 한다

\* 입력 값 : pid(프로세스 ID), 정수형 포인터 변수(statloc), 정수형 변수(options)

\* 출력 값 : 성공 시 프로세스 ID, 오류 시 -1

 

\#include <unistd.h>

int execlp(const char *filename, const char *arg0, ... /* (char *)0 */ )

\* 기능 : 해당 파일 명을 입력 받아 받은 인자 값을 파일에 전달하면서 실행

\* 입력 값 : 실행 할 파일명(const char *filename), 인자 값(const char *arg0, ... /* (char *)0 */)

int execvp(const char *filename, const argv []);

\* 입력 값 : 실행 할 파일명(const char *filename), 인자 값(const argv []))

\* 출력 값 : 오류 시 -1, 성공 시 반환되지 않음

 

\#include <unistd.h>

char *getcwd(char *buf, size_t size)

\* 기능 : 현재 작업 디렉터리의 전체 경로명(절대 경로명)을 알아낸다.

\* 입력 값 : 경로 명을 저장한 공간(char *buf), 최대 Size(size_t size)

\* 출력 값 : 성공 시 buf, 오류 시 NULL

 

\#include <unistd.h>

int chdir(const char *pathname)

\* 기능 : 경로 명을 받아 프로세스의 현재 디렉터리를 변경한다.

\* 입력 값 : 경로 명(const char *pathname)

\* 출력 값 : 성공 시 0, 오류 시 -1

 

\#include <sys/types.h>

\#include <sys/stat.h>

\#include <fcntl.h>

int open(const char* pathname, int oflag, .../*, mode_t mode */);

\* 기능 : 존재하는 파일을 열거나, 새로운 파일을 만드는 함수

\* 출력 값 : 성공하면 파일 디스크립터, 실패하면 -1

\* 입력 값 : pathname : 열거나 생성하고자 하는 파일의 이름

\- oflag : 플래그

\- 반드시 하나만 정의되어야 하는 플래그

\- O_RDONLY : 읽기 전용

\- O_WRONLY : 쓰기 전용

\- O_RDWR : 읽기 ,쓰기 전용

\- 중복 지정이 가능한 플래그(논리적으로 합당한 경우에만 가능)

\- O_APPEND : 모든 쓰기 작업은 파일의 끝에서 수행된다.

\- O_CREAT : 파일이 없을 경우 파일을 생성한다. (세 번째 인자: mode_t mode 필요)

\- O_EXCL : O_CREAT와 같이 쓰이며, 파일이 있는 경우에 error를 발생시킨다.

\- O_TRUNC : O_CREAT와 같이 쓰이며, 파일이 있는 경우에 기존 파일을 지운다.

\- O_NONBLOCK : blocking I/O를 nonblocking 모드로 바꾼다.

\- O_SYNC : 매 쓰기 연산마다 디스크 I/O가 발생하도록 설정한다.

\- mode : 새로운 파일을 만드는 경우, 접근 권한(permission) 계산을 위해 사용 (umask 값과 연산하여 permission 설정)

 

\#include <unistd.h>

int close(int filedes);

\* 기능 : 열려진 파일을 닫는다.

\* 출력 값 : 성공하면 0, 실패하면 -1

\* 입력 값 : filedes : 닫고자 하는 파일의 파일 디스크립터

\- 파일을 닫지 않더라도 프로세스가 종료하면 모든 열려진 파일들은 자동적으로 닫힌다.

 

\#include <unistd.h>

int dup2(int filedes, int filedes2);

\* 기능 : 사용 중인 파일 디스크립터를 복사

\* 출력 값 : 성공하면 할당 받은 파일 디스크립터 번호, 실패하면 -1

\- dup2() 함수는 filedes2를 리턴한다.

\- 파일 테이블의 해당 항목의 참조 계수를 1 증가 시킨다.

\- 파일 디스크립터 항목의 플래그 (FD_CLOEXEC)는 초기값 (0) 으로 설정된다.

\- dup2(fd, STDIN_FILENO) : fd를 표준 입력으로 redirection

\- dup2(fd, STDOUT_FILENO) : fd를 표준 출력으로 redirection

\- dup2(fd, STDERR_FILENO) : fd를 표준 에러로 redirection

 

 

 

### 프로그램 실행 환경

\- 운영체제 : Linux Ubuntu-13.10-desktop-amd64bit

\- 컴파일러 : gcc-4.8.1

 

1)   프로그램 실행 방법

 ![03](https://raw.githubusercontent.com/dhcho/myshell/master/images/03.jpg)

Makefile을 이용해 make로 myshell이라는 실행 파일을 생성한다. 

단순히 make라고 입력하면 자동으로 실행파일(myshell)이 현재 디렉터리에 생성된다.

위의 화면은 myshell을 실행 한 화면이다.

 

 ![04](https://raw.githubusercontent.com/dhcho/myshell/master/images/04.jpg)

 위의 화면은 출력 리다이렉션(">", ">>")을 사용하여 제대로 파일에 출력 되는지 볼 수 있는 화면이다. ls -al >> list1과 ps >> list1이 제대로 파일에 출력 되었음 cat 명령을 통해 알 수 있다. Append 리다이렉션(">>")의 경우 추가구현 한 부분이다. 보다시피 파일 Append도 정상적으로 동작하는 것을 확인 할 수 있다.

 ![05](https://raw.githubusercontent.com/dhcho/myshell/master/images/05.jpg)

 위의 화면은 파일 입출력 리다이렉션("<", ">")을 동시에 실행한 화면이다. 명령 cat < list1 > foo를 입력하면 list1의 파일 data를 입력으로 받아 foo 파일에 list1의 파일 data를 출력하고 있는 모습을 cat 명령어를 통해 정상적으로 동작함을 확인할 수 있다. 

 

 ![06](https://raw.githubusercontent.com/dhcho/myshell/master/images/06.jpg)

위의 화면에서는 입력 리다이렉션("<")을 사용하여 cat < list1 명령이 제대로 실행 되는지를

확인 할 수 있다. 또한 kill %1로도 백그라운드 프로세스를 종료할 수 있도록 개선하였다. kill %1 명령과 jobs 명령을 통해 해당 백그라운드 프로세스가 정상적으로 종료되는 모습을 확인할 수 있다.

 

 ![07](https://raw.githubusercontent.com/dhcho/myshell/master/images/07.jpg)

depth 1 파이프를 실행 했을 때의 화면이다 명령어는 ls -l | wc이며 정상적으로 실행 되는 모습을 확인 할 수 있다.

 

 ![08](https://raw.githubusercontent.com/dhcho/myshell/master/images/08.jpg)

 위의 화면은 추가구현 한 부분이다. depth 1 파이프까지 구현하는 것이지만 추가구현 한 부분에서는 depth 20 파이프까지 실행이 가능하다. 명령어 ls -al | sort -r | head -n 5인 depth 3 파이프 명령도 제대로 동작하며 ls -al | sort -r | head -n 5 | wc인 depth 4 파이프 명령, ls -al | sort -r | head -n 5 | wc > list2인 depth 4 파이프 명령 + 입출력 리다이렉션의 복합적인 명령도 정상적으로 수행되는 것 cat 명령을 통해 확인 할 수 있다.

 

 ![09](https://raw.githubusercontent.com/dhcho/myshell/master/images/09.jpg)

 위의 화면은 에러의 경우를 처리한 화면이다. 출력 파일을 지정하지 않았거나(ls > ) 출력파일을 복합적으로 지정한 경우 모두 에러로 처리한다. 다만 ls > out1 > out2 명령의 경우 실제 Shell에서 동작해본 결과 ls의 출력 결과가 out2로 저장되는 것을 확인할 수 있었다. 그러므로 실제 Shell과 동일하게 ls > out1 > out2는 에러로 처리하지 않고 실제 Shell과 동일하게 처리되도록 하였다.

 

# 결론

\-  Shell 구현을 통해 시스템 콜을 적재적소에 활용해봄으로써, 언제 어떻게 어떤 함수를 써야 하는지에 대해 명확히 알 수 있었다. 어려웠던 점은 pipe() 함수에 대한 이해, 활용 측면과 파일 입출력 리다이렉션을 위해 어떻게 시스템 콜을 활용하는지가 주요한 부분이었다. 하지만 이런 노력을 통해 실제 Shell에서 파이프가 어떤 방식으로 동작하고 리다이렉션이 어떻게 구현되어 있는지를 세부적으로 알 수 있었으며 실력향상에 매우 도움이 되었다고 생각한다.  더욱 완성도를 높이기 위해 depth 20 파이프까지 실행 가능하도록 하였는데 덕분에 파이프 함수(pipe())의 동작과정과 dup(), dup2() 함수의 활용측면을 확실히 이해할 수 있었다. 

 

1. 최대 depth 20 파이프까지 실행 가능합니다. (parse.h 파일에서 PIPE_MAX_NUM의 상수 값만 바꿔주면 파이프 최대 개수의 조정이 가능합니다.)

 

2. 파이프 + 입출력 리다이렉션의 복합 사용이 가능합니다. 예를 들어 ls -al | sort -r | head -n 5 | wc > list2 명령을 입력하였을 때 최종 파이프 처리 된 결과가 list2에 출력됩니다.

 

3. 파일 출력(Append) 리다이렉션(">>")이 사용가능 합니다. 기호(">>")를 쓸 경우 기존 파일의 끝에 data를 Append합니다.

 

4. kill 명령을 실행할 경우 pid를 인자 값으로 줘야 백그라운드 프로세스를 종료할 수 있었지만 이를 개선하여 kill %1 명령으로 백그라운드 프로세스가 종료되도록 하였습니다.

 

5. 사소한 예외들을 처리하였습니다. 기존에는 사용자가 명령어 입력 없이 Enter 키를 누를 경우 세그먼테이션 fault가 발생해서 프로그램이 강제 종료 되었지만 실제 Shell과 동일하게 아무 입력 없이 Enter 키를 누를 경우 다시 명령어를 재입력 받도록 개선하였습니다.

