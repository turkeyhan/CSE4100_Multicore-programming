/* $begin shellmain */
#include "csapp.h"
#include<errno.h>
#define MAXARGS   128

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv); 
void append_decision(char *cmdline); // dicide appending to history
int main(int argc, char** argv) 
{
    char cmdline[MAXLINE]; /* Command line */

    while (1) {
	/* Read */
	    printf("CSE4100-MP-P1> ");                   
	    char *clptr = fgets(cmdline, MAXLINE, stdin);
        append_decision(cmdline); // dicide appending to history
        if (feof(stdin)) exit(0);
	    /* Evaluate */
	    eval(cmdline);
    }
} 
void append_decision(char *cmdline){
    if(cmdline[0] != '!'){ // if operator's first character is '!', not append.
        FILE* appendfile = fopen("/sogang/under/cse20211606/history.txt", "a"); // For append 
        FILE* readfile = fopen("/sogang/under/cse20211606/history.txt", "r"); // For read
        
        /* If last line of history is same with cmdline, not append.*/
        char str[MAXLINE];
        int flag = 1;
        char laststr[MAXLINE];
        while(fgets(str, MAXLINE, readfile) != NULL){
            strcpy(laststr, str);
        }
        if(strcmp(laststr, cmdline)){ // laststr != cmdline
            fputs(cmdline, appendfile);
        }
        fclose(readfile);
        fclose(appendfile);
    }
}

/* $end shellmain */
  
/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline) 
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
    
    strcpy(buf, cmdline);
    bg = parseline(buf, argv); 
    if (argv[0] == NULL) return;   /* Ignore empty lines */
    if (!builtin_command(argv)) { //quit -> exit(0), & -> ignore, other -> run
        if((pid = fork()) == 0){ // if child process is successfully created
            char extmp[50] = "/bin/"; // for absolute route of bin PATH
            strcat(extmp, argv[0]); // Union the operator and bin PATH
            if (execve(extmp, argv, environ) < 0) {	//ex) /bin/ls ls -al &
                printf("%s: Command not found.\n", argv[0]);
                exit(0);
            }
        }
        /* Parent waits for foreground job to terminate */
        if (!bg){ 
	        int status;
            if(waitpid (pid , &status, 0) < 0) unix_error("waitfg : waitpid error");
	    }
        else printf("%d %s", pid, cmdline);//when there is backgrount process!
    }
	return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) 
{
    if (!strcmp(argv[0], "quit")) exit(0);/* quit command */
    if(!strcmp(argv[0], "exit")) exit(0); /*exit command*/
    if(!strcmp(argv[0], "cd")){ // cd command
        if(argv[1] == NULL) return 1; // if cd nothing, do nothing
        else{
            if(chdir(argv[1])){ // move to directory, if can't, print error message 
                printf("%s: Invalid path\n", argv[1]);
            }
            return 1;
        }
    }
    if(!strcmp(argv[0], "history")){ // history command
        FILE *readfile = fopen("/sogang/under/cse20211606/history.txt", "r"); // for read
        int num = 0; // for numbering to history
        char str[MAXLINE]; // approach each line of history
        while(fgets(str, MAXLINE, readfile) != NULL){ // store and print
            printf("%d  %s", ++num, str);
        }
        fclose(readfile);
        return 1;
    }
    if(!strcmp(argv[0], "!!")){ // !! command
        FILE *readfile = fopen("/sogang/under/cse20211606/history.txt", "r"); // for read
        char str[MAXLINE]; // approach each line of history
        char laststr[MAXLINE]; // last line of history
        while(fgets(str, MAXLINE, readfile) != NULL){ // find last line
            strcpy(laststr, str);
        }
        fclose(readfile);
        printf("%s", laststr); // print last line 
        eval(laststr); // execute last line arguments
        return 1;
    }
    if(argv[0][0] == '!'){ // !# command
        int flag = 1; // for # is numeral or not.
        char str[MAXARGS];
        for(int i = 1; i < strlen(argv[0]); i++){ // store # number to str
            if('0' > argv[0][i] || argv[0][i] > '9'){
                flag = 0;
                break;
            }
            else str[i-1] = argv[0][i];
        }
        if(flag){ // # is number
            int xx = 1; // for digit
            int Sum = 0; // int number
            for(int i = strlen(str) - 1; i >= 0; i--){ // change char array number to integer number
                Sum += ((str[i] - '0') * xx);
                xx *= 10;
            }
            FILE *readfile = fopen("/sogang/under/cse20211606/history.txt", "r");
            int num = 0; // for count line
            char readstr[MAXLINE];
            while(fgets(readstr, MAXLINE, readfile) != NULL){
                if(++num == Sum){ // line = #
                    flag = 0; // find flag
                    printf("%s", readstr); 
                    append_decision(readstr); // decide appending to history
                    eval(readstr); // execute # line arguments
                }
            }
            if(flag){ // can't find #line  in history
                printf("In history, !%d is not exist.\n", Sum);   
            }
            fclose(readfile);
            return 1;
        }
    }
    if (!strcmp(argv[0], "&")) return 1;   /* Ignore singleton & */
    return 0;                     /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) 
{
    char *delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */
    int bg;              /* Background job? */

    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
	    buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
	    argv[argc++] = buf;
	    *delim = '\0';
	    buf = delim + 1;
	    while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* Ignore blank line */
	    return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0) argv[--argc] = NULL;

    return bg;
}
/* $end parseline */


