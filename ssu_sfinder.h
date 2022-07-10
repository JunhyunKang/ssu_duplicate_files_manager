#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <dirent.h>
#include <ctype.h>
#include <math.h>
#include <openssl/sha.h>
#define NAMEMAX 255
#define PATHMAX 4096
#define TIME_LENGTH 9
#define DATE_LENGTH 11

#ifdef HASH_SHA1
	#define HASHMAX 41
#else
	#define HASHMAX 33
#endif

#define STRMAX 10000 
#define ARGMAX 11

typedef struct fileInfo {
	char path[PATHMAX];
	struct stat statbuf;
	struct fileInfo *next;
} fileInfo;

typedef struct fileList {
	long long filesize;
	char hash[HASHMAX];
	fileInfo *fileInfoList;
	struct fileList *next;
} fileList;

typedef struct dirList {
	char dirpath[PATHMAX];
	struct dirList *next;
} dirList;

typedef struct trashList{
	char path[PATHMAX];
	long long size;
	char date[DATE_LENGTH];
	char time[TIME_LENGTH];
	char trashed[PATHMAX];
	struct trashList *next;
} trashList;

#define DIRECTORY 1
#define REGFILE 2

#define KB 1024
#define MB 1048576
#define GB 1073741824
#define SIZE_ERROR -2


int find_sha1(int argc, char *argv[]);
int trash_command(int argc, char *argv[]);
int list_command(int argc, char *argv[]);
int restore_command(int argc, char *argv[]);



