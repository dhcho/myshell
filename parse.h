#define MAX_VAR_NUM 20
#define PIPE_MAX_NUM 20
#define FILE_MAX_SIZE 40

// for single command
struct commandType {
	char *command;  
	char *VarList[MAX_VAR_NUM];  
	int VarNum;
} commandType;

/* parsing information structure */
typedef struct {
	struct commandType CommArray[PIPE_MAX_NUM];
	int   flagInfile;
	int   flagOutfile;
	int   flagAppendOutfile;
	int   flagBackground;
	int   flagPipe;
	int   numPipe;  
	char  inFile[FILE_MAX_SIZE];
	char  outFile[FILE_MAX_SIZE];
} parseInfo;

/* the function prototypes */
void initInfo(parseInfo *);
void extractCommand(char *, struct commandType *);
parseInfo *parseCommand(char *);
void freeInfo(parseInfo *);
