// ConsoleApplication1.cpp : Defines the entry point for the console application.
//


extern "C" {
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
}
#include "stdafx.h"
#include "NIDAQmx.h"
#include<conio.h>
#include<stdlib.h>
#include "interface.h"
#include "ConsoleApplication1.h"
#include <iostream>

#define KEY_UP 72
#define KEY_DOWN 80
#define KEY_LEFT 75
#define KEY_RIGHT 77
//structure
typedef struct {
	int x;
	int z;
} TPosition;

//mailboxes
xQueueHandle        mbx_x;  //for goto_x
xQueueHandle        mbx_z;  //for goto_z
xQueueHandle        mbx_y;  //for goto_y
xQueueHandle        mbx_xz;
xQueueHandle        mbx_req;
xQueueHandle  sem_x_mov;
xQueueHandle  sem_z_mov;

//semahores
xSemaphoreHandle   semkit; //exclusive access to the DAQ/Kit
xSemaphoreHandle   _CRITICAL_port_access_;
xSemaphoreHandle  sem_x_done;
xSemaphoreHandle  sem_z_done;
xSemaphoreHandle  sem_y_done;

/*
*				FUNTIONS START
*
*
*/

bool getBitValue(uInt8 value, uInt8 n_bit)
// given a byte value, returns the value of bit n
{
	return(value & (1 << n_bit)) >> n_bit;
}

void setBitValue(uInt8  &variable, int n_bit, bool new_value_bit)
// given a byte value, set the n bit to value
{
	uInt8  mask_on = (uInt8)(1 << n_bit);
	uInt8  mask_off = ~mask_on;
	if (new_value_bit)  variable |= mask_on;
	else         variable &= mask_off;
}
uInt8 safe_ReadDigitalU8(int porto)
{
	uInt8 aa = 0;
	xSemaphoreTake(semkit, portMAX_DELAY);
	aa = ReadDigitalU8(porto);
	xSemaphoreGive(semkit);
	return(aa);
}
void safe_WriteDigitalU8 (int porto, uInt8 value)
{
	uInt8 aa = 0;
	xSemaphoreTake(semkit, portMAX_DELAY);
	WriteDigitalU8(porto, value);
	xSemaphoreGive(semkit);
}




/**
 *	FUNCOES X
 *
 */
void move_x_right()
{
	xSemaphoreTake(_CRITICAL_port_access_, portMAX_DELAY);
	uInt8 vp2 = safe_ReadDigitalU8(2);
	setBitValue(vp2, 6, 0);
	setBitValue(vp2, 7, 1);
	safe_WriteDigitalU8(2, vp2);
	xSemaphoreGive(_CRITICAL_port_access_, portMAX_DELAY);
}
void move_x_left(){
	xSemaphoreTake(_CRITICAL_port_access_, portMAX_DELAY);
	uInt8 vp2 = safe_ReadDigitalU8(2);
	setBitValue(vp2, 7, 0);
	setBitValue(vp2, 6, 1);
	safe_WriteDigitalU8(2, vp2);
	xSemaphoreGive(_CRITICAL_port_access_, portMAX_DELAY);
}
void stop_x()
{
	xSemaphoreTake(_CRITICAL_port_access_, portMAX_DELAY);
	uInt8 vp2 = safe_ReadDigitalU8(2);
	setBitValue(vp2, 7, 0);
	setBitValue(vp2, 6, 0);
	safe_WriteDigitalU8(2, vp2);
	xSemaphoreGive(_CRITICAL_port_access_, portMAX_DELAY);
}
bool is_moving_x_left()
{
	int vp2 = safe_ReadDigitalU8(2);
	return getBitValue(vp2, 6);
}
bool is_moving_x_right()
{
	int vp2 = safe_ReadDigitalU8(2);
	return getBitValue(vp2, 7);
}
bool is_at_x(int Pos)
{
	int vp0 = safe_ReadDigitalU8(0);
	if (Pos == 1 && !getBitValue(vp0, 2))
		return(true);
	if (Pos == 2 && !getBitValue(vp0, 1))
		return(true);
	if (Pos == 3 && !getBitValue(vp0, 0))
		return(true);
	return(false);
}
int actual_x()
{
	int vp0 = safe_ReadDigitalU8(0);
	if (!getBitValue(vp0, 2))
		return(1);
	if (!getBitValue(vp0, 1))
		return(2);
	if (!getBitValue(vp0, 0))
		return(3);
	return(-1);
}
bool isAtx(int x) {
	return actual_x() == x;
}
void goto_x(int x_dest) {
	if (actual_x() != -1) {  // is it at valid position?
		if (actual_x() < x_dest) {
			move_x_right();
		}
		else if (actual_x() > x_dest) {
			move_x_left();
		}
		//while position not reached             
		while (actual_x() != x_dest) {
			_sleep(10);
		}
		stop_x();   // arrived.
	}
}/*
 *			FUNTIONS Z
 *
 */

bool IsAtUp() {
	uInt8 vp0 = safe_ReadDigitalU8(0);//ver se tem algum bit de cima activado
	uInt8 vp1 = safe_ReadDigitalU8(1);
	return !getBitValue(vp0, 6) || !getBitValue(vp1, 0) || !getBitValue(vp1, 2);
}
int actual_z()
{	//uses the sensor below each cell only, not the upper one
	int vp1 = safe_ReadDigitalU8(1);
	int vp0 = safe_ReadDigitalU8(0);
	if (!getBitValue(vp1, 3))  		return(1);
	if (!getBitValue(vp1, 1))  		return(2);
	if (!getBitValue(vp0, 7))  		return(3);
	return(-1);
}
int actual_zTop()
{	//uses the sensor below each cell only, not the upper one
	int vp1 = safe_ReadDigitalU8(1);
	int vp0 = safe_ReadDigitalU8(0);
	if (!getBitValue(vp1, 2))  		return(1);
	if (!getBitValue(vp1, 0))  		return(2);
	if (!getBitValue(vp0, 6))  		return(3);
	return(-1);
}

bool IsAtDown() {
	return actual_z()!=-1 && actual_z()!=0;
}
bool isAtz(int z) {
	return actual_z() == z;
}
	
void move_z_up() {
	xSemaphoreTake(_CRITICAL_port_access_, portMAX_DELAY);
	uInt8 vp2 = safe_ReadDigitalU8(2);
	setBitValue(vp2, 3, 1);
	setBitValue(vp2, 2, 0);
	safe_WriteDigitalU8(2, vp2);
	xSemaphoreGive(_CRITICAL_port_access_, portMAX_DELAY);
}void move_z_down() {
	xSemaphoreTake(_CRITICAL_port_access_, portMAX_DELAY);
	uInt8 vp2 = safe_ReadDigitalU8(2);
	setBitValue(vp2, 3, 0);
	setBitValue(vp2, 2, 1);
	safe_WriteDigitalU8(2, vp2);
	xSemaphoreGive(_CRITICAL_port_access_, portMAX_DELAY);
}
void stop_z() {
	xSemaphoreTake(_CRITICAL_port_access_, portMAX_DELAY);
	uInt8 vp2 = safe_ReadDigitalU8(2);
	setBitValue(vp2, 3, 0);
	setBitValue(vp2, 2, 0);
	safe_WriteDigitalU8(2, vp2);
	xSemaphoreGive(_CRITICAL_port_access_, portMAX_DELAY);
}


void goto_z(int z_dest) {
	if (actual_z() != -1) {  // is it at valid position?
		if (actual_z() < z_dest) {
			move_z_up();
		}
		else if (actual_z() > z_dest) {
			move_z_down();
		}
		//while position not reached             
		while (actual_z() != z_dest) {
			_sleep(10);
		}
		stop_z();   // arrived.
	}
	else if (actual_zTop()) {
		if (actual_zTop() < z_dest) {
			move_z_up();
		}
		else if (actual_zTop() > z_dest || actual_z() == actual_zTop()) {
			move_z_down();
		}
		
		//while position not reached             
		while (actual_z() != z_dest) {
			_sleep(10);
		}
		stop_z();   // arrived.
	}
}
void goto_z_up()
{
	//if (is_at_down_level(1) || is_at_down_level(2) || is_at_down_level(3))
	if (IsAtDown()) {
		move_z_up();
		while (!IsAtUp())
			_sleep(10);//		vTaskDelay(10);
		stop_z();
	}
}
void goto_z_dn()
{
	if (IsAtUp()) {
		move_z_down();
		while (!IsAtDown())
			_sleep(10);//		vTaskDelay(10);
		stop_z();
	}
/*move_z_down();
	while (!is_at_z_dn(1))  // …. similar to goto_z_up  )
		_sleep(10);//		vTaskDelay(10);
	stop_z();
	*/
}
bool is_at_z_up()
{
	int vp0 = safe_ReadDigitalU8(0);
	int vp1 = safe_ReadDigitalU8(1);
	if (!getBitValue(vp0, 6) || !getBitValue(vp1, 0) || !getBitValue(vp1, 2))
		return true;
	return false;
}
/*
*				Funtions Y
*
*/
void move_y_inside() {
	xSemaphoreTake(_CRITICAL_port_access_, portMAX_DELAY);
	uInt8 vp2 = safe_ReadDigitalU8(2);
	setBitValue(vp2, 5, 1);
	setBitValue(vp2, 4, 0);
	safe_WriteDigitalU8(2, vp2);
	xSemaphoreGive(_CRITICAL_port_access_, portMAX_DELAY);
}
void move_y_outside() {
	xSemaphoreTake(_CRITICAL_port_access_, portMAX_DELAY);
	uInt8 vp2 = safe_ReadDigitalU8(2);
	setBitValue(vp2, 5, 0);
	setBitValue(vp2, 4, 1);
	safe_WriteDigitalU8(2, vp2);
	xSemaphoreGive(_CRITICAL_port_access_, portMAX_DELAY);
}
void stop_y(){
	xSemaphoreTake(_CRITICAL_port_access_, portMAX_DELAY);
	uInt8 vp2 = safe_ReadDigitalU8(2);
	setBitValue(vp2, 5, 0);
	setBitValue(vp2, 4, 0);
	safe_WriteDigitalU8(2, vp2);
	xSemaphoreGive(_CRITICAL_port_access_, portMAX_DELAY);
}
int actual_y() {//3 for outside, 1 for inside
	int vp0 = safe_ReadDigitalU8(0);
	if (!getBitValue(vp0, 3))  		return(1);
	if (!getBitValue(vp0, 4))  		return(2);
	if (!getBitValue(vp0, 5))  		return(3);
	return(-1);
}
bool isOutside() {
	return actual_y() == 3;
}
bool isMiddle() {
	return actual_y() == 2;
}
bool isAty(int y) {
	return actual_y() == y;
}
void goto_y(int y_dest) {
	if (actual_y() != -1) {  // is it at valid position?
		if (actual_y() < y_dest) {
			move_y_outside();
		}
		else if (actual_y() > y_dest) {
			move_y_inside();
		}
		//while position not reached             
		while (actual_y() != y_dest) {
			_sleep(10);
		}
		stop_y();   // arrived.
	}
}
/*
*
*						Combined Functions
*
*
*/

bool put_piece() {
	if (IsAtDown && isMiddle()/*&& cage is free*/) {
		goto_z_up();
		goto_y(1);
		goto_z_dn();
		goto_y(2);
		//Give Info Pakage is There
		return true;
	}
	return false;
}
bool get_piece() {
	if (IsAtDown && isMiddle()&& actual_x()!=-1/*&& cage is free*/) {
		goto_y(1);
		goto_z_up();
		goto_y(2);
		goto_z_dn();
		//Give Info Pakage is taken and in transit
		return true;//teste
	}
	return false;
}
void goto_xz(int x, int z) {
	goto_x(x);
	goto_z(z);
}


bool isAtCell(int x,int z) {
	return actual_x() == x && actual_z() == z;
}
void goto_xz_task(int x, int z, bool _wait_done = false)
{
	TPosition pos;
	pos.x = x;
	pos.z = z;
	//xQueueSend(mbx_xz, &pos, portMAX_DELAY);
	xQueueSend(mbx_x, &pos.x, portMAX_DELAY);
	xQueueSend(mbx_z, &pos.z, portMAX_DELAY);
	if (_wait_done) {
		do { vTaskDelay(1); } while (actual_x() != x);
		do { vTaskDelay(1); } while (actual_z() != z);

		uInt8  p;
		do {
			vTaskDelay(1);
			p = safe_ReadDigitalU8(2);
			// wait while still running
		} while (getBitValue(p, 2) || getBitValue(p, 3) || getBitValue(p, 7) || getBitValue(p, 6));
	}
}
/*
*
*						 Tasks
*
*/
void show_menu()
{
	printf("\n gxz...... goto(x,z)");
	printf("\n pp....... Put a part in a specific x, z position");
	printf("\n rp....... Retrieve a part from a specific x, z position");
	printf("\n ......... more options you like...");
	printf("\n ppc...... Put product ref in the cell (reference, date, x, z)");
	printf("\n ppfc..... Put product ref in a free cell (reference, date)");
	printf("\n rpc...... Retrieve a product from cell(x, z)");
	printf("\n ......... more options you like...");
	printf("\n end...... Terminate application...");
}

void task_storage_services(void *)
{
	char cmd[20];
	// I recommend using the keyboard only in this task
	// that is, do not share keyboard in several tasks
	// otherwise you must code specific synchronization 
	// mechanism   which allows a task owning the 
	// keyboard  until not necessary
	while (true) {
		// show the menu(1-> put, 2->get, 3…)
		show_menu();
		// get selected option from keyboard
		printf("\n\nEnter option=");
		fgets(cmd,20,stdin);
		for (int i = 0; i < 20; i++) {
			if (cmd[i] == 0) {
				cmd[i - 1] = 0;
				break;
			}
		}
		// execute selected option 
		// you can use number and "switch case break" 
		//approach
		if (stricmp(cmd, "mxl") == 0) // hidden option
			move_x_left(); // not in the show_menu  
		if (stricmp(cmd, "mxr") == 0)  // hidden option
			move_x_right();  // not in the show_menu  

		if (stricmp(cmd, "sx") == 0)
			stop_x();
		if (stricmp(cmd, "gxz") == 0)
		{
			char str_x[20], str_z[20];
			int x, z; // you can use scanf or one else you like
			printf("\nX="); fgets(str_x,20,stdin); x = atoi(str_x);
			printf("\nZ="); fgets(str_z,20,stdin); z = atoi(str_z);
			if (x >= 1 && x <= 3 && z >= 1 && z <= 3)
				goto_xz_task(x, z,true);  //try many goto_xz fast
			else
				printf("\nWrong (x,z) coordinates, are you sleeping?... ");
		}
		if (stricmp(cmd, "end") == 0)
		{
			safe_WriteDigitalU8(2, 0); //stop all motores;
			vTaskEndScheduler(); // terminates application
		}
	}
}

void goto_x_task(void *)
{
	while (true)
	{
		int x;
		xQueueReceive(mbx_x, &x, portMAX_DELAY);
		goto_x(x);
		xSemaphoreGive(sem_x_done);
	}
}
void goto_z_task(void *)
{
	while (true)
	{
		int z;
		xQueueReceive(mbx_z, &z, portMAX_DELAY);

		printf("\nentra goto_z_task");
		goto_z(z);
		xSemaphoreGive(sem_z_done);
	}
}
void goto_y_task(void *)
{
	while (true)
	{
		int y;
		xQueueReceive(mbx_y, &y, portMAX_DELAY);
		goto_z(y);
		xSemaphoreGive(sem_y_done);
	}
}
void keyChecker(void *) {
	bool left = false, right = false, up = false, down = false;
	while (1) {
		if (GetKeyState(VK_LEFT)& -128 && actual_x() != 1) {
			if (right) {
				right = false;
			}
			if (!left) {
				int i = -1;
				xQueueSend(sem_x_mov, &i, 0);
			}
			left = true;
		}
		else
		{
			if (GetKeyState(VK_RIGHT)& -128 && actual_x() != 3) {
				if (left) {
					left = false;
				}
				if (!right) {
					int i = 1;
					xQueueSend(sem_x_mov, &i, 0);
				}
				right = true;
			}
			else {
				if (right || left) {
					int i = 6;
					xQueueSend(sem_x_mov, &i, 0);
				}
				right = false;
				left = false;

			}
		}
		if (GetKeyState(VK_UP)& -128 && actual_zTop() != 3) {
			if (down)
				down = false;
			if(!up){
				int i = 2; 
				xQueueSend(sem_x_mov, &i, 0);
			}
			up = true;

		}
		else {
			if (GetKeyState(VK_DOWN)& -128 && actual_z() != 1) {
				if (up)
					up = false;
				if (!down) {
					int i = -2; 
					xQueueSend(sem_x_mov, &i, 0);
				}
				down = true;
			}
			else {
				if(up||down){
					int i = 5;
					xQueueSend(sem_x_mov, &i, 0);
				}
				up = false;
				down = false;

			}
		}

	}

}

void motor_works(void *) {
	while (1) {
		int i = 0;
		xQueueReceive(sem_x_mov, &i, portMAX_DELAY);
		switch (i) {
		case 0:	if (xSemaphoreTake(sem_x_done, portMAX_DELAY)&& xSemaphoreTake(sem_z_done, portMAX_DELAY)) {
			stop_x();
			stop_z();
			xSemaphoreGive(sem_x_done);
			xSemaphoreGive(sem_z_done);
		}
		case 1:	if (xSemaphoreTake(sem_x_done, portMAX_DELAY)) {
			printf("is gonna move\n");
			move_x_right();
			xSemaphoreGive(sem_x_done);
		}break;
		case -1:	if (xSemaphoreTake(sem_x_done, portMAX_DELAY)) {
			printf("is gonna move");
			move_x_left();
			xSemaphoreGive(sem_x_done);
		}break;
		case 2:	if (xSemaphoreTake(sem_z_done, portMAX_DELAY)) {
			printf("is gonna move");
			move_z_up();
			xSemaphoreGive(sem_z_done);
		}break;
		case -2:	if (xSemaphoreTake(sem_z_done, portMAX_DELAY)) {
			printf("is gonna move");
			move_z_down();
			xSemaphoreGive(sem_z_done);
		}break;
		case 5:	if (xSemaphoreTake(sem_z_done, portMAX_DELAY)) {
			printf("stoping");
			stop_z();
			xSemaphoreGive(sem_z_done);
		}break;
		case 6:if (xSemaphoreTake(sem_x_done, portMAX_DELAY)) {
			printf("stoping");
			stop_x();
			xSemaphoreGive(sem_x_done);
		}break;
		}
				
		
	}
}

void keyControler_x(void *) {
	while (1) {

		if (GetKeyState(VK_LEFT)& -128 && actual_x() != 1) {
			xSemaphoreTake(sem_x_done,portMAX_DELAY);
			move_x_left();
			while (GetKeyState(VK_LEFT)& -128 && actual_x() != 1)
				vTaskDelay(1);
			stop_x();
			xSemaphoreGive(sem_x_done);
		}
		if (GetKeyState(VK_RIGHT)& -128 && actual_x() != 3) {
			xSemaphoreTake(sem_x_done, portMAX_DELAY);
			move_x_right();
			while (GetKeyState(VK_RIGHT)& -128 && actual_x() != 3) {
				vTaskDelay(1);
			}
			stop_x();
			xSemaphoreGive(sem_x_done);
		}
	}
}
void keyControler_z(void*){
	while(1){
		if (GetKeyState(VK_UP)& -128 && actual_zTop() != 3) {
			xSemaphoreTake(sem_z_done, portMAX_DELAY);
			move_z_up();
			while (GetKeyState(VK_UP)& -128 && actual_zTop() != 3) {
				vTaskDelay(1);
			}
			stop_z();
			xSemaphoreGive(sem_x_done);
		}
		if (GetKeyState(VK_DOWN)& -128 && actual_z() != 1) {
			xSemaphoreTake(sem_z_done, portMAX_DELAY);
			move_z_down();
			while (GetKeyState(VK_DOWN)& -128 && actual_z() != 1) {
				vTaskDelay(1);
			}
			stop_z();
			xSemaphoreGive(sem_z_done);
		}
		
	}
}

void testes(void *) {
	while (1) {
			printf("estas clicando%d\n\n\n", GetKeyState(VK_LEFT)& -128);
	}
}



/********************************************************************************
			MAIN
*/
void main(void) {
	semkit = xSemaphoreCreateCounting(10, 1);   //SEMAPHORE CREATION
	sem_x_done = xSemaphoreCreateCounting(10, 1); // synchronization semaphore
	sem_z_done = xSemaphoreCreateCounting(10, 1);  // synchronization semaphore
	_CRITICAL_port_access_ = xSemaphoreCreateCounting(10, 1); // exclusive access/resource semaphore 


															  // maximum number of messages, size of each message
	sem_z_mov = xQueueCreate(10, sizeof(int));
	sem_x_mov = xQueueCreate(10, sizeof(int));
	mbx_x = xQueueCreate(10, sizeof(int));
	mbx_z = xQueueCreate(10, sizeof(int));
	mbx_xz = xQueueCreate(10, sizeof(TPosition));
	//mbx_req = xQueueCreate(10, sizeof(TRequest));
	//xTaskCreate(goto_x_task, "v_gotox_task", 100, NULL, 0, NULL);
	//xTaskCreate(goto_z_task, "v_gotoz_task", 100, NULL, 0, NULL);
	//xTaskCreate(task_storage_services, "task_storage_services", 100, NULL, 0, NULL);
	//xTaskCreate(testes, "t", 100, NULL, 0, NULL);
	xTaskCreate(keyChecker, "key_Checker", 100, NULL, 0, NULL);
	xTaskCreate(motor_works, "motors", 100, NULL, 0, NULL);
	vTaskStartScheduler();
	
	/*
	create_DI_channel(0);
	create_DI_channel(1);
	create_DO_channel(2);  // channel 2 is an output port

						   // second argument is the one that really matters
	semkit = xSemaphoreCreateCounting(10, 1);   //SEMAPHORE CREATION
	sem_x_done = xSemaphoreCreateCounting(10, 0); // synchronization semaphore
	sem_z_done = xSemaphoreCreateCounting(10, 0);  // synchronization semaphore
	_CRITICAL_port_access_ = xSemaphoreCreateCounting(10, 1); // exclusive access/resource semaphore 

															  // maximum number of messages, size of each message
	mbx_x = xQueueCreate(10, sizeof(int));
	mbx_z = xQueueCreate(10, sizeof(int));
	mbx_xz = xQueueCreate(10, sizeof(TPosition));
	//mbx_req = xQueueCreate(10, sizeof(TRequest));

	xTaskCreate(goto_x_task, "v_gotox_task", 100, NULL, 0, NULL);
	xTaskCreate(goto_z_task, "v_gotoz_task", 100, NULL, 0, NULL);
	xTaskCreate(goto_xz_task, "v_gotoxz_task", 100, NULL, 0, NULL);
	//xTaskCreate(task_storage_services, "task_storage_services", 100, NULL, 0, NULL);

	//callibration();  // this routine should be called befor starting the scheduler
	vTaskStartScheduler();







	*******************************************************************************
	/*
	create_DI_channel(0);
	create_DI_channel(1);
	create_DO_channel(2);
	semkit = xSemaphoreCreateCounting(1, 1);   //SEMAPHORE CREATION
	xTaskCreate(vTaskHorizontal, "TaskHoriz", 100, NULL, 0, NULL);
	xTaskCreate(vTaskVertical, "TaskVertcal", 100, NULL, 0, NULL);

	vTaskStartScheduler();

	safe_WriteDigitalU8(2, 0);  // all bits of port 2 set to z => stop all motors
	close_channels();

	//	xTaskCreate(vTaskCode_2, "vTaskCode_1", 100, NULL, 0, NULL);
	//	xTaskCreate(vTaskCode_1, "vTaskCode_2", 100, NULL, 0, NULL);
	//	vTaskStartScheduler();
	


	/*
	IsAtDown();
	IsAtUp();
	getchar();
	printf("%d",actual_z());
	getchar();
	printf("%d", actual_z());
	getchar();	getchar();
	printf("%d", actual_z());
	getchar();
	printf("%d", actual_z());
	getchar();	getchar();
	printf("%d", actual_z());
	getchar();
	printf("%d", actual_z());
	getchar();	getchar();
	printf("%d", actual_z());
	getchar();
	printf("%d", actual_z());
	getchar();

	
	printf("\n%d", actual_y());
	getchar();
	printf("\n%d", actual_y());
	getchar();
	printf("\n%d", actual_y());
	getchar();
	printf("\n%d", actual_y());
	getchar();
	printf("\n%d", actual_y());
	getchar();
	/*
	printf("%d\n", actual_x());
	getchar();
	printf("%d\n", actual_x());
	getchar();
	printf("%d\n", actual_x());
	getchar();
	printf("%d\n", actual_x());
	getchar();
	*/
/*	
	goto_x(1);
	getchar();
	*/
	}
