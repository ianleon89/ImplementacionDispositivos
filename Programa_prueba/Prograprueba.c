

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

/********************************************************************************MAIN********************************************************************************************/
/*Funcion principal*/
int main(){

	int i,fd;
	char ch,writeBuff[100],readBuff[100];

	fd = open(DEVICE,O_RDWR);
	
	if(fd == -1)
	{
		printf("File %s no existe o se bloqueo \r\n",DEVICE);
		exit(-1);
	
	}
	
	printf("Presiona r para leer desde el dispositivo %s \n",DEVICE);
	printf("Presiona w para escribir al dispositivo %s \n",DEVICE);
	scanf("%c",&ch);
	
	switch(ch)
	{
	
		case 'r':
			read(fd,readBuff,sizeof(readBuff));
			printf("Lectura desde el dispositivo : %s\n",readBuff);
			bzero(readBuff,sizeof(readBuff));
		break;
			
		case 'w' :
			printf("Ingresa el dato : ");
			scanf(" %[^\n]",writeBuff);
			write(fd,writeBuff,sizeof(writeBuff));
		break;
			
		default :
		 printf("Comando erroneo \r\n");	
	    break;
	
	}
  return 0;
}

