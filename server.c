#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <sqlite3.h>

extern int errno;
void user(int client,struct sockaddr_in from, socklen_t len);
int initiate_server();
void int_to_char(char c[], int x);

int main()
{
	int sd = initiate_server();
	int client;
	socklen_t len;
	struct sockaddr_in from;
	while(1)
	{
		bzero (&from, sizeof (from));
		len = sizeof (from);
		client = accept (sd, (struct sockaddr *) &from, &len);
	    if (client < 0)
	   	{
	   		perror ("[server] Eroare la accept().\n");
	   	 	continue;
	   	}
		int pid = fork();
		if(pid == 0){
			user(client,from,len);
			exit(0);
		}
	}
}

void signin(int client,sqlite3 *database);
char * login(int client,struct sockaddr_in from, socklen_t len, sqlite3 *database);
void logout(sqlite3 *database, int client, char * user_id);
void insertintoconn(int client, struct sockaddr_in from, socklen_t len, sqlite3 *database, char * user_id);
void deletefromconn(sqlite3 *database, char *user_id);
void addfile(int client, sqlite3 *database, char *user_id);
void removefile(int client, sqlite3 *database, char *user_id);
void getfile(int client, sqlite3 *database, char *user_id);
void searchfiles(int client,sqlite3 *database,char *user_id);
void myfilesserver(int client,sqlite3 *database,char *user_id);

void user(int client,struct sockaddr_in from, socklen_t len)
{
	sqlite3 *database;
	int result_code,loggedin = 0;
	char *user_id,msg[100];
	result_code = sqlite3_open("server.db",&database);
	if(result_code)
	{
		printf("Can't open database: %s", sqlite3_errmsg(database));
		sqlite3_close(database);
		exit(0);
	}	
    
	while(1){
		if(read(client,msg,100) <= 0){
			break;
		}
		if(strncmp(msg,"signin",6) == 0)
		{
			write(client,"signin",6);
			signin(client,database);
		}
		else if(strncmp(msg,"login",5) == 0)
		{
			write(client,"login",5);
			user_id = login(client,from,len, database);
			if(strcmp(user_id,"-1")!= 0){
				insertintoconn(client,from,len,database,user_id);
				while(1)
				{
					if(read(client,msg,100) <= 0){
						break;
					}
					if(strncmp(msg,"logout",6) == 0)
					{
						logout(database,client,user_id);
						break;
					}
					else if(strncmp(msg,"addfile",7) == 0)
						addfile(client,database,user_id);
					else if(strncmp(msg,"removefile",10) == 0)
						removefile(client,database,user_id);
					else if(strncmp(msg,"getfile",7) == 0)
						getfile(client,database,user_id);
					else if(strncmp(msg,"searchfiles",11) == 0)
						searchfiles(client,database,user_id);
					else if(strncmp(msg,"quit",4) == 0)
					{
						logout(database,client,user_id);
						exit(0);
					}
					else if(strncmp(msg,"myfileslocal",12) == 0)
						write(client,"myfileslocal",12);
					else if(strncmp(msg,"myfilesserver",13) == 0)
						myfilesserver(client,database,user_id);
					else
					{
						write(client,"Unknown command",15);
					}
					
				}
			}
			else
				continue;
		}
		else if(strncmp(msg,"quit",4) == 0)
			break;
		else if(strncmp(msg,"myfiles",7) == 0)
			write(client,"myfiles",7);
		else if((strncmp(msg,"logout",6) == 0) || (strncmp(msg,"addfile",7) == 0) || (strncmp(msg,"getfile",7) == 0))
			write(client,"Not logged in", 13);
		else
			write(client,"Unknown command", 15);
	}
	
	deletefromconn(database,user_id);
	sqlite3_close(database);
	close(client);
}
	
void signin(int client,sqlite3 *database)
{
	char username[50],password[50],sql[100];
	bzero(username,50);
	bzero(password,50);
	bzero(sql,100);
	int result_code,r;
	char *pzErrmsg = 0;
	strcpy(sql,"insert into users (username,password) values('");
	if(read(client,username,50) == 0)
		exit(0);
	strcat(sql,username);
	strcat(sql,"','");
	if(read(client,password,50) == 0)
		exit(0);
	strcat(sql,password);
	strcat(sql,"');");
	result_code = sqlite3_exec(database,sql,NULL,NULL,&pzErrmsg);
	if(result_code != SQLITE_OK)
	{
		write(client,"Error occured!",14);
		printf("SQL error: %s", pzErrmsg);
		sqlite3_free_table(pzErrmsg);
		exit(0);
	}
	write(client,"Sign in successful!\n",20);
}

char * login(int client,struct sockaddr_in from, socklen_t len, sqlite3 *database)
{
	char username[50],password[50],sql[100];
	bzero(username,50);
	bzero(password,50);
	bzero(sql,100);
	int nr_rows,nr_columns,r,result_code;
	char *pzErrmsg = 0;
	char **result;
	strcpy(sql,"select user_id from users where username = '");
	if((r = read(client,username,50)) <= 0)
		exit(0);
	strncat(sql,username,r-1);
	strcat(sql,"' and password = '");
	if((r = read(client,password,50)) <= 0)
		exit(0);
	strncat(sql,password,r-1);
	strcat(sql,"';");
	result_code = sqlite3_get_table(database,sql,&result,&nr_rows,&nr_columns,&pzErrmsg);
	if(result_code != SQLITE_OK)
	{
		printf("SQL error: %s", pzErrmsg);
		sqlite3_free_table(pzErrmsg);
		exit(0);
	}
	if(nr_rows == 1)
	{
		write(client,"Accept", 6);
		return result[1];
	}
	write(client,"Denied", 6);	
	return "-1";
}

void logout(sqlite3 *database, int client, char * user_id)
{
	deletefromconn(database,user_id);
	strcpy(user_id,"-1");
	write(client,"Logged out",10);
}

void insertintoconn(int client, struct sockaddr_in from, socklen_t len, sqlite3 *database, char * user_id)
{
	char sql[150],port[10];
	int r, result_code;
	bzero(sql,150);
	strcpy(sql,"insert into connection_info values ('");
	strcat(sql,user_id);
	strcat(sql,"','");
	strcat(sql,inet_ntoa (from.sin_addr));
	strcat(sql,"',");
	if(r = read(client,port,10) <= 0)
		exit(0);
	strncat(sql,port,r-1);
	strcat(sql,");");
	char *pzErrmsg = 0;
	result_code = sqlite3_exec(database,sql,0,0,&pzErrmsg);
	if(result_code != SQLITE_OK)
	{
		printf("SQL error: %s", pzErrmsg);
		sqlite3_free_table(pzErrmsg);
		exit(0);
	}
}

void deletefromconn(sqlite3 *database, char * user_id)
{
	int result_code;
	char *pzErrmsg = 0;
	char sql[150];
	bzero(sql,150);
	strcpy(sql,"delete from connection_info where user_id = ");
	strcat(sql,user_id);
	strcat(sql,";");
	result_code = sqlite3_exec(database,sql,0,0,&pzErrmsg);
	if(result_code != SQLITE_OK)
	{
		printf("SQL error: %s", pzErrmsg);
		sqlite3_free_table(pzErrmsg);
		exit(0);
	}
}

void addfile(int client,sqlite3 *database, char *user_id)
{
	write(client,"addfile",7);
	char sql[400],filename[100],filetype[10],filelength[100];
	char **result;
	int nr_rows,nr_columns;
	bzero(sql,400);
	bzero(filename,100);
	bzero(filetype,10);
	bzero(filelength,100);
	if(read(client,filename,300) <= 0)
	{
		deletefromconn(database,user_id);
		exit(0);
	}
	if(strncmp(filename,"-1",2) == 0)
		return;
	if(read(client,filelength,100) <= 0)
	{
		deletefromconn(database,user_id);
		exit(0);
	}
	int i = strlen(filename)-1;
	int p = -1;
	while(i > 0)
	{
		if(filename[i] == '.')
			break;
		else
			i--;
	}
	strcpy(filetype,filename+i);
	strcpy(sql,"insert into files (filename,filetype,filelength) values('");
	strcat(sql,filename);
	strcat(sql,"','");
	strcat(sql,filetype);
	strcat(sql,"',");
	strcat(sql,filelength);
	strcat(sql,"); SELECT last_insert_rowid();");
	
	char *pzErrmsg = 0;
	int result_code;
	result_code = sqlite3_get_table(database,sql,&result,&nr_rows,&nr_columns,&pzErrmsg);
	if(result_code != SQLITE_OK)
	{
		printf("SQL error: %s", pzErrmsg);
		sqlite3_free_table(pzErrmsg);
		deletefromconn(database,user_id);
		exit(0);
	}
	write(client,result[1],strlen(result[1]));
	bzero(sql,400);
	strcpy(sql,"insert into userfiles values(");
	strcat(sql,user_id);
	strcat(sql,", ");
	strcat(sql,result[1]);
	strcat(sql,");");
	result_code = sqlite3_exec(database,sql,0,0,&pzErrmsg);
	if(result_code != SQLITE_OK)
	{
		printf("SQL error: %s", pzErrmsg);
		sqlite3_free_table(pzErrmsg);
		deletefromconn(database,user_id);
		exit(0);
	}
	sqlite3_free_table(result);
}

void removefile(int client, sqlite3 *database, char *user_id)
{
	char sql[500],file_id[100];
	bzero(file_id,100);
	bzero(sql,500);
	write(client,"removefile",10);
	int result_code,r;
	r = read(client,file_id,100);
	if(r <= 0){
		deletefromconn(database,user_id);
		exit(0);
	}
	strcpy(sql, "delete from userfiles where file_id = ");
	strcat(sql,file_id);
	strcat(sql," and user_id = ");
	strcat(sql,user_id);
	strcat(sql,"; delete from files where file_id = ");
	strcat(sql,file_id);
	strcat(sql," and not exists (select * from userfiles where file_id = ");
	strcat(sql,file_id);
	strcat(sql,");");
	char *pzErrmsg = 0;
	result_code = sqlite3_exec(database,sql,0,0,&pzErrmsg);
	if(result_code != SQLITE_OK)
	{
		printf("SQL error: %s", pzErrmsg);
		sqlite3_free_table(pzErrmsg);
		return;
	}
}

void getfile(int client, sqlite3 *database, char *user_id)
{
	write(client,"getfile",7);
	char sql[300],file_id[100],filepath[150];
	int nr_rows,nr_columns,r,result_code;
	char *pzErrmsg = 0;
	char **result;
	bzero(sql,300);
	bzero(file_id,100);
	bzero(filepath,150);
	if(read(client,file_id,100) <= 0){
		deletefromconn(database,user_id);
		exit(0);
	}
	strcpy(sql,"select filename,filelength,ip,port from connection_info c join userfiles uf on c.user_id = uf.user_id join files f on uf.file_id = f.file_id where f.file_id = ");
	strcat(sql,file_id);
	strcat(sql,";");
	result_code = sqlite3_get_table(database,sql,&result,&nr_rows,&nr_columns,&pzErrmsg);
	if(result_code != SQLITE_OK)
	{
		write(client,"-2",2);
		printf("SQL error: %s", pzErrmsg);
		sqlite3_free_table(pzErrmsg);
		return;
	}
	else if(nr_rows == 0)
		write(client,"-1",2);
	write(client,"ok",2);
	sleep(1);
	write(client,result[nr_columns],strlen(result[nr_columns]));
	sleep(1);
	write(client,result[nr_columns+1],strlen(result[nr_columns+1]));
	sleep(1);
	int i = 1;
	char confirm[3];
	bzero(confirm,3);
	do{
		if(i > nr_rows)
		{
			write(client,"end",3);
			break;
		}
		write(client,result[i*nr_columns+2],strlen(result[i*nr_columns+2]));
		sleep(1);
		write(client,result[i*nr_columns+3],strlen(result[i*nr_columns+3]));
		sleep(1);
		i++;
		if(read(client,confirm,10) == 0){
			deletefromconn(database,user_id);
			exit(0);
		}
	}while(strcmp(confirm,"ok") != 0);
	if(strcmp(confirm,"ok") == 0)
	{
		char sql[200];
		bzero(sql,200);
		strcpy(sql,"insert into userfiles values(");
		strcat(sql,user_id);
		strcat(sql,", ");
		strcat(sql,file_id);
		strcat(sql,");");
		result_code = sqlite3_exec(database,sql,0,0,&pzErrmsg);
		if(result_code != SQLITE_OK)
		{
			printf("SQL error: %s", pzErrmsg);
			sqlite3_free_table(pzErrmsg);
			return;
		}
	}
}

void searchfiles(int client,sqlite3 *database,char *user_id)
{
	write(client,"searchfiles",11);
	char filename[100],filetype[20],input[100],sql[200];
	bzero(filename,100);
	bzero(filetype,20);
	bzero(input,100);
	bzero(sql,200);
	strcpy(sql,"select * from files");
	if(read(client,input,100) <= 0){
		deletefromconn(database,user_id);
		exit(0);
	}
	if(strcmp(input,"filename") == 0 || strcmp(input,"filetype") == 0){
		write(client,"ok",2);
		strcat(sql," where ");
		strcat(sql,input);
		strcat(sql," like '%");
		bzero(input,100);
		if(read(client,input,100) <= 0)
			exit(0);
		strcat(sql,input);
		strcat(sql,"%';");
	}
	else if(strcmp(input,"filelength") == 0){
		write(client,"ok",2);
		strcat(sql," where ");
		strcat(sql,input);
		strcat(sql, " = ");
		bzero(input,100);
		if(read(client,input,100) <= 0){
			deletefromconn(database,user_id);
			exit(0);
		}
		strcat(sql, input);
		strcat(sql, ";");
	}
	else if(strcmp(input,"allfiles") == 0){
		write(client,"ok",2);
		strcat(sql, ";");
	}
	else{
		write(client,"Wrong input",11);
		return;
	}
	int result_code,nr_rows,nr_columns;
	char **result;
	char *pzErrmsg = 0;
	result_code = sqlite3_get_table(database,sql,&result,&nr_rows,&nr_columns,&pzErrmsg);
	if(result_code != SQLITE_OK)
	{
		write(client,"Error from server",17);
		printf("SQL error: %s", pzErrmsg);
		fflush(stdout);
		return;
	} else if(nr_rows == 0){
		write(client,"Couldn't find a file matching the input",39);
		return;
	}
	else
		write(client,"ok",2);
	char c[100],buff[500];
	if(read(client,c,1) <= 0){
		deletefromconn(database,user_id);
		exit(0);
	}
	bzero(c,100);
	int_to_char(c,nr_rows);
	write(client,c,strlen(c));
	if(read(client,c,1) <= 0){
		deletefromconn(database,user_id);
		exit(0);
	}
	int i,j;
	for(i = nr_columns; i < (nr_rows+1) * nr_columns; i+=nr_columns)
	{
		bzero(buff,500);
		strcpy(buff,result[i]);
		strcat(buff," | ");
		strcat(buff,result[i+1]);
		strcat(buff," | ");
		strcat(buff,result[i+2]);
		strcat(buff," | ");
		strcat(buff,result[i+3]);
		int size = strlen(buff);
		bzero(c,100);
		int_to_char(c,size);
		write(client,c,strlen(c));
		write(client," ",1);
		write(client,buff,size);
	}
	sqlite3_free_table(result);
}

void myfilesserver(int client,sqlite3 *database,char *user_id)
{
	write(client,"myfilesserver",13);
	char sql[500];
	bzero(sql,500);
	strcpy(sql,"select * from files natural join userfiles where user_id = ");
	strcat(sql,user_id);
	int result_code,nr_rows,nr_columns;
	char **result;
	char *pzErrmsg = 0;
	result_code = sqlite3_get_table(database,sql,&result,&nr_rows,&nr_columns,&pzErrmsg);
	if(result_code != SQLITE_OK)
	{
		write(client,"Error from server",17);
		printf("SQL error: %s", pzErrmsg);
		fflush(stdout);
		return;
	} else if(nr_rows == 0){
		write(client,"Couldn't find a file matching the input",39);
		return;
	}
	else
		write(client,"ok",2);
	char c[100],buff[500];
	if(read(client,c,1) <= 0){
		deletefromconn(database,user_id);
		exit(0);
	}
	bzero(c,100);
	int_to_char(c,nr_rows);
	write(client,c,strlen(c));
	if(read(client,c,1) <= 0){
		deletefromconn(database,user_id);
		exit(0);
	}
	int i,j;
	for(i = nr_columns; i < (nr_rows+1) * nr_columns; i+=nr_columns)
	{
		bzero(buff,500);
		strcpy(buff,result[i]);
		strcat(buff," | ");
		strcat(buff,result[i+1]);
		strcat(buff," | ");
		strcat(buff,result[i+2]);
		strcat(buff," | ");
		strcat(buff,result[i+3]);
		int size = strlen(buff);
		bzero(c,100);
		int_to_char(c,size);
		write(client,c,strlen(c));
		write(client," ",1);
		write(client,buff,size);
	}
	sqlite3_free_table(result);
}

int initiate_server()
{
	
	char sql[] = "delete from connection_info;";
	sqlite3 *database;
	int result_code;
	result_code = sqlite3_open("server.db",&database);
	if(result_code)
	{
		printf("Can't open database: %s", sqlite3_errmsg(database));
		sqlite3_close(database);
		exit(0);
	}	
	char *pzErrmsg = 0;
	result_code = sqlite3_exec(database,sql,0,0,&pzErrmsg);
	if(result_code != SQLITE_OK)
	{
		printf("SQL error: %s", pzErrmsg);
		sqlite3_free_table(pzErrmsg);
		exit(0);
	}
	sqlite3_close(database);
    struct sockaddr_in server;
    int sd;	
    int optval=1; 
	
    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
		perror ("[server] Eroare la socket().\n");
        return errno;
	}

    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR,&optval,sizeof(optval));

    bzero (&server, sizeof (server));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl (INADDR_ANY);
    server.sin_port = 0;

    if (bind (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
	{
		perror ("[server] Eroare la bind().\n");
        return errno;
	}
	
	socklen_t len = sizeof(server);
	if (getsockname(sd, (struct sockaddr *)&server, &len) == -1) {
	    perror("Error getsockname");
	    exit(errno);
	}
	
	printf("Listening to port: %d\n",ntohs(server.sin_port));
	fflush(stdout);
	
    if (listen (sd, 5) == -1)
    {
		perror ("[server] Eroare la listen().\n");
        return errno;
   	}
	
	return sd;
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
