#define HASH_SHA1
#include "ssu_sfinder.h"
#include <pwd.h>
#include <pthread.h>

char extension[10];
char same_size_files_dir[PATHMAX];
char logfile[PATHMAX];
char trash_d[PATHMAX];
char trash_path[PATHMAX];
char trash_info[PATHMAX];
char trashedTime[STRMAX];
char USER[NAMEMAX];

int threadnum;
long long minbsize;
long long maxbsize;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
fileList *dups_list_h;
trashList *trash_list;//trash 명령어에 사용되는 리스트

int find_sha1(int argc, char *argv[]);
int refind(void);
void *multi_find_duplicates(void *arg);

void fileinfo_append(fileInfo *head, char *path)
{
	fileInfo *fileinfo_cur;

	fileInfo *newinfo = (fileInfo *)malloc(sizeof(fileInfo));
	memset(newinfo, 0, sizeof(fileInfo));
	strcpy(newinfo->path, path);
	lstat(newinfo->path, &newinfo->statbuf);
	newinfo->next = NULL;

	if (head->next == NULL)
		head->next = newinfo;
	else {
		fileinfo_cur = head->next;
		while (fileinfo_cur->next != NULL)
			fileinfo_cur = fileinfo_cur->next;

		fileinfo_cur->next = newinfo;
	}
	
}

fileInfo *fileinfo_delete_node(fileInfo *head, char *path)
{
	fileInfo *deleted;

	if (!strcmp(head->next->path, path)){
		deleted = head->next;
		head->next = head->next->next;
		return head->next;
	}
	else {
		fileInfo *fileinfo_cur = head->next;

		while (fileinfo_cur->next != NULL){
			if (!strcmp(fileinfo_cur->next->path, path)){
				deleted = fileinfo_cur->next;
				
				fileinfo_cur->next = fileinfo_cur->next->next;
				return fileinfo_cur->next;
			}

			fileinfo_cur = fileinfo_cur->next;
		}
	}
}

int fileinfolist_size(fileInfo *head)
{
	fileInfo *cur = head->next;
	int size = 0;
	
	while (cur != NULL){
		size++;
		cur = cur->next;
	}
	
	return size;
}

void filelist_append(fileList *head, long long filesize, char *path, char *hash)
{
    fileList *newfile = (fileList *)malloc(sizeof(fileList));
    memset(newfile, 0, sizeof(fileList));

    newfile->filesize = filesize;
    strcpy(newfile->hash, hash);

    newfile->fileInfoList = (fileInfo *)malloc(sizeof(fileInfo));
    memset(newfile->fileInfoList, 0, sizeof(fileInfo));

    fileinfo_append(newfile->fileInfoList, path);
    newfile->next = NULL;

    if (head->next == NULL) {
        head->next = newfile;
    }    
    else {
        fileList *cur_node = head->next, *prev_node = head, *next_node;

        while (cur_node != NULL && cur_node->filesize < newfile->filesize) {
            prev_node = cur_node;
            cur_node = cur_node->next;
        }

        newfile->next = cur_node;
        prev_node->next = newfile;
    }    
}

void filelist_delete_node(fileList *head, char *hash)
{
	fileList *deleted;

	if (!strcmp(head->next->hash, hash)){
		deleted = head->next;
		head->next = head->next->next;
	}
	else {
		fileList *filelist_cur = head->next;

		while (filelist_cur->next != NULL){
			if (!strcmp(filelist_cur->next->hash, hash)){
				deleted = filelist_cur->next;
				
				filelist_cur->next = filelist_cur->next->next;

				break;
			}

			filelist_cur = filelist_cur->next;
		}
	}

	free(deleted);
}

int filelist_size(fileList *head)
{
	fileList *cur = head->next;
	int size = 0;
	
	while (cur != NULL){
		size++;
		cur = cur->next;
	}
	
	return size;
}

int filelist_search(fileList *head, char *hash)
{
	fileList *cur = head;
	int idx = 0;

	while (cur != NULL){
		if (!strcmp(cur->hash, hash)){

			return idx;
		}
		cur = cur->next;
		idx++;
	}

	return 0;
}

void dirlist_append(dirList *head, char *path)
{
	dirList *newFile = (dirList *)malloc(sizeof(dirList));


	strcpy(newFile->dirpath, path);
	newFile->next = NULL;


	if (head->next == NULL){
		head->next = newFile;
	}
	else{
		dirList *cur = head->next;

		while(cur->next != NULL)
			cur = cur->next;
		
		cur->next = newFile;
	}

	
	

}

void dirlist_print(dirList *head, int index)
{
	dirList *cur = head->next;
	int i = 1;

	while (cur != NULL){
		if (index) 
			printf("[%d] ", i++);
		printf("%s\n", cur->dirpath);
		cur = cur->next;
	}
}

void dirlist_delete_all(dirList *head)
{
	dirList *dirlist_cur = head->next;
	dirList *tmp;

	while (dirlist_cur != NULL){
		tmp = dirlist_cur->next;
		free(dirlist_cur);
		dirlist_cur = tmp;
	}

	head->next = NULL;
}

int tokenize(char *input, char *argv[])
{
	char *ptr = NULL;
	int argc = 0;
	ptr = strtok(input, " ");

	while (ptr != NULL){
		argv[argc++] = ptr;
		ptr = strtok(NULL, " ");
	}

	argv[argc-1][strlen(argv[argc-1])-1] = '\0';

	return argc;
}

void get_path_from_home(char *path, char *path_from_home)
{
	char path_without_home[PATHMAX] = {0,};
	char *home_path;

    home_path = getenv("HOME");

	memset(USER,0,NAMEMAX);

	struct passwd *pwd;
	pwd=getpwuid(getuid());
	strcpy(USER, pwd->pw_name);

	if (strlen(path) == 1){
		strncpy(path_from_home, home_path, strlen(home_path));
	}
    else {
        strncpy(path_without_home, path + 1, strlen(path)-1);
        sprintf(path_from_home, "%s%s", home_path, path_without_home);
    }
}

int is_dir(char *target_dir)
{
    struct stat statbuf;

    if (lstat(target_dir, &statbuf) < 0){
        printf("ERROR: lstat error for %s\n", target_dir);
        return 1;
    }
    return S_ISDIR(statbuf.st_mode) ? DIRECTORY : 0;

}

long long get_size(char *filesize)
{
	double size_bytes = 0;
	char size[STRMAX] = {0, };
	char size_unit[4] = {0, };
	int size_idx = 0;

	if (!strcmp(filesize, "~"))
		size_bytes = -1;
	else {
		for (int i = 0; i < strlen(filesize); i++){
			if (isdigit(filesize[i]) || filesize[i] == '.'){
				size[size_idx++] = filesize[i];
				if (filesize[i] == '.' && !isdigit(filesize[i + 1]))
					return SIZE_ERROR;
			}
			else {
				strcpy(size_unit, filesize + i);
				break;
			}
		}

		size_bytes = atof(size);

		if (strlen(size_unit) != 0){
			if (!strcmp(size_unit, "kb") || !strcmp(size_unit, "KB"))
				size_bytes *= KB;
			else if (!strcmp(size_unit, "mb") || !strcmp(size_unit, "MB"))
				size_bytes *= MB;
			else if (!strcmp(size_unit, "gb") || !strcmp(size_unit, "GB"))
				size_bytes *= GB;
			else
				return SIZE_ERROR;
		}
	}

	return (long long)size_bytes;
}

int get_file_mode(char *target_file, struct stat *statbuf)
{
	if (lstat(target_file, statbuf) < 0){
        printf("ERROR: lstat error for %s\n", target_file);
        return 0;
    }

    if (S_ISREG(statbuf->st_mode))
    	return REGFILE;
    else if(S_ISDIR(statbuf->st_mode))
    	return DIRECTORY;
    else
    	return 0;
}

void get_fullpath(char *target_dir, char *target_file, char *fullpath)
{
	strcat(fullpath, target_dir);

	if(fullpath[strlen(target_dir) - 1] != '/')
		strcat(fullpath, "/");

	strcat(fullpath, target_file);
	fullpath[strlen(fullpath)] = '\0';
}

int get_dirlist(char *target_dir, struct dirent ***namelist)
{
	int cnt = 0;

	if ((cnt = scandir(target_dir, namelist, NULL, alphasort)) == -1){
		printf("ERROR: scandir error for %s\n", target_dir);
		return -1;
	}

	return cnt;
}

char *get_extension(char *filename)
{
	char *tmp_ext;

	if ((tmp_ext = strstr(filename, ".tar")) != NULL || (tmp_ext = strrchr(filename, '.')) != NULL)
		return tmp_ext + 1;
	else
		return NULL;
}

void get_filename(char *path, char *filename)
{
	char tmp_name[NAMEMAX];
	char *pt = NULL;

	memset(tmp_name, 0, sizeof(tmp_name));
	
	if (strrchr(path, '/') != NULL)
		strcpy(tmp_name, strrchr(path, '/') + 1);
	else
		strcpy(tmp_name, path);
	
	if ((pt = get_extension(tmp_name)) != NULL)
		pt[-1] = '\0';

	if (strchr(path, '/') == NULL && (pt = strrchr(tmp_name, '.')) != NULL)
		pt[0] = '\0';
	
	strcpy(filename, tmp_name);
}

void get_new_file_name(char *org_filename, char *new_filename)
{
	char new_trash_path[PATHMAX];
	char tmp[NAMEMAX];
	struct dirent **namelist;
	int trashlist_cnt;
	int same_name_cnt = 1;

	get_filename(org_filename, new_filename);
	trashlist_cnt = get_dirlist(trash_path, &namelist);

	for (int i = 0; i < trashlist_cnt; i++){
		if (!strcmp(namelist[i]->d_name, ".") || !strcmp(namelist[i]->d_name, ".."))
			continue;

		memset(tmp, 0, sizeof(tmp));
		get_filename(namelist[i]->d_name, tmp);

		if (!strcmp(new_filename, tmp))
			same_name_cnt++;
	}
	
	sprintf(new_filename + strlen(new_filename), ".%d", same_name_cnt);

	if (get_extension(org_filename) != NULL)
		sprintf(new_filename + strlen(new_filename), ".%s", get_extension(org_filename));
}

void remove_files(char *dir)
{
	struct dirent **namelist;
	int listcnt = get_dirlist(dir, &namelist);

	for (int i = 0; i < listcnt; i++){
		char fullpath[PATHMAX] = {0, };

		if (!strcmp(namelist[i]->d_name, ".") || !strcmp(namelist[i]->d_name, ".."))
			continue;

		get_fullpath(dir, namelist[i]->d_name, fullpath);

		remove(fullpath);
	}
}

void get_same_size_files_dir(void)
{
	get_path_from_home("~/.20202925", same_size_files_dir);//중복들 관리하는 디렉토리 생성

	if (access(same_size_files_dir, F_OK) == 0)
		remove_files(same_size_files_dir);
	else
		mkdir(same_size_files_dir, 0755);
}

void get_logfile(void)
{
	get_path_from_home("~/.duplicate_20202925.log", logfile);//로그 기록하는 파일생성
	if(access(logfile,F_OK)==0)
	{
		return;
	}
	else
		open(logfile,O_RDWR|O_CREAT|O_TRUNC);
}



void get_trash_path(void)
{

	get_path_from_home("~/.Trash/", trash_d);
	get_path_from_home("~/.Trash/files/", trash_path);
	get_path_from_home("~/.Trash/info/", trash_info);

	if (access(trash_d, F_OK) != 0)
		mkdir(trash_d, 0755);

	if (access(trash_path, F_OK) != 0)
		mkdir(trash_path, 0755);
	
	if (access(trash_info, F_OK) != 0)
		mkdir(trash_info, 0755);
}

int check_args(int argc, char *argv[])
{
	char target_dir[PATHMAX];

	if(!strcmp(argv[0],"fsha1")){

		if (argc != 9 && argc!=11){
			printf("Usage: fsha1 -e [FILE_EXTENSION] -l [MINSIZE] -h [MAXSIZE] -d [DIRECTORY] -t [THREAD_NUM]\n");
			return 1;
		}

		int flag_e=0, flag_l=0, flag_h=0, flag_d=0, flag_t=0;
		int opt;
		threadnum=1;
		while((opt=getopt(argc, argv, "e:l:h:d:t:"))!=-1){
			switch(opt){
				case 'e':
					flag_e=1;
					if (strchr(optarg, '*') == NULL){
						printf("ERROR: [FILE_EXTENSION] should be '*' or '*.extension'\n");
						return 1;
					}

					if (strchr(optarg, '.') != NULL){
						strcpy(extension, get_extension(optarg));

						if (strlen(extension) == 0){
							printf("ERROR: There should be extension\n");
							return 1;
						}
					}
					break;
				case 'l':
					flag_l=1;
					minbsize = get_size(optarg);
	
					if (minbsize == -1)
						minbsize = 0;
					break;
				case 'h':
					flag_h=1;
					maxbsize = get_size(optarg);
					
					if (minbsize == SIZE_ERROR || maxbsize == SIZE_ERROR){
						printf("ERROR: Size wrong -min size : %s -max size : %s\n", argv[4], argv[6]);
						return 1;
					}

					if (maxbsize != -1 && minbsize > maxbsize){
						printf("ERROR: [MAXSIZE] should be bigger than [MINSIZE]\n");
						return 1;
					}
					break;
				case 'd':
					flag_d=1;
					if (strchr(optarg, '~') != NULL)
						get_path_from_home(optarg, target_dir);
					else{
						if (realpath(optarg, target_dir) == NULL){
							printf("ERROR: [TARGET_DIRECTORY] should exist\n");
							return 1;
						}
					}
	
					if (access(target_dir, F_OK) == -1){
						printf("ERROR: %s directory doesn't exist\n", target_dir);
						return 1;
					}

					if (!is_dir(target_dir)){
						printf("ERROR: [TARGET_DIRECTORY] should be a directory\n");
						return 1;
					}
					break;

				case 't':
					threadnum=atoi(optarg);
					if(0>threadnum||5<threadnum){
						printf("ERROR: 1~5 threads allowed\n");
						return 1;
					}
					break;
	
				case '?':
					break;
				
			}
		}
		if(flag_e!=1||flag_l!=1||flag_h!=1||flag_d!=1)
			return 1;
		
	}


	return 0;
}

void filesize_with_comma(long long filesize, char *filesize_w_comma)
{
	char filesize_wo_comma[STRMAX] = {0, };
	int comma;
	int idx = 0;

	sprintf(filesize_wo_comma, "%lld", filesize);
	comma = strlen(filesize_wo_comma)%3;

	for (int i = 0 ; i < strlen(filesize_wo_comma); i++){
		if (i > 0 && (i%3) == comma)
			filesize_w_comma[idx++] = ',';

		filesize_w_comma[idx++] = filesize_wo_comma[i];
	}

	filesize_w_comma[idx] = '\0';
}

void sec_to_ymdt(struct tm *time, char *ymdt)
{
	sprintf(ymdt, "%04d-%02d-%02d %02d:%02d:%02d", time->tm_year + 1900, time->tm_mon + 1, time->tm_mday, time->tm_hour, time->tm_min, time->tm_sec);
}


void trash_time(struct tm *time, char *ymdt)
{
	sprintf(ymdt, "%04d-%02d-%02d\t%02d:%02d:%02d", time->tm_year + 1900, time->tm_mon + 1, time->tm_mday, time->tm_hour, time->tm_min, time->tm_sec);
}

void filelist_print_format(fileList *head)
{
	fileList *filelist_cur = head->next;
	int set_idx = 1;	

	while (filelist_cur != NULL){
		fileInfo *fileinfolist_cur = filelist_cur->fileInfoList->next;
		char mtime[STRMAX];
		char atime[STRMAX];
		char filesize_w_comma[STRMAX] = {0, };
		int i = 1;

		filesize_with_comma(filelist_cur->filesize, filesize_w_comma);

		printf("---- Identical files #%d (%s bytes - %s) ----\n", set_idx++, filesize_w_comma, filelist_cur->hash);

		while (fileinfolist_cur != NULL){
			sec_to_ymdt(localtime(&fileinfolist_cur->statbuf.st_mtime), mtime);
			sec_to_ymdt(localtime(&fileinfolist_cur->statbuf.st_atime), atime);
			printf("[%d] %s (mtime : %s) (atime : %s) (uid : %d) (gid : %d) (mode : %d)\n", i++, fileinfolist_cur->path, mtime, atime, fileinfolist_cur->statbuf.st_uid,fileinfolist_cur->statbuf.st_gid, fileinfolist_cur->statbuf.st_mode);

			fileinfolist_cur = fileinfolist_cur->next;
		}

		printf("\n");

		filelist_cur = filelist_cur->next;
	}	
}


int sha1(char *target_path, char *hash_result)
{
	FILE *fp;
	unsigned char hash[SHA_DIGEST_LENGTH];
	unsigned char buffer[SHRT_MAX];
	int bytes = 0;
	SHA_CTX sha1;

	if ((fp = fopen(target_path, "rb")) == NULL){
		printf("ERROR: fopen error for %s\n", target_path);
		return 1;
	}

	SHA1_Init(&sha1);

	while ((bytes = fread(buffer, 1, SHRT_MAX, fp)) != 0)
		SHA1_Update(&sha1, buffer, bytes);
	
	SHA1_Final(hash, &sha1);

	for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
		sprintf(hash_result + (i * 2), "%02x", hash[i]);
	hash_result[HASHMAX-1] = 0;

	fclose(fp);

	return 0;
}

void hash_func(char *path, char *hash)
{
#ifdef HASH_SHA1
	sha1(path, hash);

#endif
}

void dir_traverse(dirList *dirlist)
{


	dirList *cur = dirlist->next;
	dirList *subdirs = (dirList *)malloc(sizeof(dirList));

	memset(subdirs, 0 , sizeof(dirList));

	while (cur != NULL){
		struct dirent **namelist;
		int listcnt;

		listcnt = get_dirlist(cur->dirpath, &namelist);

		for (int i = 0; i < listcnt; i++){
			char fullpath[PATHMAX] = {0, };
			struct stat statbuf;
			int file_mode;
			long long filesize;

			if (!strcmp(namelist[i]->d_name, ".") || !strcmp(namelist[i]->d_name, ".."))
				continue;

			get_fullpath(cur->dirpath, namelist[i]->d_name, fullpath);
		

			char t[PATHMAX];
			get_path_from_home("~/.Trash", t);

			if (!strcmp(fullpath,"/proc") || !strcmp(fullpath, "/run") || !strcmp(fullpath, "/sys") || !strcmp(fullpath, t)|| !strcmp(fullpath, trash_path+1) || !strcmp(fullpath, trash_info+1))
				continue;

			file_mode = get_file_mode(fullpath, &statbuf);

			if (file_mode == DIRECTORY)
				dirlist_append(subdirs, fullpath);       
			else if (file_mode == REGFILE){
				if ((filesize = (long long)statbuf.st_size) == 0)
					continue;

				if (filesize < minbsize)
					continue;

				if (maxbsize != -1 && filesize > maxbsize)
					continue;

				FILE *fp;//파일 관리 디렉토리
				char filename[PATHMAX*2];
				char *path_extension;
				char hash[HASHMAX];

				sprintf(filename, "%s/%lld", same_size_files_dir, filesize);

				memset(hash, 0, HASHMAX);
				hash_func(fullpath, hash);

				path_extension = get_extension(fullpath);

				if (strlen(extension) != 0){
					if (path_extension == NULL || strcmp(extension, path_extension))
						continue;
				}

				if ((fp = fopen(filename, "a")) == NULL){
					printf("ERROR: fopen error for %s\n", filename);
					return;
				}

				fprintf(fp, "%s %s\n", hash, fullpath);

				fclose(fp);
			}
		}

		cur = cur->next;
	}

	dirlist_delete_all(dirlist);

	if (subdirs->next != NULL)
		dir_traverse(subdirs);
}







typedef struct thread_data{
	int start;
	int num;
}thread_data;

struct thread_data thread_data_array[5];

int k=0;
int i=0;
int j=0;
void multi_thread_find(int listcnt){//thread num 만큼 씩 반복해서 실행 예를 들어 3이면 1->1파일 2->2파일 3->3파일 1->4파일 .. .
	int status;

	for(j=0; j< listcnt;j++){
		pthread_t threads[threadnum];
		memset(threads,0,sizeof(threads));
		memset(thread_data_array,0,sizeof(thread_data_array));
		for(i=0; i<threadnum;i++){
			k=k+1;
			if(k<=listcnt){


				thread_data_array[i].start=i+j;
				thread_data_array[i].num=i+1;
				pthread_create(&threads[i],NULL, multi_find_duplicates, (void *)&thread_data_array[i]);
				


			}
			else
				break;
		}
		j=k-1;
		for(i=0;i<threadnum;i++){
			pthread_join(threads[i], (void *)&status);

		}
		status=pthread_mutex_destroy(&mutex);

				
	}


	return ;


}


void *multi_find_duplicates(void *arg)
{
	thread_data *data;
	struct dirent **namelist;
	int start;
	char hash[HASHMAX];
	FILE *fp;
	int th;//몇번째 쓰레드인지

	data=(thread_data *)arg;
	start=data->start;
	th=data->num;
	struct timeval begin_t, end_t;

	gettimeofday(&begin_t, NULL);


//파일 관리 디렉토리에서 가져와서 찾음
	get_dirlist(same_size_files_dir, &namelist);
//	for (int i = start; i < end; i++){
	char filename[PATHMAX*2];
	long long filesize;
	char filepath[PATHMAX];

	char line[STRMAX];
	
	if (!strcmp(namelist[start]->d_name, ".") || !strcmp(namelist[start]->d_name, ".."))
		return NULL;

	filesize = atoll(namelist[start]->d_name);
	sprintf(filename, "%s/%s", same_size_files_dir, namelist[start]->d_name);

	if ((fp = fopen(filename, "r")) == NULL){
		printf("ERROR: fopen error for %s\n", filename);
		return NULL;
	}

	while (fgets(line, sizeof(line), fp) != NULL){
		pthread_mutex_lock(&mutex);


		int idx;

		strncpy(hash, line, HASHMAX);
		hash[HASHMAX-1] = '\0';

		strcpy(filepath, line+HASHMAX);
		filepath[strlen(filepath)-1] = '\0';
			
		if ((idx = filelist_search(dups_list_h, hash)) == 0){

			filelist_append(dups_list_h, filesize, filepath, hash);
		}
		else{
			fileList *filelist_cur = dups_list_h;
			while (idx--){
				filelist_cur = filelist_cur->next;
			}
			fileinfo_append(filelist_cur->fileInfoList, filepath);
		}
		
		pthread_mutex_unlock(&mutex);

	}

	fclose(fp);
		
	
	

	gettimeofday(&end_t, NULL);

	end_t.tv_sec -= begin_t.tv_sec;

	if (end_t.tv_usec < begin_t.tv_usec){
		end_t.tv_sec--;
		end_t.tv_usec += 1000000;
	}

	end_t.tv_usec -= begin_t.tv_usec;
	printf("Thread %d working time: %ld:%06ld(sec:usec)\n\n",th, end_t.tv_sec, end_t.tv_usec);

	return NULL;
}

void find_duplicates(void)
{
	struct dirent **namelist;
	int listcnt;
	char hash[HASHMAX];
	FILE *fp;
//파일 관리 디렉토리에서 가져와서 찾음
	listcnt = get_dirlist(same_size_files_dir, &namelist);
	
	if(threadnum>1){//thread 개수가 2이상일때 멀티쓰레드로 작업
		multi_thread_find(listcnt);
		threadnum=1;
		k=0;
		i=0;
		j=0;

		return ;
	}
	threadnum=1;

	for (int i = 0; i < listcnt; i++){
		char filename[PATHMAX*2];
		long long filesize;
		char filepath[PATHMAX];
		char hash[HASHMAX];
		char line[STRMAX];

		if (!strcmp(namelist[i]->d_name, ".") || !strcmp(namelist[i]->d_name, ".."))
			continue;

		filesize = atoll(namelist[i]->d_name);
		sprintf(filename, "%s/%s", same_size_files_dir, namelist[i]->d_name);

		if ((fp = fopen(filename, "r")) == NULL){
			printf("ERROR: fopen error for %s\n", filename);
			continue;
		}
		while (fgets(line, sizeof(line), fp) != NULL){
			int idx;

			strncpy(hash, line, HASHMAX);
			hash[HASHMAX-1] = '\0';

			strcpy(filepath, line+HASHMAX);
			filepath[strlen(filepath)-1] = '\0';
			
			if ((idx = filelist_search(dups_list_h, hash)) == 0){

				filelist_append(dups_list_h, filesize, filepath, hash);
			}
			else{
				fileList *filelist_cur = dups_list_h;
				while (idx--){
					filelist_cur = filelist_cur->next;
				}
				fileinfo_append(filelist_cur->fileInfoList, filepath);
			}
		}

		fclose(fp);
	}
}

void remove_no_duplicates(void)
{
	fileList *filelist_cur = dups_list_h->next;

	while (filelist_cur != NULL){
		fileInfo *fileinfo_cur = filelist_cur->fileInfoList;

		if (fileinfolist_size(fileinfo_cur) < 2)
			filelist_delete_node(dups_list_h, filelist_cur->hash);
		
		filelist_cur = filelist_cur->next;
	}
}

time_t get_recent_mtime(fileInfo *head, char *last_filepath)
{
	fileInfo *fileinfo_cur = head->next;
	time_t mtime = 0;

	while (fileinfo_cur != NULL){
		if (fileinfo_cur->statbuf.st_mtime > mtime){
			mtime = fileinfo_cur->statbuf.st_mtime;
			strcpy(last_filepath, fileinfo_cur->path);
		}
		fileinfo_cur = fileinfo_cur->next;
	}
	return mtime;
}

void delete_prompt(void)
{
	while (filelist_size(dups_list_h) > 0){

		optind=1;
		char input[STRMAX];
		char last_filepath[PATHMAX];
		char modifiedtime[STRMAX];
		char *argv[6];
		int argc;
		int set_idx;
		time_t mtime = 0;
		fileList *target_filelist_p;
		fileInfo *target_infolist_p;
				
		printf(">> ");
		
		fgets(input, sizeof(input), stdin);

		if (!strcmp(input, "exit\n")){
			printf(">> Back to Prompt\n");
			break;
		}
		else if(!strcmp(input, "\n"))
			continue;

		argc = tokenize(input, argv);
		if (argc != 5 && argc!= 4){
			printf("ERROR: >> delete -l [SET_IDX] -d [LIST_IDX] -i -f -t\n");
			continue;
		}

		if(argc==5){
			if(strcmp(argv[3],"-d"))
				continue;
		}

		if(strcmp(argv[1],"-l"))
			continue;

		if (!atoi(argv[2])){
			printf("ERROR: [SET_IDX] should be a number\n");
			continue;
		}

		if (atoi(argv[2]) < 0 || atoi(argv[2]) > filelist_size(dups_list_h)){
			printf("ERROR: [SET_IDX] out of range\n");
			continue;
		}
		
		target_filelist_p = dups_list_h->next;

		set_idx = atoi(argv[2]);

		while (--set_idx)
			target_filelist_p = target_filelist_p->next;

		target_infolist_p = target_filelist_p->fileInfoList;

		mtime = get_recent_mtime(target_infolist_p, last_filepath);
		sec_to_ymdt(localtime(&mtime), modifiedtime);

		set_idx = atoi(argv[2]);

		int check=0;
		if(!strcmp(argv[0],"delete")){
			
			int flag_l=0, flag_d=0, flag_i=0, flag_f=0, flag_t=0;
			int opt;
			fileInfo *deleted;
			int list_idx;
		
			char ans[STRMAX];
			fileInfo *fileinfo_cur;
			fileInfo *deleted_list;
			fileInfo *tmp;
			int listcnt;

			char move_to_trash[PATHMAX];
			char filename[PATHMAX];

			char trashTime[STRMAX];
			time_t timer;

			
			FILE *fp;//Trash info 기록용
			char filename_t[PATHMAX*2];
			char *path_extension;

			FILE *fp_l;//log 기록용
			char filename_l[PATHMAX*2];
			char *path_extension_l;
			
			while((opt=getopt(argc, argv, "l:d:ift"))!=-1){
				switch(opt){
					case 'l':
						if (!atoi(optarg)){
							printf("ERROR: [SET_IDX] should be a number\n");
							break;
						}

						if (atoi(optarg) < 0 || atoi(optarg) > filelist_size(dups_list_h)){
							printf("ERROR: [SET_IDX] out of range\n");
							break;
						}
		
						target_filelist_p = dups_list_h->next;

						set_idx = atoi(optarg);
		
						while (--set_idx)
							target_filelist_p = target_filelist_p->next;
	
						target_infolist_p = target_filelist_p->fileInfoList;

						mtime = get_recent_mtime(target_infolist_p, last_filepath);
						sec_to_ymdt(localtime(&mtime), modifiedtime);
			
						set_idx = atoi(optarg);
						check++;
						break;

					case 'd':

						if (optarg == NULL || (list_idx = atoi(optarg)) == 0){
							printf("ERROR: There should be an index\n");
							break;
						}

						if (list_idx < 0 || list_idx > fileinfolist_size(target_infolist_p)){
							printf("ERROR: [LIST_IDX] out of range\n");
							break;
						}

						deleted = target_infolist_p;

						while (list_idx--)
							deleted = deleted->next;

						//log 기록
							
						if ((fp = fopen(logfile, "a")) == NULL){
							printf("ERROR: fopen error for %s\n", logfile);
							continue;
						}
		
						timer=time(NULL);
						trash_time(localtime(&timer), trashTime);

						fprintf(fp, "%s\t%s\t%s\t%s\n","[DELETE]", deleted->path, trashTime, USER);
	
						fclose(fp);
						
						//삭제
						printf("\"%s\" has been deleted in #%d\n\n", deleted->path, atoi(argv[2]));
						remove(deleted->path);
						fileinfo_delete_node(target_infolist_p, deleted->path);

						if (fileinfolist_size(target_infolist_p) < 2)
							filelist_delete_node(dups_list_h, target_filelist_p->hash);
						check++;
						break;


					case 'i':
						fileinfo_cur = target_infolist_p->next;
						deleted_list = (fileInfo *)malloc(sizeof(fileInfo));
						tmp;
						listcnt = fileinfolist_size(target_infolist_p);


						while (fileinfo_cur != NULL && listcnt--){
							printf("Delete \"%s\"? [y/n] ", fileinfo_cur->path);
							memset(ans, 0, sizeof(ans));
							fgets(ans, sizeof(ans), stdin);

							if (!strcmp(ans, "y\n") || !strcmp(ans, "Y\n")){
										
								//log 기록

								
								if ((fp = fopen(logfile, "a")) == NULL){
									printf("ERROR: fopen error for %s\n", logfile);
									continue;
								}
			
								timer=time(NULL);
								trash_time(localtime(&timer), trashTime);

	
								fprintf(fp, "%s\t%s\t%s\t%s\n","[DELETE]", fileinfo_cur->path, trashTime, USER);
		
								fclose(fp);

								//삭제
								remove(fileinfo_cur->path);
								fileinfo_cur = fileinfo_delete_node(target_infolist_p, fileinfo_cur->path);				
							}
							else if (!strcmp(ans, "n\n") || !strcmp(ans, "N\n"))
								fileinfo_cur = fileinfo_cur->next;										
							else {
								printf("ERROR: Answer should be 'y/Y' or 'n/N'\n");
								break;
							}
						}
						printf("\n");

						if (fileinfolist_size(target_infolist_p) < 2)
							filelist_delete_node(dups_list_h, target_filelist_p->hash);
						check++;
						break;


					case 'f':
		
						deleted = target_infolist_p->next;

						while (deleted != NULL){
							tmp = deleted->next;
				
							if (!strcmp(deleted->path, last_filepath)){
								deleted = tmp;
								continue;
							}
						
							//log 기록

							if ((fp = fopen(logfile, "a")) == NULL){
								printf("ERROR: fopen error for %s\n", filename_l);
								continue;
							}
		
							timer=time(NULL);
							trash_time(localtime(&timer), trashTime);

							fprintf(fp, "%s\t%s\t%s\t%s\n","[DELETE]", deleted->path, trashTime, USER);
	
							fclose(fp);

							//삭제
							remove(deleted->path);
							free(deleted);
							deleted = tmp;
						}

						filelist_delete_node(dups_list_h, target_filelist_p->hash);
						printf("Left file in #%d : %s (%s)\n\n", atoi(argv[2]), last_filepath, modifiedtime);
						check++;
						break;


					case 't':
						tmp;
						deleted = target_infolist_p->next;
						move_to_trash[PATHMAX];
						filename[PATHMAX];

						while (deleted != NULL){
						
							tmp = deleted->next;
				
							if (!strcmp(deleted->path, last_filepath)){
								deleted = tmp;
								continue;
							}

							memset(move_to_trash, 0, sizeof(move_to_trash));
							memset(filename, 0, sizeof(filename));
						
							memset(trashTime,0,STRMAX);

							sprintf(move_to_trash, "%s%s", trash_path, strrchr(deleted->path, '/') + 1);


							if (access(move_to_trash, F_OK) == 0){
								get_new_file_name(deleted->path, filename);
			
								strncpy(strrchr(move_to_trash, '/') + 1, filename, strlen(filename));
							}
							else
								strcpy(filename, strrchr(deleted->path, '/') + 1);
			

										
							//log 기록
						

							if ((fp = fopen(logfile, "a")) == NULL){
								printf("ERROR: fopen error for %s\n", logfile);
								continue;
							}
		
							timer=time(NULL);
							trash_time(localtime(&timer), trashTime);

							fprintf(fp, "%s\t%s\t%s\t%s\n","[REMOVE]", deleted->path, trashTime, USER);
	
							fclose(fp);


							strcpy(trashedTime, trashTime);

							//Trash/info에 기록
							sprintf(filename_t, "%s/%lld", trash_info, (long long)deleted->statbuf.st_size);

							path_extension = get_extension(deleted->path);

							if (strlen(extension) != 0){
								if (path_extension == NULL || strcmp(extension, path_extension))
									continue;
							}
							

							if ((fp = fopen(filename_t, "a")) == NULL){
								printf("ERROR: fopen error for %s\n", filename_t);
								continue;
							}
		


							fprintf(fp, "%s\t%s\t%s\n", deleted->path, trashTime,move_to_trash);
	
							fclose(fp);

							//Trash/files 로 이동
							if (rename(deleted->path, move_to_trash) == -1){
								printf("ERROR: Fail to move duplicates to Trash\n");
								continue;
							}
				
							free(deleted);
							deleted = tmp;
						}
						
						filelist_delete_node(dups_list_h, target_filelist_p->hash);
						printf("All files in #%d have moved to Trash except \"%s\" (%s)\n\n", atoi(argv[2]), last_filepath, modifiedtime);
						check++;
						break;

					case '?':
						printf("ERROR: Only f, t, i, d options are available\n");
						break;

				}


			}
		}
		
		if(check!=2)
			continue;
		filelist_print_format(dups_list_h);
	}
}	
char targetDir[PATHMAX];//refind에서 사용할 것들
char e[PATHMAX];
char l[PATHMAX];
char h[PATHMAX];
int find_sha1(int argc, char *argv[])
{
	get_trash_path();

	char target_dir[PATHMAX];
	dirList *dirlist = (dirList *)malloc(sizeof(dirList));
	
	dups_list_h = (fileList *)malloc(sizeof(fileList));
	memset(dups_list_h, 0, sizeof(fileList));
	if (check_args(argc, argv))
		return 1;

	if (strchr(argv[8], '~') != NULL)
		get_path_from_home(argv[8], target_dir);
	else
		realpath(argv[8], target_dir);
	
	strcpy(targetDir, target_dir);
	strcpy(e, argv[2]);
	strcpy(l, argv[4]);
	strcpy(h, argv[6]);
	


	get_same_size_files_dir();
	get_logfile();

	struct timeval begin_t, end_t;

	gettimeofday(&begin_t, NULL);


	dirlist->next=NULL;
	dirlist_append(dirlist, target_dir);
	dir_traverse(dirlist);
	find_duplicates();

	remove_no_duplicates();
	gettimeofday(&end_t, NULL);

	end_t.tv_sec -= begin_t.tv_sec;

	if (end_t.tv_usec < begin_t.tv_usec){
		end_t.tv_sec--;
		end_t.tv_usec += 1000000;
	}

	end_t.tv_usec -= begin_t.tv_usec;

	if (dups_list_h->next == NULL)
		printf("No duplicates in %s\n", target_dir);
	else 
		filelist_print_format(dups_list_h);

	printf("Searching time: %ld:%06ld(sec:usec)\n\n", end_t.tv_sec, end_t.tv_usec);


	delete_prompt();
	
	return 0;
}


void list_listInfo_append(fileInfo *head, char *path,int flag_fileset, int flag_filename, int flag_size, int flag_uid,int flag_gid, int flag_mode ,int flag_asc )
{
	fileInfo *newfile = (fileInfo *)malloc(sizeof(fileInfo));
	memset(newfile, 0, sizeof(fileInfo));
	strcpy(newfile->path, path);
	lstat(newfile->path, &newfile->statbuf);
	newfile->next = NULL;

	if (head->next == NULL)
		head->next = newfile;
	
	else if(flag_fileset==1){
		fileInfo *fileinfo_cur = head->next;
		while (fileinfo_cur->next != NULL)
			fileinfo_cur = fileinfo_cur->next;

		fileinfo_cur->next = newfile;
	}

	else {
		fileInfo *cur_node = head->next, *prev_node = head, *next_node;
		if(flag_filename==1){
			if(flag_asc==1){
				while (cur_node != NULL && strcmp(cur_node->path, newfile->path)<0) {
					prev_node = cur_node;
					cur_node = cur_node->next;
				}

				newfile->next = cur_node;
				prev_node->next = newfile;
			}
			else{
				while (cur_node != NULL && strcmp(cur_node->path, newfile->path)>0) {
					prev_node = cur_node;
					cur_node = cur_node->next;
				}

				newfile->next = cur_node;
				prev_node->next = newfile;
			}

		}
		else if(flag_uid==1){
			if(flag_asc==1){
				while (cur_node != NULL && (cur_node->statbuf.st_uid < newfile->statbuf.st_uid )) {
					prev_node = cur_node;
					cur_node = cur_node->next;
				}

				newfile->next = cur_node;
				prev_node->next = newfile;
			}
			else{
				while (cur_node != NULL && (cur_node->statbuf.st_uid > newfile->statbuf.st_uid )){
					prev_node = cur_node;
					cur_node = cur_node->next;
				}

				newfile->next = cur_node;
				prev_node->next = newfile;
			}

		}
		else if(flag_gid==1){
			if(flag_asc==1){
				while (cur_node != NULL && (cur_node->statbuf.st_gid < newfile->statbuf.st_gid )) {
					prev_node = cur_node;
					cur_node = cur_node->next;
				}

				newfile->next = cur_node;
				prev_node->next = newfile;
			}
			else{
				while (cur_node != NULL && (cur_node->statbuf.st_gid > newfile->statbuf.st_gid )){
					prev_node = cur_node;
					cur_node = cur_node->next;
				}

				newfile->next = cur_node;
				prev_node->next = newfile;
			}

		}
		else if(flag_mode==1){
			if(flag_asc==1){
				while (cur_node != NULL && (cur_node->statbuf.st_mode < newfile->statbuf.st_mode )){
					prev_node = cur_node;
					cur_node = cur_node->next;
				}

				newfile->next = cur_node;
				prev_node->next = newfile;
			}
			else{
				while (cur_node != NULL && (cur_node->statbuf.st_mode > newfile->statbuf.st_mode )){
					prev_node = cur_node;
					cur_node = cur_node->next;
				}

				newfile->next = cur_node;
				prev_node->next = newfile;
			}

		}
		
	}
	
}


void list_list_append(fileList *head, fileList *dupslist, long long filesize, char *path, char *hash,int flag_fileset, int flag_filename, int flag_size, int flag_uid,int flag_gid, int flag_mode ,int flag_asc )
{

    fileList *newfile = (fileList *)malloc(sizeof(fileList));
    memset(newfile, 0, sizeof(fileList));

    newfile->filesize = filesize;
    strcpy(newfile->hash, hash);

    newfile->fileInfoList = (fileInfo *)malloc(sizeof(fileInfo));
    memset(newfile->fileInfoList, 0, sizeof(fileInfo));

	fileList *dups_list=dupslist;
	while(dups_list->fileInfoList->next != NULL){
		list_listInfo_append(newfile->fileInfoList, dups_list->fileInfoList->next->path, flag_fileset,flag_filename,flag_size, flag_uid, flag_gid, flag_mode , flag_asc);
		dups_list->fileInfoList->next=dups_list->fileInfoList->next->next;
	}

	newfile->next = NULL;

    if (head->next == NULL) {
        head->next = newfile;
    }    
    else {
        fileList *cur_node = head->next, *prev_node = head, *next_node;
		
		if(flag_fileset==0){
		    while (cur_node != NULL && cur_node->filesize < newfile->filesize) {
				prev_node = cur_node;
				cur_node = cur_node->next;
			}
			newfile->next = cur_node;
			prev_node->next = newfile;
		}
		
		else if(flag_size==1){
			if(flag_asc==1){
			    while (cur_node != NULL && cur_node->filesize < newfile->filesize) {
					prev_node = cur_node;
					cur_node = cur_node->next;
				}

				newfile->next = cur_node;
				prev_node->next = newfile;
			}
			else{
				while (cur_node != NULL && cur_node->filesize > newfile->filesize) {
					prev_node = cur_node;
					cur_node = cur_node->next;
				}

				newfile->next = cur_node;
				prev_node->next = newfile;
			
			}
		}

		else if(flag_uid==1){
			if(flag_asc==1){
				while (cur_node != NULL && (cur_node->fileInfoList->next->statbuf.st_uid < newfile->fileInfoList->next->statbuf.st_uid )) {
					prev_node = cur_node;
					cur_node = cur_node->next;
				}

				newfile->next = cur_node;
				prev_node->next = newfile;
			}
			else{
				while (cur_node != NULL && (cur_node->fileInfoList->next->statbuf.st_uid > newfile->fileInfoList->next->statbuf.st_uid )){
					prev_node = cur_node;
					cur_node = cur_node->next;
				}

				newfile->next = cur_node;
				prev_node->next = newfile;
			}

		}
		else if(flag_gid==1){
			if(flag_asc==1){
				while (cur_node != NULL && (cur_node->fileInfoList->next->statbuf.st_gid < newfile->fileInfoList->next->statbuf.st_gid )) {
					prev_node = cur_node;
					cur_node = cur_node->next;
				}

				newfile->next = cur_node;
				prev_node->next = newfile;
			}
			else{
				while (cur_node != NULL && (cur_node->fileInfoList->next->statbuf.st_gid > newfile->fileInfoList->next->statbuf.st_gid )){
					prev_node = cur_node;
					cur_node = cur_node->next;
				}

				newfile->next = cur_node;
				prev_node->next = newfile;
			}

		}
		else if(flag_mode==1){
			if(flag_asc==1){
				while (cur_node != NULL && (cur_node->fileInfoList->next->statbuf.st_mode < newfile->fileInfoList->next->statbuf.st_mode )){
					prev_node = cur_node;
					cur_node = cur_node->next;
				}

				newfile->next = cur_node;
				prev_node->next = newfile;
			}
			else{
				while (cur_node != NULL && (cur_node->fileInfoList->next->statbuf.st_mode > newfile->fileInfoList->next->statbuf.st_mode )){
					prev_node = cur_node;
					cur_node = cur_node->next;
				}

				newfile->next = cur_node;
				prev_node->next = newfile;
			}

		}
		

    }    
}



int list_command(int argc, char *argv[])
{

	struct dirent **namelist;
	int listcnt;
	FILE *fp;
	fileList *list_list=(fileList *)malloc(sizeof(fileList));
	memset(list_list,0,sizeof(fileList));

	fileList *dupslist=(fileList *)malloc(sizeof(fileList));
	memset(dupslist,0,sizeof(fileList));
	if(dups_list_h->next==NULL)//다음번에해도 되게 만들어야함
		return 1;

	dupslist=dups_list_h->next;
	int flag_fileset=1, flag_filename=0, flag_size=1, flag_uid=0, flag_gid=0, flag_mode=0, flag_asc=1;


	if(!strcmp(argv[0],"list")){
		int opt;
		if (argc != 1 && argc!=3 && argc!=5 && argc != 7){
			printf("Usage: list -l [LIST_TYPE] -c [CATEGORY] -o [ORDER]\n");
			return 1;
		}

		while((opt=getopt(argc, argv, "l:c:o:"))!=-1){
			switch(opt){
				case 'l':
					if (!strcmp(optarg, "fileset")){
						
						flag_fileset=1;
						break;
					}

					else if (!strcmp(optarg, "filelist")){
						flag_fileset=0;
						break;
					}

					else{
						printf("only, fileset or filelist\n");
						return 1;
					}
					break;

				case 'c':
					if (!strcmp(optarg, "filename")){
						flag_size=0;
						flag_filename=1;
						break;
					}
					else if (!strcmp(optarg, "size")){
						flag_size=1;
						break;
					}
					else if (!strcmp(optarg, "uid")){
						flag_size=0;

						flag_uid=1;
						break;
					}
					else if (!strcmp(optarg, "gid")){
						flag_size=0;

						flag_gid=1;
						break;
					}
					else if (!strcmp(optarg, "mode")){
						flag_size=0;

						flag_mode=1;
						break;
					}

					else{
						printf("Usage: list -l [LIST_TYPE] -c [CATEGORY] -o [ORDER]\n");
						return 1;
					}
					break;
				case 'o':
					if (!strcmp(optarg, "1")){
						flag_asc=1;
						break;
					}
					else if (!strcmp(optarg, "-1")){
						flag_asc=0;
						break;
					}
					else{
						printf("only 1, -1 \n");
						return 1;
					}
						break;
			
				case '?':
					printf("Usage: trash -c [CATEGORY] -o [ORDER]\n");
					return 1;			
			}
		}
	}
	else{
		printf("Usage: trash -c [CATEGORY] -o [ORDER]\n");
		return 1;
	}


	while (dupslist != NULL) {	
		list_list_append(list_list, dupslist, dupslist->filesize,dupslist->fileInfoList->next->path, dupslist->hash, flag_fileset, flag_filename, flag_size, flag_uid, flag_gid, flag_mode, flag_asc);
		dupslist=dupslist->next;
	}


	

	filelist_print_format(list_list);

	
	refind();


	return 0;

}




int tokenize_trash(char *input, char *argv[])
{
	char *ptr = NULL;
	int argc = 0;
	ptr = strtok(input, "\t");

	while (ptr != NULL){
		argv[argc++] = ptr;
		ptr = strtok(NULL, "\t");
	}

	argv[argc-1][strlen(argv[argc-1])-1] = '\0';

	return argc;
}

int trashlist_append(trashList *head, long long filesize, char *path, char *trash_date, char *trash_time, char *trashedpath,int flag_filename, int flag_size, int flag_date, int flag_time,int flag_asc)//trash_list 추가 및 옵션 이용해 정렬
{
    trashList *newfile = (trashList *)malloc(sizeof(trashList));
    memset(newfile, 0, sizeof(trashList));

    newfile->size = filesize;
    strcpy(newfile->path, path);
    strcpy(newfile->date, trash_date);
	strcpy(newfile->time, trash_time);
	strcpy(newfile->trashed, trashedpath);

	newfile->next = NULL;

    if (head->next == NULL) {
        head->next = newfile;
    }   

    else {
		
        trashList *cur_node = head->next, *prev_node = head, *next_node;
	
		if(flag_filename==1){
			if(flag_asc==1){
				while (cur_node != NULL && strcmp(cur_node->path, newfile->path)<0) {
					prev_node = cur_node;
					cur_node = cur_node->next;
				}

				newfile->next = cur_node;
				prev_node->next = newfile;
			}
			else{
				while (cur_node != NULL && strcmp(cur_node->path, newfile->path)>0) {
					prev_node = cur_node;
					cur_node = cur_node->next;
				}

				newfile->next = cur_node;
				prev_node->next = newfile;
			}

		}
		else if(flag_date==1){
			if(flag_asc==1){
				while (cur_node != NULL && strcmp(cur_node->date , newfile->date)<0) {
					prev_node = cur_node;
					cur_node = cur_node->next;
				}

				newfile->next = cur_node;
				prev_node->next = newfile;
			}
			else{
				while (cur_node != NULL && strcmp(cur_node->date , newfile->date)>0) {
					prev_node = cur_node;
					cur_node = cur_node->next;
				}

				newfile->next = cur_node;
				prev_node->next = newfile;
			}

		}
		else if(flag_size==1){
			if(flag_asc==1){
				while (cur_node != NULL && (cur_node->size < newfile->size)) {
					prev_node = cur_node;
					cur_node = cur_node->next;
				}

				newfile->next = cur_node;
				prev_node->next = newfile;
			}
			else{
				while (cur_node != NULL && (cur_node->size > newfile->size)) {
					prev_node = cur_node;
					cur_node = cur_node->next;
				}

				newfile->next = cur_node;
				prev_node->next = newfile;
			}

		}
		else if(flag_time==1){
			if(flag_asc==1){
				while (cur_node != NULL && strcmp(cur_node->time , newfile->time)<0) {
					prev_node = cur_node;
					cur_node = cur_node->next;
				}

				newfile->next = cur_node;
				prev_node->next = newfile;
			}
			else{
				while (cur_node != NULL && strcmp(cur_node->time , newfile->time)>0) {
					prev_node = cur_node;
					cur_node = cur_node->next;
				}

				newfile->next = cur_node;
				prev_node->next = newfile;
			}

		}
		

    }    

	return 0;
}

int trash_command(int argc, char *argv[])
{
	struct dirent **namelist;
	int listcnt;
	FILE *fp;
	trash_list=(trashList *)malloc(sizeof(trashList));

	get_trash_path();

	trash_list->next=NULL;

	//.Trash/info/ 디렉토리에서 가져와서 찾음
	listcnt = get_dirlist(trash_info, &namelist);

	if(listcnt<3){//. .. 제외하고 빈 것 체크
		printf("Trash bin is empty\n");
		return 1;
	}

	printf(" \tFILENAME\t\t\t\t\t\t\tSIZE\t\tDELETION DATE\t\tDELETION TIME\n");

	int flag_filename=1, flag_size=0, flag_date=0, flag_time=0, flag_asc=1;


	if(!strcmp(argv[0],"trash")){
		int opt;
		if (argc != 1 && argc!=3 && argc!=5){
			printf("Usage: trash -c [CATEGORY] -o [ORDER]\n");
			return 1;
		}

		while((opt=getopt(argc, argv, "c:o:"))!=-1){
			switch(opt){
				case 'c':
					if (!strcmp(optarg, "filename")){
						flag_filename=1;
						break;
					}
					else if (!strcmp(optarg, "size")){
						flag_filename=0;
						flag_size=1;
						break;
					}
					else if (!strcmp(optarg, "date")){
						flag_filename=0;

						flag_date=1;
						break;
					}
					else if (!strcmp(optarg, "time")){
						flag_filename=0;

						flag_time=1;
						break;
					}
					else{
						printf("only, filename, size, date, time\n");
						return 1;
					}
					break;
				case 'o':
					if (!strcmp(optarg, "1")){
						flag_asc=1;
						break;
					}
					else if (!strcmp(optarg, "-1")){
						flag_asc=0;
						break;
					}
					else{
						printf("only 1, -1 \n");
						return 1;
					}
						break;
			
				case '?':
					printf("Usage: trash -c [CATEGORY] -o [ORDER]\n");
					return 1;			
			}
		}
	}
	else{
		printf("Usage: trash -c [CATEGORY] -o [ORDER]\n");
		return 1;
	}


	for (int i = 0; i < listcnt; i++){
		char filename[PATHMAX*2];
		long long filesize;
		char filepath[PATHMAX];
		char time_trash[TIME_LENGTH];
		char date_trash[DATE_LENGTH];
		char line[STRMAX];

		if (!strcmp(namelist[i]->d_name, ".") || !strcmp(namelist[i]->d_name, ".."))
			continue;

		filesize = atoll(namelist[i]->d_name);
		
		sprintf(filename, "%s/%s", trash_info, namelist[i]->d_name);

		if ((fp = fopen(filename, "r")) == NULL){
			printf("ERROR: fopen error for %s\n", filename);
			continue;
		}

		while (fgets(line, sizeof(line), fp) != NULL){//읽어오는 부분 수정
			int count;
			char *trash_read[4];
			memset(trash_read, 0, 4);
			count=tokenize_trash(line, trash_read);


			if(trashlist_append(trash_list, filesize, trash_read[0], trash_read[1], trash_read[2],trash_read[3],flag_filename, flag_size,flag_date, flag_time,flag_asc )>0)
				return 1;


				
		}

		fclose(fp);
	}

	trashList *trashlist_cur = trash_list->next;
	int number = 1;	

	while (trashlist_cur != NULL){
		
		printf("[ %d]  %s\t\t\t\t\t\t%lld\t\t%s\t\t%s\n", number++, trashlist_cur->path, trashlist_cur->size, trashlist_cur->date, trashlist_cur->time);
		
		trashlist_cur = trashlist_cur->next;
	}	

	printf("\n");

	return 0;

}

trashList *trashList_delete_node(trashList *head, char *path)
{
	trashList *deleted;

	if (!strcmp(head->next->path, path)){
		deleted = head->next;
		head->next = head->next->next;
		return head->next;
	}
	else {
		trashList *fileinfo_cur = head->next;

		while (fileinfo_cur->next != NULL){
			if (!strcmp(fileinfo_cur->next->path, path)){
				deleted = fileinfo_cur->next;
				
				fileinfo_cur->next = fileinfo_cur->next->next;
				return fileinfo_cur->next;
			}

			fileinfo_cur = fileinfo_cur->next;
		}
	}
}

int refind(void)
{
	
	get_trash_path();

	dirList *dirlist = (dirList *)malloc(sizeof(dirList));
	memset(dirlist, 0, sizeof(dirList));


	memset(dups_list_h, 0, sizeof(fileList));
	

	dups_list_h = (fileList *)malloc(sizeof(fileList));

	


	memset(dups_list_h, 0, sizeof(fileList));

	get_same_size_files_dir();
	get_logfile();

	dirlist->next=NULL;
	dirlist_append(dirlist, targetDir);

	dir_traverse(dirlist);

	find_duplicates();


	remove_no_duplicates();
	return 0;
}


int restore_command(int argc, char *argv[])
{
	struct dirent **namelist;
	int listcnt;
	FILE *fp;
	FILE *fpp;

	int restore_idx=atoi(argv[1]);
	char restore_path[PATHMAX];

	if(argc != 2){
		printf("USAGE: restore [RESTORE_INDEX]\n");
		return 1;
	}

	get_trash_path();
	get_logfile();


	//.Trash/info/ 디렉토리에서 가져와서 찾음
	listcnt = get_dirlist(trash_info, &namelist);

	if(listcnt<3){//. .. 제외하고 빈 것 체크
		printf("Trash bin is empty\n");
		return 1;
	}


	int i;
	trashList *restoreList=(trashList *)malloc(sizeof(trashList));
	restoreList=trash_list;




	for(i=0;i<restore_idx;i++){

		restoreList=restoreList->next;

	}


	strcpy(restore_path,restoreList->path);
	
	for (int i = 0; i < listcnt; i++){
		char filename[PATHMAX*2];
		long long filesize;
		char filepath[PATHMAX];
		char date_trash[DATE_LENGTH];
		char line[STRMAX];

		
		char trashTime[STRMAX];
		time_t timer;


		if (!strcmp(namelist[i]->d_name, ".") || !strcmp(namelist[i]->d_name, ".."))
			continue;
		
		sprintf(filename, "%s%s", trash_info, namelist[i]->d_name);
		


		if ((fp = fopen(filename, "r")) == NULL){
			printf("ERROR: fopen error for %s\n", filename);
			continue;
		}

	

		while (fgets(line, sizeof(line), fp) != NULL){//읽어오는 부분 수정
			int count;
			char *trash_read[4];
			memset(trash_read, 0, 4);
			count=tokenize_trash(line, trash_read);
			//log 기록

			
			if(!strcmp(restore_path, trash_read[0])){	
				if ((fpp = fopen(logfile, "a")) == NULL){
					printf("ERROR: fopen error for %s\n", logfile);
					continue;
				}
				timer=time(NULL);
				trash_time(localtime(&timer), trashTime);

				fprintf(fpp, "%s\t%s\t%s\t%s\n","[RESTORE]", trash_read[0], trashTime, USER);
				fclose(fpp);
				
				printf("[RESTORE] success for %s\n", trash_read[0]);

				// 윈위치로 이동
				if (rename(trash_read[3], trash_read[0]) == -1){
					printf("ERROR: Fail to move duplicates to Restore\n");
					continue;
				}				
				
				refind();
				fclose(fp);

				trashList_delete_node(trash_list, restore_path);
				trashList *rewriteList=trash_list->next;


				remove_files(trash_info);
				
				while(rewriteList != NULL){
					FILE *rfp;
					char reinfo[PATHMAX*2];
					
					sprintf(reinfo, "%s%lld", trash_info, rewriteList->size);

					if ((rfp = fopen(reinfo, "a")) == NULL){
						printf("ERROR: fopen error for %s\n", reinfo);
						continue;
					}

					fprintf(rfp, "%s\t%s\t%s\t%s\n", rewriteList->path, rewriteList->date, rewriteList->time, rewriteList->trashed);




					rewriteList=rewriteList->next;
					fclose(rfp);
				}

				char *a[1]={"trash"};

				trash_command(1, a);

				return 0;

			}


		}

	}

	refind();

	return 0;
}
