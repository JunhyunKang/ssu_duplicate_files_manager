#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "ssu_sfinder.h"

#define STRMAX 10000 
#define HASH_SHA1

int tokenize_main(char *input, char *argv[]);
void help(void);

int main(void)
{
	int order=0;
	while (1) {
		char input[STRMAX];
		char *argv[ARGMAX];

		optind=1;
		int i;
		int argc = 0;
		pid_t pid;
		

		printf("20202925> ");
		fgets(input, sizeof(input), stdin);
		input[strlen(input) - 1] = '\0';
		
		argc = tokenize_main(input, argv);
		argv[argc] = (char *)0;
		
		if (argc == 0)
			continue;

		if (!strcmp(argv[0], "exit"))
			break;
		
		if (!strcmp(argv[0], "fsha1")){
			order++;
			find_sha1(argc, argv);
		}	
		else if (!strcmp(argv[0], "list")){
			if(order>0){

				list_command(argc, argv);
			}
			else{
				printf("first use fsha1\n");
				continue;
			}
		}
		else if (!strcmp(argv[0], "trash"))
				trash_command(argc, argv);

		else if (!strcmp(argv[0], "restore")){
			if(order>0){

				restore_command(argc, argv);
			}
			else{
				printf("first use fsha1\n");
				continue;
			}
		}

		else{
		
			help();
		}		
	}

	printf("Prompt End\n");
	
	return 0;
}

int tokenize_main(char *input, char *argv[])
{
	char *ptr = NULL;
	int argc = 0;
	ptr = strtok(input, " ");

	while (ptr != NULL){
		argv[argc++] = ptr;
		ptr = strtok(NULL, " ");
	}
	return argc;
}


void help(void){
    printf("Usage:\n");
    printf("  > fsha1 -e [FILE_EXTENSION] -l [MINSIZE] -h [MAXSIZE] -d [TARGET_DIRECTORY] -t [THREAD_NUM]\n");
    printf("     >> delete -l [SET_INDEX] -d [OPTARG] -i -f -t\n");
    printf("  > trash -c [CATEGORY] -o [ORDER]\n");
    printf("  > restore [RESTORE_INDEXi]\n");
    printf("  > help\n");
    printf("  > exit\n\n");
}
