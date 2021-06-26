#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "parse.h" 

#define MAXLINE 81

void initInfo(parseInfo *p) {
	int i;
	p->flagInfile=0;
	p->flagOutfile=0;
	p->flagAppendOutfile=0;
	p->flagBackground=0;
	p->flagPipe=0;
	p->numPipe=0;
	
	for (i=0; i<PIPE_MAX_NUM; i++) {
		p->CommArray[i].command=NULL;
        p->CommArray[i].VarList[0]=NULL;
        p->CommArray[i].VarNum=0;
  }
}

void extractCommand(char * command, struct commandType *comm) {
  int i=0;
  int pos=0;
  char word[MAXLINE];

  comm->VarNum=0;
  comm->command=NULL;
  comm->VarList[0]=NULL;
  while(isspace(command[i]))
    i++; 
  if (command[i] == '\0') 
    return;
  while (command[i] != '\0') {
    while (command[i] != '\0' && !isspace(command[i])) {
      word[pos++]=command[i++];
    }
    word[pos]='\0';
    comm->VarList[comm->VarNum]=malloc((strlen(word)+1)*sizeof(char));
    strcpy(comm->VarList[comm->VarNum], word);
    comm->VarNum++;
    word[0]='\0';
    pos=0;
    while(isspace(command[i]))
      i++; 
  }
  comm->command=malloc((strlen(comm->VarList[0])+1)*sizeof(char));
  strcpy(comm->command, comm->VarList[0]);
  comm->VarList[comm->VarNum]=NULL;
}

parseInfo *parseCommand (char *cmdline) {
  parseInfo *Result;
  int i=0;
  int pos;
  int end=0;
  char command[MAXLINE];
  int com_pos;

  if (cmdline[i] == '\n' || cmdline[i] == '\0')
    return NULL;

  Result = malloc(sizeof(parseInfo));
  initInfo(Result);
  com_pos=0;
  while (cmdline[i] != '\n' && cmdline[i] != '\0') {
    if (cmdline[i] == '&') {
      Result->flagBackground=1;
      if (cmdline[i+1] != '\n' && cmdline[i+1] != '\0') {
		fprintf(stderr, "Ignore anything beyond &.\n");
      }
      break;
    }
    else if (cmdline[i] == '<') {
      Result->flagInfile=1;
      while (isspace(cmdline[++i]));
      pos=0;
      while (cmdline[i] != '\0' && !isspace(cmdline[i])) {
		if (pos==FILE_MAX_SIZE) {
			fprintf(stderr, "Error.The input redirection file name exceeds the size limit 40\n");
			freeInfo(Result);
			return NULL;
		}
		Result->inFile[pos++] = cmdline[i++];
      }
      Result->inFile[pos]='\0';
      end=1;
      while (isspace(cmdline[i])) {
		if (cmdline[i] == '\n') 
			break;
		i++;
		}
    }   
    else if ((cmdline[i] == '>' ) && (cmdline[i+1] == '>')) {
	  Result->flagAppendOutfile=1;
	  i++;
      while (isspace(cmdline[++i]));
      pos=0;
      while (cmdline[i] != '\0' && !isspace(cmdline[i])) {
		if (pos==FILE_MAX_SIZE) {
			fprintf(stderr, "Error.The output redirection file name exceeds the size limit 40\n");
			freeInfo(Result);
			return NULL;
		}
	  Result->outFile[pos++] = cmdline[i++];
      }
      Result->outFile[pos]='\0';
      end=1;
      while (isspace(cmdline[i])) {
		if (cmdline[i] == '\n') 
			break;
		i++;
      }
    }
    else if (cmdline[i] == '>') {
      Result->flagOutfile=1;
      while (isspace(cmdline[++i]));
      pos=0;
      while (cmdline[i] != '\0' && !isspace(cmdline[i])) {
		if (pos==FILE_MAX_SIZE) {
			fprintf(stderr, "Error.The output redirection file name exceeds the size limit 40\n");
			freeInfo(Result);
			return NULL;
		}
	  Result->outFile[pos++] = cmdline[i++];
      }
      Result->outFile[pos]='\0';
      end=1;
      while (isspace(cmdline[i])) {
		if (cmdline[i] == '\n') 
			break;
		i++;
      } 
    }
    else if (cmdline[i] == '|') {
      command[com_pos]='\0';
      Result->flagPipe=1;
      extractCommand(command, &Result->CommArray[Result->numPipe]);
      com_pos=0;
      end=0;
      Result->numPipe++;
      i++;
    }
    else {
      if (end == 1) {
		fprintf(stderr, "Error. Wrong format of input\n");
		freeInfo(Result);
		return NULL;
      }
      if (com_pos == MAXLINE-1) {
		fprintf(stderr, "Error. The command length exceeds the limit 80\n");
		freeInfo(Result);
		return NULL;
      }
      command[com_pos++] = cmdline[i++];
    }
  }
  command[com_pos]='\0';
  extractCommand(command, &Result->CommArray[Result->numPipe]);
  return Result;
}

void freeInfo (parseInfo *info) {
  int i,j;
  struct commandType *comm;

  if (NULL == info) return;
  for (i=0; i<PIPE_MAX_NUM;i++) {
    comm=&(info->CommArray[i]);
    for (j=0; j<comm->VarNum; j++) 
      free(comm->VarList[j]);
    if (NULL != comm->command) free(comm->command);
  }
  free(info);
}