#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sqlite3.h>
#include <sys/stat.h>
#include <limits.h>

extern int errno;
int PORT;
char PORTC[10],ip[20],ports[10];
int pid;

void signin(int sd);
void login(int sd);
void myfileslocal(sqlite3 *database);
void myfilesserver(int sd);
void addfile(sqlite3 *database,int sd);
void removefile(sqlite3 *database,int sd);
void searchfiles(int sd);
void getfile(int sd,sqlite3 *database);
void filetransfer(int client,struct sockaddr_in from, socklen_t len);
int initiate_connection(char argv1[],char argv2[]);
int initiate_listen();
int getsize(int fd);
void int_to_char(char c[], int x);

int main(int argc, char *argv[])
{
	char adr[50];
	bzero(adr,50);
	bzero(ip,20);
	bzero(ports,10);
	printf("Give server adress: ");
	fflush(stdout);
	int r = read(0,adr,50),i;
	i = r - 1;
	while(i > 0 && adr[i] != ':')
		i--;
	if(i == 0 || i == r-2)
	{
		printf("Wrong format: <ip:port>");
		fflush(stdout);
		exit(0);
	}
	strncpy(ip,adr,i);
	strncpy(ports,adr+i+1,r-i-2);
	int sd = initiate_connection(ip,ports);
	int ld = initiate_listen();
	int_to_char(PORTC,PORT);
	pid = fork();
	if(pid != 0){
		close(ld);
		sqlite3 *database;
		int result_code;
		result_code = sqlite3_open("client.db",&database);
		if(result_code)
		{
			printf("Can't open database: %s", sqlite3_errmsg(database));
			sqlite3_close(database);
			exit(0);
		}	
		char comm[20],msg[100];
		int r,length;
		while(1)
		{
			bzero(comm,20);
			printf("Command: ");
			fflush(stdout);
			r = read(0,comm,20);
			write(sd,comm,r);
			if(strncmp(comm,"quit",4) == 0)
			{
				if(kill(pid,SIGTERM) == -1)
				{
					perror("Error");
				}
				exit(0);
			}
			bzero(msg,100);
			r = read(sd,msg,100);
			if(r == 0)
			{
				printf("The server has been closed\n");
				fflush(stdout);
				exit(0);
			}
			if(strncmp(msg,"signin",6) == 0)
			{
				signin(sd);
			}
			else if(strncmp(msg,"login",5) == 0)
			{
				login(sd);
			}
			else if(strncmp(msg,"addfile",7) == 0)
			{
				addfile(database,sd);
			}
			else if(strncmp(msg,"removefile",10) == 0)
			{
				removefile(database,sd);
			}
			else if(strncmp(msg,"getfile",7) == 0)
			{
				getfile(sd,database);
			}
			else if(strncmp(msg,"searchfiles",7) == 0)
			{
				searchfiles(sd);
			}
			else if(strncmp(msg,"myfileslocal",12) == 0)
			{
				myfileslocal(database);
			}
			else if(strncmp(msg,"myfilesserver",13) == 0)
			{
				myfilesserver(sd);
			}
			else 
			{
				printf("%s\n",msg);
				fflush(stdout);
			}
		}
	}
	else
	{
		close(sd);
		int client;
		socklen_t len;
		struct sockaddr_in from;
		while(1)
		{
			bzero (&from, sizeof (from));
			len = sizeof (from);
			client = accept (ld, (struct sockaddr *) &from, &len);
		    if (client < 0)
		   	{
		   		perror ("Eroare la accept().\n");
		   	 	continue;
		   	}
			if(fork() == 0){
				filetransfer(client,from,len);
				exit(0);
			}
		}
	}
	return 0;
}

void signin(int sd)
{
	int r;
	char msg[100];
	printf("username: ");
	fflush(stdout);
	bzero(msg,100);
	if((r = read(0,msg,20)) < 0)
	{
		printf("Error occured!");
		fflush(stdout);
	}
	write(sd,msg,r-1);
	printf("password: ");
	fflush(stdout);
	bzero(msg,100);
	if((r = read(0,msg,20)) < 0)
	{
		printf("Error occured!");
		fflush(stdout);
	}
	write(sd,msg,r-1);
	bzero(msg,100);
	if(read(sd,msg,20) == 0)
	{
		printf("The server has been closed\n");
		fflush(stdout);
		exit(0);
	}
	printf("%s",msg);
	fflush(stdout);
}

void login(int sd)
{
	int r;
	char msg[100];
	printf("username: ");
	fflush(stdout);
	bzero(msg,100);
	if((r = read(0,msg,20)) < 0)
	{
		printf("Error occured!");
		fflush(stdout);
	}
	write(sd,msg,r);
	printf("password: ");
	fflush(stdout);
	bzero(msg,100);
	if((r = read(0,msg,20)) < 0)
	{
		printf("Error occured!");
		fflush(stdout);
	}
	write(sd,msg,r);
	bzero(msg,100);
	if(read(sd,msg,20) == 0)
	{
		printf("The server has been closed\n");
		fflush(stdout);
		exit(0);
	}
	if(strncmp(msg,"Accept",6) == 0)
	{
		write(sd,PORTC,r);
	}
	else
	{
		printf("Acces denied\n");
		fflush(stdout);
	}
}

void myfileslocal(sqlite3 *database)
{
	char **list,sql[100];
	int nr_rows,nr_columns,i,result_code;
	char *pzErrmsg = 0;
	bzero(sql,100);
	strcpy(sql,"select file_id,filepath from files where server_adr = '");
	strcat(sql,ip);
	strcat(sql,"';");
	result_code = sqlite3_get_table(database,sql,&list,&nr_rows,&nr_columns,&pzErrmsg);
	if(result_code != SQLITE_OK)
	{
		printf("SQL error: %s", pzErrmsg);
		sqlite3_free_table(pzErrmsg);
		exit(0);
	}
	if(nr_rows == 0)
	{
		printf("You don't have any files shared\n");
		fflush(stdout);
		return;
	}
	printf("%s   | %s\n",list[0],list[1]);
	for(i = nr_columns; i < (nr_rows + 1) * nr_columns; i += nr_columns)
		printf("%s | %s\n",list[i],list[i+1]);
	fflush(stdout);
}

void addfile(sqlite3 *database, int sd)
{
	char msg[150],file_id[10],filepath[150];
	int r;
	printf("filepath: ");
	fflush(stdout);
	bzero(msg,150);
	r = read(0,msg,150);
	struct stat buff;
	bzero(filepath,150);
	strncpy(filepath,msg,strlen(msg)-1);
	if(stat(filepath,&buff) == -1)
	{
		printf("File not found\n");
		fflush(stdout);
		write(sd,"-1",2);
		return;
	}
	int i = strlen(filepath) - 1;
	while(i > 0)
	{
		if(filepath[i] == '/')
			break;
		else
			i--;
	}
	write(sd,msg+i+1,r-i-2);
	sleep(1);
	char c[100];
	bzero(c,100);
	int_to_char(c, buff.st_size);
	write(sd,c,strlen(c));
	r = read(sd,file_id,10);
	if(r == 0)
	{
		printf("The server has been closed\n");
		fflush(stdout);
		exit(0);
	}
	char actualpath [PATH_MAX+1];
	realpath(filepath, actualpath);
	
	char sql[150];
	int result_code;
	bzero(sql,150);
	strcpy(sql,"insert into files values ('");
	strcat(sql,ip);
	strcat(sql,"',");
	strcat(sql,file_id);
	strcat(sql,",'");
	strcat(sql,actualpath);
	strcat(sql,"');");
	char *pzErrmsg = 0;
	result_code = sqlite3_exec(database,sql,0,0,&pzErrmsg);
	if(result_code != SQLITE_OK)
	{
		printf("SQL error: %s", pzErrmsg);
		sqlite3_free_table(pzErrmsg);
		exit(0);
	}
	else
	{
		printf("File added!\n");
		fflush(stdout);
	}
}

void removefile(sqlite3 *database,int sd)
{
	char sql[100],file_id[100];
	int result_code,r;
	bzero(file_id,100);
	bzero(sql,100);
	printf("File id: ");
	fflush(stdout);
	r = read(0,file_id,100);
	write(sd,file_id,r-1);
	file_id[r-1] = '\0';
	strcpy(sql,"delete from files where file_id = ");
	strcat(sql,file_id);
	strcat(sql," and server_adr = '");
	strcat(sql,ip);
	strcat(sql,"';");
	char *pzErrmsg = 0;
	result_code = sqlite3_exec(database,sql,0,0,&pzErrmsg);
	if(result_code != SQLITE_OK)
	{
		printf("SQL error: %s", pzErrmsg);
		sqlite3_free_table(pzErrmsg);
		return;
	}
}

void searchfiles(int sd)
{
	printf("Search criteria (allfiles/filename/filetype/filelength): ");
	int x;
	fflush(stdout);
	char msg[100];
	bzero(msg,100);
	int r = read(0,msg,50);
	if(strncmp(msg,"allfiles\n",9) == 0)
		x = 0;
	else
		x = 1;
	write(sd,msg,r-1);
	bzero(msg,100);
	if(read(sd,msg,100) == 0)
	{
		printf("The server has been closed\n");
		fflush(stdout);
		exit(0);
	}
	if(strncmp(msg,"ok",2) != 0){
		printf("%s\n",msg);
		fflush(stdout);
		return;
	}
	if(x == 1)
	{
		bzero(msg,100);
		printf("Search for: ");
		fflush(stdout);
		r = read(0,msg,100);
		write(sd,msg,r-1);
	}
	bzero(msg,100);
	if(read(sd,msg,100) == 0)
	{
		printf("The server has been closed\n");
		fflush(stdout);
		exit(0);
	}
	if(strncmp(msg,"ok",2) != 0){
		printf("%s\n",msg);
		fflush(stdout);
		return;
	} else
		write(sd,"1",1);
	bzero(msg,100);
	if(read(sd,msg,100) == 0)
	{
		printf("The server has been closed\n");
		fflush(stdout);
		exit(0);
	}
	int nr_rows = atoi(msg),i,size;
	write(sd,"1",1);
	char buff[500];
	for(i = 0; i < nr_rows; i++)
	{
		size = getsize(sd);
		bzero(buff,500);
		if(read(sd,buff,size) == 0)
		{
			printf("The server has been closed\n");
			fflush(stdout);
			exit(0);
		}
		printf("%s\n",buff);
		fflush(stdout);
	}
}

void myfilesserver(int sd)
{
	char msg[100];
	bzero(msg,100);
	if(read(sd,msg,100) == 0)
	{
		printf("The server has been closed\n");
		fflush(stdout);
		exit(0);
	}
	if(strncmp(msg,"ok",2) != 0){
		printf("%s\n",msg);
		fflush(stdout);
		return;
	} else
		write(sd,"1",1);
	bzero(msg,100);
	if(read(sd,msg,100) == 0)
	{
		printf("The server has been closed\n");
		fflush(stdout);
		exit(0);
	}
	int nr_rows = atoi(msg),i,size;
	write(sd,"1",1);
	char buff[500];
	for(i = 0; i < nr_rows; i++)
	{
		size = getsize(sd);
		bzero(buff,500);
		if(read(sd,buff,size) == 0)
		{
			printf("The server has been closed\n");
			fflush(stdout);
			exit(0);
		}
		printf("%s\n",buff);
		fflush(stdout);
	}
}

void getfile(int sd, sqlite3 *database)
{
	char filename[100],file_id[100],ip[20],port[10],buff[500],filelength[50],confirm[5],filepath[300];
	int r,length;
	off_t l = 0;
	bzero(filelength,50);
	bzero(file_id,100);
	bzero(filename,100);
	bzero(filepath,300);
	printf("file id: ");
	fflush(stdout);
	r = read(0,file_id,100);
	
	printf("file path: ");
	fflush(stdout);
	r = read(0,filepath,300);
	if(filepath[r-2] != '/')
		filepath[r-1] = '/';
	else
		filepath[r-1] = '\0';
	write(sd,file_id,r-1);
	bzero(confirm,5);
	r = read(sd,confirm,5);
	if(r == 0)
	{
		printf("The server has been closed\n");
		fflush(stdout);
		exit(0);
	}
	if(strncmp(confirm,"-1",2) == 0)
	{
		printf("No users online to transfer the file selected\n");
		fflush(stdout);
		return;
	}
	else if(strncmp(confirm,"-2",2) == 0)
	{
		printf("Error from server\n");
		fflush(stdout);
		return;
	}
	if(read(sd,filename,100) == 0)
	{
		printf("The server has been closed\n");
		fflush(stdout);
		exit(0);
	}
	if(read(sd,filelength,50) == 0)
	{
		printf("The server has been closed\n");
		fflush(stdout);
		exit(0);
	}
	length = atoi(filelength);
	strcat(filepath,filename);
	while(1){
		bzero(ip,20);
		bzero(port,10);
		if(read(sd,ip,20) == 0)
		{
			printf("The server has been closed\n");
			fflush(stdout);
			exit(0);
		}
		if(strncmp(ip,"end",3) == 0)
		{
			printf("File couldn't be transfered\n");
			fflush(stdout);
			break;
		}
		if(read(sd,port,10) == 0)
		{
			printf("5The server has been closed\n");
			fflush(stdout);
			exit(0);
		}
		int td = initiate_connection(ip,port);
		write(td,file_id,strlen(file_id)-1);
		bzero(confirm,5);
		read(td,confirm,2);
		if(strncmp(confirm,"-1",2) == 0)
		{
			write(sd,"error",5);
			close(td);
			continue;
		}
		int fd = open(filepath,O_WRONLY | O_CREAT | O_TRUNC);
    	if (fd < 0)
   		{
   			perror ("Eroare la open().\n");
			exit(0);
   		}
		while(1)
		{
			r = read(td,buff,500);
	    	if (r < 0)
	  	    {
	   			perror ("Eroare la read().\n");
				exit(0);
	   		}
			if(r == 0)
				break;
			write(fd,buff,r);
			if(r < 500)
				break;
		}
		struct stat buff;
		stat(filename,&buff);
		if(buff.st_size == length){
			printf("File received\n");
			fflush(stdout);
			write(sd,"ok",2);
			close(fd);
			break;
		}
		else 
			write(sd,"error",5);
		close(td);
		close(fd);
	}
	char actualpath [PATH_MAX+1];
	realpath(filepath, actualpath);

	char sql[150];
	int result_code;
	bzero(sql,150);
	strcpy(sql,"insert into files values ('");
	strcat(sql,ip);
	strcat(sql,"',");
	strcat(sql,file_id);
	strcat(sql,",'");
	strcat(sql,actualpath);
	strcat(sql,"');");
	char *pzErrmsg = 0;
	result_code = sqlite3_exec(database,sql,0,0,&pzErrmsg);
	if(result_code != SQLITE_OK)
	{
		printf("SQL error: %s", pzErrmsg);
		sqlite3_free_table(pzErrmsg);
		exit(0);
	}
}

void filetransfer(int client,struct sockaddr_in from, socklen_t len)
{
	sqlite3 *database;
	int result_code,nr_rows,nr_columns;
	result_code = sqlite3_open("client.db",&database);
	if(result_code)
	{
		printf("Can't open database: %s", sqlite3_errmsg(database));
		sqlite3_close(database);
		exit(0);
	}	
	char buff[500],file_id[100],sql[300];
	char **filepath;
	off_t l = 0;
	bzero(file_id,100);
	read(client,file_id,100);
	int fd;
	bzero(sql,300);
	strcpy(sql,"select filepath from files where file_id = ");
	strcat(sql,file_id);
	strcat(sql,";");
	char *pzErrmsg = 0;
	result_code = sqlite3_get_table(database,sql,&filepath,&nr_rows,&nr_columns,&pzErrmsg);
	if(result_code != SQLITE_OK)
	{
		printf("SQL error: %s", pzErrmsg);
		sqlite3_free_table(pzErrmsg);
		exit(0);
	}
	fd = open(filepath[1],O_RDONLY);
	int r;
    if (fd < 0)
   	{
		write(client,"-1",2);
   		perror ("Eroare la open().\n");
		exit(0);
   	}
	else write(client,"+1",2);
	sendfile(fd,client,0,&l,NULL,0);
	sqlite3_free_table(filepath);
	sqlite3_close(database);
	close(fd);
}

int initiate_connection(char argv1[],char argv2[])
{
	int port;
	int sd;
	struct sockaddr_in server;
    port = atoi (argv2);
    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
      {
        perror ("Error socket().\n");
       exit(errno);
      }
	  
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv1);
    server.sin_port = htons (port);

    if (connect (sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
      {
        perror ("Error connect().\n");
        exit(errno);
      }
	  return sd;
}

int initiate_listen()
{
    struct sockaddr_in server;
    int sd;	
    int optval=1; 
	
    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
		perror ("Error socket().\n");
        exit(errno);
	}

    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR,&optval,sizeof(optval));

    bzero (&server, sizeof (server));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl (INADDR_ANY);
    server.sin_port = 0;

    if (bind (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
	{
		perror ("Error bind().\n");
        exit(errno);
	}
	
	socklen_t len = sizeof(server);
	if (getsockname(sd, (struct sockaddr *)&server, &len) == -1) {
	    perror("Error getsockname");
	    exit(errno);
	}

	PORT = ntohs(server.sin_port);
	
    if (listen (sd, 5) == -1)
    {
		perror ("Error listen().\n");
        exit(errno);
   	}
	return sd;
}

int getsize(int fd)
{
	int i = 0,size;
	char c, charsize[5];
	while(1){
		read(fd,&c,1);
		if(c == ' ' || c == '\n')
			break;
		else
			charsize[i] = c;
		i++;
	}
	size = atoi(charsize);
	return size;
}

void int_to_char(char c[], int x)
{
	int nr=0,n=0;
	char b[20];
	do
	{
		b[nr] = x % 10 + 48;
		x = x / 10;
		nr++;
	}while(x != 0);
	
	do
	{
		c[n] = b[nr-1];
		n++;
		nr--;
	}while(nr != 0);
	c[n] = '\0';
}
