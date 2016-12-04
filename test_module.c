/* Copyright (C) 2015, Santiago F. Maudet
 * This file is part of char01 module.
 *
 * char01 module is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * char01 module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* !\brief
*   This  is a user application to read data from the module buffer.
*   USAGE: ./test_module /dev/char_01
*/



#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define BUF_SIZE 15

int main (int argc, char ** argv){
  int fd;
  int index = 0, nbytes;
  char data[BUF_SIZE];

  if(argc < 2){
    perror("usage: ./test_module /dev/[device_name]");
    return -1;
  }

  fd = open(argv[1],O_RDONLY);

  if( fd == -1){
    perror("Error openning device");
    return -1;
  }
 do{
  nbytes = read(fd,&data[index],BUF_SIZE);
  index++;
  }while (nbytes != 0);

  data[index + 1] = '\0';
  puts(data);
  return 0;
}
