#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <signal.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "parse.h"   //include declarations for parse-related structs 

#define MAX 256
#define ASCII 48

enum BUILTIN_COMMANDS { NO_SUCH_BUILTIN=0, BUILTIN_CMD};

void err_sys(const char* x);
char* printPrompt();
int isBuiltInCommand(parseInfo *info);
void executeBuiltInCommand(parseInfo *info);
void cmdRedirection(parseInfo *info);
void cmdPipe(parseInfo *info);
void executeCommand(parseInfo *info);
int isBackgroundJob(parseInfo *info);

char bStr[MAX][MAX];
char buff[MAX];
int jobNum = 0;
int backgroundNum = 0;
pid_t bPid[MAX];

int main(int argc, char **argv)
{
	while(1)
	{
		parseInfo *info;
		pid_t childPid;
		int status;
		int i = 0;
		char *cmdLine;
		
		// let's use GNU readline()
		while(1) {
			cmdLine = readline(printPrompt()); // read a line
			info = parseCommand(cmdLine);
			if(info == NULL)
				continue;
			add_history(cmdLine);
			break;
		}
		
		if(isBuiltInCommand(info)) {
			if(info->flagPipe)
				cmdPipe(info);
			else if((info->flagInfile) || (info->flagOutfile) || (info->flagAppendOutfile))
				cmdRedirection(info);
			else
				executeBuiltInCommand(info);
		}
		else {
			childPid = vfork();
			if(childPid == 0) {
				bPid[i] = getpid();
				i++;
				executeCommand(info); // calls execvp
			}
			else {
				if(isBackgroundJob(info)) { // backgrounds
					usleep(50000);
					waitpid(childPid, &status, WNOHANG);
				}
				else {
					usleep(50000);
					waitpid(childPid, &status, 0);
				}
			}
		}
		freeInfo(info);
	} // end of while
}

void err_sys(const char* x)
{
	perror(x);
	exit(1);
}
 
char * printPrompt()
{
	char * str;
	printf("msh:");
	str = getcwd(buff, MAX);
	strcat(str, "% ");
	return str;
}
    
int isBuiltInCommand(parseInfo *info)
{ 
	struct commandType *comm;
	comm = &(info->CommArray[0]);
	char *cmd = comm->command;
	if ((info->flagInfile) || (info->flagOutfile) || (info->flagAppendOutfile)) {
		return BUILTIN_CMD;
	}
	if (info->flagPipe) {
		return BUILTIN_CMD;
	}
	if (strncmp(cmd, "cd", strlen("cd")) == 0){
		return BUILTIN_CMD;
	}
	if (strcmp(cmd, "kill") == 0){
		return BUILTIN_CMD;
	}
	if (strncmp(cmd, "exit", strlen("exit")) == 0){
		return BUILTIN_CMD;
	}
	if (strncmp(cmd, "jobs", strlen("jobs")) == 0){
		return BUILTIN_CMD;
	}
	return NO_SUCH_BUILTIN;
}

void executeBuiltInCommand(parseInfo *info) {
	int status;
	int i;
	struct commandType *comm;
	comm = &(info->CommArray[0]);
	char *cmd = comm->command;
	if(!(strcmp(cmd, "exit"))) {
		if(backgroundNum != 0) {
			puts("Running background process"); 
			return;
		}
		clear_history();
		puts("Bye bye. MyShell is finished.");
		exit(0);
	}
	else if(!(strcmp(cmd, "jobs"))) {
		if(backgroundNum != 0) {
			for(i=0; i < backgroundNum; i++)
				printf("[%d] %s\n", i+1, bStr[i]);
			return;
		}
		else{
			printf("No background jobs.\n");
			return;
		}
	}
	else if(!(strcmp(cmd, "cd"))) {
		if(comm->VarList[1] == NULL) { 
				chdir(getenv("OLDPWD"));
				getcwd(buff, MAX);
				setenv("PWD", buff, 1);
				return;
		}
			getcwd(buff, MAX);
			setenv("OLDPWD", buff, 1);
			chdir(comm->VarList[1]);
			getcwd(buff, MAX);
			setenv("PWD", buff, 1);
			return;
	}
	else if(!(strcmp(cmd, "kill"))) {
		i = comm->VarList[1][1] - ASCII;
		i--;
		printf("[%d] %s terminated\n",jobNum, bStr[i]);
		backgroundNum--;
		jobNum--;
		if(backgroundNum == 0)
			info->flagBackground = 0;
		kill(bPid[i], SIGTERM);
		waitpid(bPid[i], &status, 0);
	}
	else
		return;
}

void cmdRedirection(parseInfo *info)
{
	int fd1, fd2;
	pid_t pid;
	struct commandType *comm;
	char *cmd;
	int status;
	comm=&(info->CommArray[0]);
	cmd = comm->command;

	if((pid=fork()) < 0)
		err_sys("fork error!");
	else if(pid != 0){
		waitpid(pid, &status, 0);
	}
	else {
		if(info->flagOutfile){
			if(info->flagInfile){
				fd2 = open(info->inFile, O_RDONLY);
				if(fd2 < 0){
					err_sys(info->inFile);
					exit(1);
				}
				if(dup2(fd2, STDIN_FILENO) == -1)
					err_sys("dup2 error!");
				close(fd2);
			}
			if((fd1 = open(info->outFile, O_WRONLY | O_CREAT | O_TRUNC, 0644))== -1){
				err_sys("File error!");
				exit(-1);
			}
			if(dup2(fd1, 1)==-1)
				err_sys("Redirection error!");
			execvp(cmd, comm->VarList);
			close(fd1);
			exit(-1);
		}
		else if(info->flagInfile){
			fd2 = open(info->inFile, O_RDONLY);
			if(fd2 < 0){
				err_sys(info->inFile);
				exit(1);
			}
			if(dup2(fd2, STDIN_FILENO) == -1)
				err_sys("dup2 error!");
			execvp(cmd, comm->VarList);
			close(fd2);
			exit(-1);
		}
		else if(info->flagAppendOutfile){
			if(info->flagInfile){
				fd2 = open(info->inFile, O_RDONLY);
				if(fd2 < 0){
					err_sys(info->inFile);
					exit(1);
				}
				if(dup2(fd2, STDIN_FILENO) == -1)
					err_sys("dup2 error!");
				close(fd2);
			}
			if((fd1 = open(info->outFile, O_WRONLY | O_CREAT | O_APPEND, 0644))== -1){
				err_sys("File error!");
				exit(-1);
			}
			if(dup2(fd1, 1) < 0)
				err_sys("Redirection Error!");
			execvp(cmd, comm->VarList);
			close(fd1);
			exit(-1);
		}
		return;
	}
}

void cmdPipe(parseInfo *info)
{
	int i;
	int pipe_id[2];
	int fd1, fd2;
	pid_t pid;
	int status;
	struct commandType *comm;
	char *cmd;
	
	if((pid=fork()) < 0)
		err_sys("fork error!");
	else if(pid != 0) {
		usleep(50000);
		waitpid(pid, &status, 0);
	}	
	else {
		for(i = 0; i < info->numPipe; i++){
			comm=&(info->CommArray[i]);
			cmd = comm->command;
			pipe(pipe_id);
			switch(fork()) {
				default : 
					dup2(pipe_id[1], STDOUT_FILENO);
					close(pipe_id[0]);
					execvp(cmd, comm->VarList);
					exit(-1);
				case 0 :
					dup2(pipe_id[0], STDIN_FILENO);
					close(pipe_id[1]);
					break;
				case -1 : 
					err_sys("Pipe error!");
			}
		}
		comm=&(info->CommArray[info->numPipe]);
		cmd = comm->command;
		if(info->flagOutfile){
			if(info->flagInfile){
				fd2 = open(info->inFile, O_RDONLY);
				if(fd2 < 0){
					err_sys(info->inFile);
					exit(1);
				}
				if(dup2(fd2, STDIN_FILENO) == -1)
					err_sys("dup2 error!");
				close(fd2);
			}
			if((fd1 = open(info->outFile, O_WRONLY | O_CREAT | O_TRUNC, 0644))== -1){
				err_sys("File error!");
				exit(-1);
			}
			if(dup2(fd1, 1)==-1)
				err_sys("Redirection error!");
			execvp(cmd, comm->VarList);
			close(fd1);
			exit(-1);
		}
		else if(info->flagInfile){
			fd2 = open(info->inFile, O_RDONLY);
			if(fd2 < 0){
				err_sys(info->inFile);
				exit(1);
			}
			if(dup2(fd2, STDIN_FILENO) == -1)
				err_sys("dup2 error!");
			execvp(cmd, comm->VarList);
			close(fd2);
			exit(-1);
		}
		else if(info->flagAppendOutfile){
			if(info->flagInfile){
				fd2 = open(info->inFile, O_RDONLY);
				if(fd2 < 0){
					err_sys(info->inFile);
					exit(1);
				}
				if(dup2(fd2, STDIN_FILENO) == -1)
					err_sys("dup2 error!");
				close(fd2);
			}
			if((fd1 = open(info->outFile, O_WRONLY | O_CREAT | O_APPEND, 0644))== -1){
				err_sys("File error!");
				exit(-1);
			}
			if(dup2(fd1, 1) < 0)
				err_sys("Redirection Error!");
			execvp(cmd, comm->VarList);
			close(fd1);
			exit(-1);
		}
		execvp(cmd, comm->VarList);
		exit(-1);
	}
}

void executeCommand(parseInfo *info) {
	struct commandType *comm;
	char *cmd;
	comm=&(info->CommArray[0]);
	cmd = comm->command;

	if(info->flagBackground == 1) {
		printf("[%d] %d\n", jobNum+1, getpid());
		execlp(cmd, cmd, (char*)0);
		printf("command not found\n");	
		exit(-1);
	}
			
	if(info->flagBackground == 0) {
		execvp(cmd, comm->VarList);
		printf("command not found\n");	
		exit(-1);
	}
}

int isBackgroundJob(parseInfo *info) {
	struct commandType *comm;
	comm = &(info->CommArray[0]);
	char *cmd = comm->command;
	if(info->flagBackground == 1 || backgroundNum != 0) {
		strcpy(bStr[backgroundNum], cmd);
		jobNum++;
		backgroundNum++;
		return 1;
	}
	else
		return 0;
}