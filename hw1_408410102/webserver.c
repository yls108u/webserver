#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#define PORT 80
#define BUFSIZE 2048

struct {
	char *ext;
	char *filetype;
} extensions [] = {
	{"gif","image/gif"},
	{"jpg","image/jpeg"},
	{"jpeg","image/jpeg"},
	{"png","image/png"},
	{"zip","image/zip"},
	{"gz","image/gz"},
	{"tar","image/tar"},
	{"htm","text/html"},
	{"html","text/html"},
	{"exe","text/plain"},
	{0,0}
};

void my_strcpy(char *pptr, char *qqtr)
{
	char *ptr = pptr;
	char *qtr = qqtr;
	int len = strlen(ptr);

	ptr = ptr + len;
	while(!isspace(*qtr)){
		*ptr = *qtr;
		ptr++;
		qtr++;
	}
	*ptr = '\0';

	return ;
}

void rewrite_html(char *filename)
{
	FILE* html;
	int i = 0;
	char *ptr = NULL;
	char buffer[4096] = {0};
	char remain[1024] = {0};

	html = fopen("./index.html", "r");
	fread(buffer, sizeof(char), 4096, html);
	ptr = strstr(buffer, "</form>") + 7;
	fclose(html);

	html = fopen("./index.html", "w+");

	while(*ptr != '\0'){
		remain[i] = *ptr;
		*ptr = '\0';
		i++;
		ptr++;
	}
	// printf("remain: %s\n", remain);

	ptr = strstr(buffer, "</form>") + 7;
	strcat(ptr, "<img src=\"./");
	strcat(ptr, filename);
	strcat(ptr, "\">");
	strcat(ptr, "\t<a href=\"./");
	strcat(ptr, filename);
	strcat(ptr, "\" download=\"");
	strcat(ptr, filename);
	strcat(ptr, "\">點選下載");
	strcat(ptr, filename);
	strcat(ptr, "</a>\n");
	ptr = strstr(ptr, "\n") + 1;

	i = 0;
	while(remain[i] != '\0'){
		*ptr = remain[i];
		i++;
		ptr++;
	}
	// printf("new file: %s\n", buffer);

	fwrite(buffer, sizeof(char), strlen(buffer), html);
	fclose(html);

	return ;
}

void upload_file_handler(int fd, long int reqq, char *file, char *file_head, char *buffer, char *end_boundary, int long filesize)
{
	FILE *txt;
	char *ptr, *qtr, *end;
	char up_time[128] = {0};
	char filename[128] = {0};
	char filetype[128] = {0};
	char buffer2[BUFSIZE + 1] = {0};
	long int req = reqq;
	time_t now;
    struct tm *timenow;

	if((ptr = strstr(file, "filename=")) == NULL){
		printf("read file error, return\n");
		return ;
	}

	time(&now);
    timenow = localtime(&now);
	strcpy(up_time, asctime(timenow));
	qtr = up_time;
	// printf("time:%s\n",up_time);
	while(*qtr != '\0'){
		if(isspace(*qtr) || *qtr == ':'){
			*qtr = '_';
		}
		qtr++;
	}
	// printf("time:%s\n",up_time);

	ptr = ptr + strlen("filename=") + 1;
	ptr = strstr(ptr, ".");
	strcpy(filename, up_time);
	strcat(filename, "upload");
	my_strcpy(filename, ptr);
	filename[strlen(filename) - 1] = '\0';
	// printf("filename:%s\n",filename);
	// strcpy(filename, "upload");
	// strcat(filename, ptr);

	ptr = strstr(ptr, "Content-Type: ");
	ptr += strlen("Content-Type: ");
	my_strcpy(filetype, ptr);
	filetype[strlen(filetype) - 1] = '\0';

	ptr = strstr(ptr, "\n");
	ptr += 3;

	if((end = strstr(file, end_boundary)) != NULL){
		txt = fopen(filename, "wb+");
		memcpy(buffer2, ptr, end - ptr);
		fwrite(buffer2, sizeof(char), end - ptr, txt);
		fclose(txt);
	}
	else{
		txt = fopen(filename, "wb+");
		memcpy(buffer2, ptr, BUFSIZE);
		fwrite(buffer2, sizeof(char), BUFSIZE - (ptr - file_head), txt);
		fclose(txt);
	}

	long int already_read = BUFSIZE - (ptr - file_head);
	while(filesize > already_read){
		memset(buffer2, 0, BUFSIZE + 1);
		long req = read(fd, buffer2, BUFSIZE);
		txt = fopen(filename, "ab+");
		fwrite(buffer2, sizeof(char), BUFSIZE, txt);
		fclose(txt);
		memset(buffer2, 0, BUFSIZE + 1);
		already_read += BUFSIZE;
	}
	rewrite_html(filename);

	return ;
}

void socket_handler(int fd)
{
	int file_fd;
	int POST = 0;
	long int req;
	long int filesize;
	char *fstr;
	static char buffer[BUFSIZE + 1];

	req = read(fd, buffer, BUFSIZE);

    printf("********************* New request *********************\n");
	// printf("buffer:\n%s", buffer);

	char *ptr = buffer, *qtr = buffer;
	char boundary[128] = {0};
	char end_boundary[128] = {0};
	if(!strncmp(ptr, "POST ", 5) && (ptr = strstr(ptr, "boundary=")) != NULL && (qtr = strstr(qtr, "Content-Length: ")) != NULL){
		POST = 1;

		qtr = qtr + strlen("Content-Length: ");
		filesize = atol(qtr);
		// printf("filesize: %ld\n", filesize);

		ptr = ptr + strlen("boundary=");
		strcat(boundary, "--");
		my_strcpy(boundary, ptr);
		strcpy(end_boundary, boundary);
		strcat(end_boundary, "--");
		// printf("boundary: %s\n", boundary);
		// printf("end boundary: %s\n", end_boundary);

		if((ptr = strstr(ptr, boundary)) != NULL){
			ptr = ptr + strlen(boundary);

			if((ptr = strstr(ptr, boundary)) != NULL){
				ptr = ptr + strlen(boundary);
				qtr = buffer;

				upload_file_handler(fd, req, ptr, qtr, buffer, end_boundary, filesize);
			}
		}
	}


	if(req == 0 || req == -1) exit(3);



	buffer[strlen(buffer)] = 0;

	//delete \r\n
	for(long i = 0; i < req; i++) if(buffer[i] == '\r' || buffer[i] == '\n') buffer[i] = 0;


	if(strncmp(buffer, "GET ", 4) && strncmp(buffer, "get ", 4) && strncmp(buffer, "POST ", 5) && strncmp(buffer, "post ",5)) exit(3);


	if(POST != 1){
		for(long i = 4; i < BUFSIZE; i++){
			if(buffer[i] == ' '){
				buffer[i] = 0;
				break;
			}
		}
	}
	else{
		for(long i = 5; i < BUFSIZE; i++){
			if(buffer[i] == ' '){
				buffer[i] = 0;
				break;
			}
		}
	}

	//read index.html
	if(!strncmp(&buffer[0], "GET /\0", 6) || !strncmp(&buffer[0], "get /\0", 6) || !strncmp(&buffer[0], "POST", 4) || !strncmp(&buffer[0], "post", 4)) strcpy(buffer, "GET /index.html\0");

    //check file
	int buflen = strlen(buffer);
    long kind;
	fstr = (char *)0;
	for(kind = 0; extensions[kind].ext != 0; kind++){
		int len = strlen(extensions[kind].ext);
		if(!strncmp(&buffer[buflen - len], extensions[kind].ext, len)){
			fstr = extensions[kind].filetype;
			break;
		}
	}
	if(fstr == 0) fstr = extensions[kind - 1].filetype;


	//open file
	if((file_fd = open(&buffer[5], O_RDONLY)) == -1) write(fd, "Failed to open file", 19);


    //response
	memset(buffer, 0, BUFSIZE);
	sprintf(buffer, "HTTP/1.0 200 OK\r\nContent-Type: %s\r\n\r\n", fstr);
	write(fd, buffer, strlen(buffer));
	while((req = read(file_fd, buffer, BUFSIZE)) > 0) write(fd, buffer, req);

	//exit(1);
}

void sigchld(int signo)
{
	pid_t pid;
	while((pid = waitpid(-1, NULL, WNOHANG)) > 0);
}

int main(int argc, char **argv)
{
	int listenfd, socketfd;
	struct sockaddr_in cli_addr, serv_addr;
    pid_t pid;
	socklen_t length;

	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	listen(listenfd, 64);
    signal(SIGCHLD, sigchld);

	while(1){	
        length = sizeof(cli_addr);
		if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0) exit(3);

		if((pid = fork()) < 0) exit(3);
		else{
			if(pid == 0){ //child
				close(listenfd);
				socket_handler(socketfd);
				exit(1);
			}
            else{ //parent
				close(socketfd);
			}
		}
	}
	return 0;
}
