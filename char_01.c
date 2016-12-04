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

/* !\brief This is a basic example of a char device.
 *         Basics of LKM are described.
 *         This module has a initialized buffer in kernel memory that you
 *         can read from user space.
 *         Usage:
 *         1) COMPILE = make
 *         2) Insert module into kernel (root): insmod char_01.ko
 *         3) Create /dev/char_01 node: mknod /dev/char_01 c [MAYOR_NUMBER] [MINOR_NUMBER]
 *         4) cat /dev/char_01 or use open and read from a C program.
 *
*/



#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/timer.h>


#include <asm/atomic.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>


#define CHAR_01_MINOR       0           //Start MINOR..
#define CHAR_01_N_DEVS      1           //Number o devices to register.
#define NUM_BYTES_TO_COPY   1           //Number of bytes to copy in copy_to_user call.
#define DRIVER_CLASS         "Char 02"
#define DRIVER_NAME         "CHAR_NAME_01"
#define MAX_NUM_OF_READERS  50


#define BUF_LEN 512
#define BUFFER_SIZE 1024
#define TIMER_TIME 1000 //ms
#define FREQ_DIVISOR_TIME 10
static dev_t device;        //This variable stores the minor and major mumber of the device
struct cdev device_cdev;    //Kernel structure for char devices.
static struct class * MyChar_ClassDevice;
//static int mi_vector[512];
//static char mem_data[] = "Hello World\n";   //Static buffer with information.
//static int nbytes;
ssize_t ret;
static char * driverWriteData = NULL;
static char * driverDataAux = NULL;
static char * driverReadData = NULL;
static size_t len;
static char * newdata;
static int cont=0;
//static char * auxpointer ;
static atomic_t contador = ATOMIC_INIT(0);
static atomic_t readersIn = ATOMIC_INIT(0);
//static int wrcont = 0;
static struct timer_list my_timer;
spinlock_t semaphoreWrite;
spinlock_t reader_lock;
static DEFINE_MUTEX(mutexWrite);
static DEFINE_MUTEX(mutexRead);
static DEFINE_MUTEX(dataAvaible);
static unsigned int writeBufferIndex=0;
//static unsigned int readIndex=0;
static unsigned int readBufferIndex=0;
static unsigned int auxBufferIndex=0;
static unsigned int FreqDivisor=0;


/*Estructura para lectores*/
struct readers
{
	//unsigned int readIndex;
	atomic_t readIndex ;
	unsigned int readerID; 
	spinlock_t readerSpinlock;
	int lock;
	char messageCopyUser[BUF_LEN];
};

static struct readers driverReaders[MAX_NUM_OF_READERS];



/*===============================================================Reader Funtions==============================================================*/

void zero_reader_struct(void)
{
	int i = 0;
	
	for(i=0;i<MAX_NUM_OF_READERS;i++)
	{	
		//driverReaders[i].readIndex =0;
		mb();
		atomic_set(&driverReaders[i].readIndex,0);
	    driverReaders[i].readerID = 0;
		spin_lock_init(&driverReaders[i].readerSpinlock);
		driverReaders[i].lock = 0;
	}
	//char messageCopyUser[BUF_LEN];	

}

void check_block_reader(void)
{
	int i=0;
	
	for(i=0;i<MAX_NUM_OF_READERS;i++)
	{	
		if(driverReaders[i].lock)
		{
			driverReaders[i].lock = 0;
			spin_unlock(&driverReaders[i].readerSpinlock);
		}
			
	}

}

/*==============================================================TIMER FUNTIONS================================================================*/


/*!******************************************
* @brief Funcion Timer callback .
* Se ejecuta cuando el timer acaba de contar
********************************************/
void my_timer_callback( unsigned long irqData )
{
  int ret=0;
  
	//printk( "my_timer_callback called (%ld).\n", jiffies );
  //printk("En el timer callback \n");	//imprimo mensaje


  spin_lock(&semaphoreWrite);
 	 memcpy(driverDataAux,driverWriteData,BUFFER_SIZE);
	 auxBufferIndex=writeBufferIndex;
  spin_unlock(&semaphoreWrite);
 
 //se uso el mismo timer para el buffer auxiliar y el buffer de lectura	
  FreqDivisor++;
  if(FreqDivisor > FREQ_DIVISOR_TIME)
  {
  	spin_lock(&semaphoreWrite);
 	 	memcpy(driverReadData,driverDataAux,BUFFER_SIZE);
	    readBufferIndex=auxBufferIndex;
  	spin_unlock(&semaphoreWrite);
	FreqDivisor=0;
  }

 // printk( "Indice de escritura buff aux: %lld\n", auxBufferIndex ); 
  //printk( "Indice de escritura buff read: %lld\n", readBufferIndex ); 
	
  mod_timer( &my_timer, jiffies + msecs_to_jiffies(TIMER_TIME) ); //Vuelvo a cargar el tiempo
  if (ret) printk("Error in mod_timer\n");
}

/*!*************************************
* @brief Inicializa el modulo del timer
***************************************/
int init_timer_module( void )
{
  int ret;

  printk("Timer module installing\n");

  // my_timer.function, my_timer.driverData
  setup_timer( &my_timer, my_timer_callback, 0 ); // Funcion de inicializacion

  printk( "Starting timer to fire in 200ms (%ld)\n", jiffies ); 
  ret = mod_timer( &my_timer, jiffies + msecs_to_jiffies(TIMER_TIME)); //inicializo el timer con el valor de TIMER_TIME 
  if (ret) printk("Error in mod_timer\n"); // si esta usandose devuelve 1

  return ret;
}
/*!*************************************
* @brief Elimina el timer
***************************************/
int cleanup_timer_module( void )
{
  int ret;

  ret = del_timer( &my_timer );
  if (ret) printk("The timer is still in use...\n"); // Si el timer aun se esta utilizando devuelve 1

  printk("Timer module uninstalling\n");

  return ret;
}


/*==============================================================IMPLEMENTED DEVICE FUNTIONS================================================================*/

/*!**********************************************
* @brief Escribe en el dispositvo 
* @param *file : File descriptor
* @param *buf :  El buffer del usuario
* @param count : El tamano del dato a escribir
************************************************/
ssize_t dev_write(struct file *file, const char __user *buf, size_t count, loff_t *f_pos)
{
    
	ssize_t returnvalue;
	
    printk(KERN_INFO"Escribiendo el dispositivo \n");
	
	if(count > ksize(driverWriteData)) // Si el tamano del dato excede al tamano del kmalloc hecho en open
	{
		 printk(KERN_INFO "El tamamno pasado excede al de kmalloc\n"); //Kernel Info6
         returnvalue = -EFAULT;
         return returnvalue;
	}
	
	mutex_lock(&mutexWrite);
    	returnvalue=strncpy_from_user(driverWriteData + *f_pos,buf,count);
	mutex_unlock(&mutexWrite);

	
	if (returnvalue < 0)
	{	
		printk(KERN_INFO "Fallo\n"); //Kernel Info6
        returnvalue = -EFAULT;
        return returnvalue;
    }
	else
	{
    	returnvalue = count;
		*f_pos += count;
		//check_block_reader();
    }
	writeBufferIndex=*f_pos;
    printk(KERN_INFO"Escritura OK");
	printk(KERN_INFO"Data %s \n",driverWriteData);
	printk(KERN_INFO "Se escribio : %lu bytes y el offset es : %lld\n",count,(*f_pos));
	printk(KERN_INFO "Write index %lld\n",writeBufferIndex);
	//newdriverData=driverData;  //variable auxiliar usado en la funcion read
 
   	
	return returnvalue;
}


/*!**********************************************
* @brief Lee el dispositvo 
* @param *file : File descriptor
* @param *buf :  El buffer del usuario
* @param count : El tamano del dato a leer
************************************************/
ssize_t dev_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) 
{

	
	char *dataRead ;
	int retval;
	unsigned int readIndex;
	int readerId;
	
	
	//spin_lock(&reader_lock);
	readerId = filp->private_data;
	mb();
	readIndex=atomic_read(&driverReaders[readerId].readIndex);
	printk(KERN_INFO"El entre al reader Id es : %ld\n",readerId);	
	printk(KERN_INFO"Reader Index :%ld\n",readIndex);
	//spin_unlock(&reader_lock);
	
	if(count > (ksize(driverReadData)-readIndex))
	{
		 printk(KERN_INFO"El tamano pedido excede al tamano del buffer del driver\n");	// si falla salgo
		return -EINVAL;	
	}
	

	if(readBufferIndex > readIndex)   //significa que el lector tiene datos sin leer
	{   
		//lectura: 			
		printk(KERN_INFO"Leyendo del dispositivo con ID: %ld\n",filp->private_data);
		//printk("Data2 es %s\n",newdata);
		//if (copy_to_user(buf, newdata, count)) //copio al usuario lo que tengo en mi variable local
		mutex_lock(&mutexRead);
		if (copy_to_user(buf, driverReadData+readIndex, count)) //copio al usuario lo que tengo en mi variable local
		{
			 printk(KERN_INFO"Fallo lectura");	// si falla salgo
			return -EINVAL;
		}
		mutex_unlock(&mutexRead);
		*f_pos = readBufferIndex; //actualizo f_pos con el indice de escritura
		mb();
		if((atomic_read(&driverReaders[readerId].readIndex) + count) >= readBufferIndex) // si el indice del lector mas el tamano es mayor al indice de escritura actualizo el buffer con el inidice de escritura
		{
			mb();
			atomic_set(&driverReaders[readerId].readIndex,readBufferIndex);
		}
		else
		{
			mb();
			atomic_set(&driverReaders[readerId].readIndex, readIndex+count);
		}
		printk(KERN_INFO"Lectura OK");
	    printk(KERN_INFO "Se leyo : %lu bytes y el offset es : %lld\n",count,(*f_pos));
	    printk(KERN_INFO "Read index %lld del lector cuyo ID es %lld\n",driverReaders[readerId].readIndex,filp->private_data);
	   // printk(KERN_INFO "Write index %lld\n",readBufferIndex);
	}
	else
	{
		spin_lock(&driverReaders[readerId].readerSpinlock);
		printk(KERN_INFO"Indice de lectura > o igual a indice escritura, Me bloqueo\n");
		printk(KERN_INFO"Indice de lectura %lld\n",readIndex);
		printk(KERN_INFO"Reader ID %lld\n",readerId);
		printk(KERN_INFO"Indice de lectura global %lld\n",readBufferIndex);
		//driverReaders[readerId].lock = 1;
		//while(readBufferIndex <= readIndex);
		spin_unlock(&driverReaders[readerId].readerSpinlock);
// 		spin_unlock(&semaphoreWrite);
		//goto lectura;
		return -EINVAL;
	}
	
	return count;
	
}

/*!**********************************************
* @brief Cierra el dispositivo
************************************************/
int char01_close(struct inode *inode, struct file *flip){
	
	printk(KERN_INFO "Pase por el close\n"); //Kernel Info
	
	mb();
	atomic_dec(&contador);
	
	mb();
	if(!atomic_read(&contador)) // por ahora el read no libera el espacio en memoria
	{
	
		while(cleanup_timer_module());
		kfree(driverWriteData);
		kfree(driverDataAux);
		kfree(driverReadData);
		printk(KERN_INFO "Se cerro el dispositivo correctamente\n");
	}

    return 0;
}

/*!**********************************************
* @brief Abre el dispositivo
************************************************/
int dev_open(struct inode *inode, struct file *filp)
{
	
	
	ssize_t retval;
	int readerId=0;
	spin_lock_init(&semaphoreWrite);
	spin_lock_init(&reader_lock);
	printk(KERN_INFO "open device\n"); //Kernel Info
	
	if(!atomic_read(&contador))
	{
		//Aloco memoria global al driver
		driverWriteData = kzalloc(sizeof(char)*BUFFER_SIZE,GFP_KERNEL);
		driverDataAux = kzalloc(sizeof(char)*BUFFER_SIZE,GFP_KERNEL);
		driverReadData = kzalloc(sizeof(char)*BUFFER_SIZE,GFP_KERNEL);
       //Valido que se pudo alocar si no salgo con error
		if (!driverWriteData && !driverDataAux && !driverReadData)  
		{
			printk(KERN_INFO "Kmalloc FAILED\n"); //Kernel Info
			retval = -ENOMEM;
			return retval;
		}
		printk(KERN_INFO "Data Write OK se aloco %zu bytes de memoria\n",ksize(driverWriteData)); //Kernel Info6
		printk(KERN_INFO "Data aux OK se aloco %zu bytes de memoria\n",ksize(driverDataAux)); //Kernel Info6		
		init_timer_module();
		zero_reader_struct();		
	}
	else
	{

		mb() ;
		readerId=atomic_read(&readersIn);
		mb() ;
		driverReaders[readerId-1].readerID = readerId;	
		filp->private_data = readerId;
		//driverReaders[readerId-1].readIndex = ATOMIC_INIT(0);
		printk("Se conecto un lector , su ID es: %lld , el private data del filp es: %d \n",driverReaders[atomic_read(&readersIn)-1].readerID,filp->private_data);
		if(readerId != 0)
		{
			printk("Datos del lector anterior,ID es: %lld , el private data del filp es: %d \n",driverReaders[atomic_read(&readersIn)-2].readerID,driverReaders[atomic_read(&readersIn)-2].readIndex);
		}
	}
    atomic_inc(&contador);   
	atomic_inc(&readersIn);   
	return 0;

}



/*==============================================================INIT/EXIT FUNTIONS================================================================*/

/*!**********************************************
* @brief EStructura file operation, 
* indica que funcion se ejecutara 
************************************************/
struct file_operations dev_fops = { //Struct File Operations, this module only supports read...
	.open = dev_open,
	//.open = test_open,
    .owner = THIS_MODULE,           // Tells who is owner of struct file_operations
   // .read = dev_Hello_Read,               // Function pointer init with dev_read function.
	.read= dev_read,
    .write= dev_write,
	//.write= memo_write,
    .release = char01_close,
	//.release = test_close,
};


/*!**********************************************
* @brief Inicializa el dispositivo 
************************************************/
static int __init dev_init(void)
{
    int result = 0;  // Aux Variable.

    printk(KERN_INFO "Loading module char_01\n"); //Kernel Info

    // Dynamic Allocation of MAJOR Number for char_01 Module
    result = alloc_chrdev_region(&device, CHAR_01_MINOR, CHAR_01_N_DEVS, DRIVER_CLASS);

    //Can't get MAJOR Number, return...
    if(result < 0){
      printk(KERN_WARNING "char_01: can't get major\n");
      return result;
    }

    MyChar_ClassDevice = class_create(THIS_MODULE, DRIVER_CLASS);
    //result_device = device_create (MyChar_ClassDevice, NULL, device, NULL, DRIVER_NAME); //si esta mal devuelve null .... IS_ERR(funcion)
	device_create (MyChar_ClassDevice, NULL, device, NULL, DRIVER_NAME);
//	pr_err("%s:%d error code %d\n", __func__, __LINE__, PTR_ERR(result_device->dev));
    // Initialize struct cdev "device_cdev" with struct file_operations "dev_fops"
    cdev_init (&device_cdev, &dev_fops);
    // Add device to kernel.
    result = cdev_add (&device_cdev, device, CHAR_01_N_DEVS);

    if(result < 0){
      printk(KERN_WARNING "char_01 can't be registered in kenel\n");
      return result;
    }
    printk(KERN_INFO "Correct Registration of device char_01...\n");

    return 0;
}

/*!**********************************************
* @brief Cierra el dispositivo 
************************************************/
static void __exit dev_exit(void)
{
	cont=0;
    printk(KERN_INFO "Unloading Module CHAR_01\n");
    cdev_del(&device_cdev); //Remove device form kernel.
	device_destroy(MyChar_ClassDevice,device);
	class_destroy(MyChar_ClassDevice);
	unregister_chrdev_region(device,CHAR_01_N_DEVS);
	
}

module_init(dev_init); //Init Macro loaded with init function.
module_exit(dev_exit); //Exit Macro loaded with exit function.

MODULE_AUTHOR("Ivan Andres Leon, UBA @ UBA http://www.fiuba.edu.ar");
MODULE_LICENSE("GPL");
