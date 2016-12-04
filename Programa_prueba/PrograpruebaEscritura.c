
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <fcntl.h>

#define DEVICE "/dev/CHAR_NAME_01"

/*Funcion principal*/
int main(){

	int i,fd;
	char ch,writeBuff[100],readBuff[100];
	int offset = 0;
	fd = open(DEVICE,O_RDWR);
	
	if(fd == -1)
	{
		printf("File %s no existe o se bloqueo \r\n",DEVICE);
		exit(-1);
	
	}
	
	do
	{	
		printf("Ingresa el dato : ");
		scanf(" %[^\n]",writeBuff);
		write(fd,writeBuff,strlen(writeBuff));
		
	}while(1);

	return 0;	
}