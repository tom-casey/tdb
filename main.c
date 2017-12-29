#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ptrace.h>
typedef struct  {
	long data;
	void *addr;
} Breakpoint;
int fork_to_child(int argc,char *argv[]);
char** str_split(char* a_str, const char a_delim)
{
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = malloc(sizeof(char*) * count);

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
            assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        assert(idx == count - 1);
        *(result + idx) = 0;
    }

    return result;
}
int main(int argc, char *argv[]){
	if (argc < 2) {
		fprintf(stderr, "Usage: %s [OPTION] \"program to debug\"",argv[0]);
	}
	char* brkpt;
	pid_t pid = -1;
	int sflag = 0;
	int c;
	while((c=getopt(argc,argv,"sb:p:"))!=-1){
		switch(c){
			case 's':
				sflag = 1;
				break;
			case 'b':
				brkpt = optarg;
				break;
			case 'p':
				pid = (pid_t) atoi(optarg);
				break;
			case '?':
				fprintf(stderr,"Bad option -%c",optopt);
			default:
				exit(EXIT_FAILURE);
		}
	}
	char** bps = NULL;
	if(brkpt){
		bps = str_split(brkpt,',');
	}
	if(pid == -1) {
		pid = fork_to_child(argc-optind,argv[optind]);
	}
	else {
		ptrace(PTRACE_ATTACH, pid, NULL, 0);
	}
	if(sflag == 1){
		ptrace(PTRACE_SINGLESTEP,pid,NULL,0);
	}
	int status;
	while(pid == waitpid(pid,&status,0)){
		if (WIFEXITED(status)){
			printf("Child exited with status %d\n",WEXITSTATUS(status));
		}
		if (WIFSIGNALED(status)){
			printf("Child got killing signal %d\n",WTERMSIG(status));
		}
		if (WIFSTOPPED(status)){
			redo:
			printf("Child caught signal %d\n(q)uit\t(r)egisters\t(s)inglestep\t(c)ontinue\n",WSTOPSIG(status));
			char input = getchar();
			switch(input){
				case 'q':
					exit(EXIT_SUCCESS);
				case 'r':
					printregs(pid);
					break;
				case 's':
					ptrace(PTRACE_SINGLESTEP,pid,NULL,0);
				case 'c':
					break;
				default:
					goto redo;
			}
			ptrace(PTRACE_CONT, pid, NULL, 0);
		}
	}
}
int fork_to_child(int argc, char *argv[]){
	pid_t childpid = fork();
	if (childpid == 0){
		ptrace(PTRACE_TRACEME,0,NULL,NULL);
		char **args = malloc(sizeof(argv[0])*(argc+1));
		memcpy(args,argv,sizeof(argv[0])*argc);
		args[argc]=NULL;
		execvp(args[0],args);
	}
	return childpid;
}
	
int insertbp(Breakpoint bp, pid_t pid){
	long data = ptrace(PTRACE_PEEKDATA,pid,bp.addr,NULL);
	//replace last byte in data with our INT 3(0xcc) instruction
	ptrace(PTRACE_POKEDATA,pid,bp.addr,(data|0xcc));
}
