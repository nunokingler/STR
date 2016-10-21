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

#define x_max 3
#define z_max 3
//structure
typedef struct {
	int x;
	int z;
} TPosition;

typedef struct {
	int time;
	char  id[100];
}TRequest;
typedef struct {
	TPosition pos;
	TRequest req;
	bool put_take;
}Task;
typedef struct {
	int i;
	bool wait;
}mov;
typedef struct {
	TPosition pos;
	bool result;
}Result;

bool cells[x_max][z_max];
TRequest cells_p[x_max][z_max];
//mailboxes
xQueueHandle        mbx_xx;  //for goto_x
xQueueHandle        mbx_z;  //for goto_z
xQueueHandle        mbx_y;  //for goto_y
xQueueHandle		mbx_y_carefull;
xQueueHandle        mbx_xz;
xQueueHandle        mbx_req;
xQueueHandle		 sem_x_mov;
xQueueHandle		sem_being_used;
xQueueHandle		mbx_pieces;
xQueueHandle		mbx_pieces_return;

//semahores
xSemaphoreHandle   semkit; //exclusive access to the DAQ/Kit
xSemaphoreHandle   _CRITICAL_port_access_;
xSemaphoreHandle  sem_x_done;
xSemaphoreHandle  sem_z_done;
xSemaphoreHandle  sem_y_done;
//void goto_xz_task(int x, int z, bool _wait_done = false);

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

void Turn_on_light_1() {
	xSemaphoreTake(_CRITICAL_port_access_, portMAX_DELAY);
	uInt8 vp2 = safe_ReadDigitalU8(2);
	setBitValue(vp2, 0, 1);
	safe_WriteDigitalU8(2, vp2);
	xSemaphoreGive(_CRITICAL_port_access_);
}
void Turn_on_light_2() {
	xSemaphoreTake(_CRITICAL_port_access_, portMAX_DELAY);
	uInt8 vp2 = safe_ReadDigitalU8(2);
	setBitValue(vp2, 1, 1);
	safe_WriteDigitalU8(2, vp2);
	xSemaphoreGive(_CRITICAL_port_access_);
}
void insert_piece() {
	xSemaphoreTake(_CRITICAL_port_access_, portMAX_DELAY);
	uInt8 vp1 = safe_ReadDigitalU8(1);
	setBitValue(vp1, 4, 1);
	safe_WriteDigitalU8(1, vp1);
	xSemaphoreGive(_CRITICAL_port_access_);
}
void Turn_off_light_1() {
	xSemaphoreTake(_CRITICAL_port_access_, portMAX_DELAY);
	uInt8 vp2 = safe_ReadDigitalU8(2);
	setBitValue(vp2, 0, 0);
	safe_WriteDigitalU8(2, vp2);
	xSemaphoreGive(_CRITICAL_port_access_);
}
void Turn_off_light_2() {
	xSemaphoreTake(_CRITICAL_port_access_, portMAX_DELAY);
	uInt8 vp2 = safe_ReadDigitalU8(2);
	setBitValue(vp2, 1, 0);
	safe_WriteDigitalU8(2, vp2);
	xSemaphoreGive(_CRITICAL_port_access_);
}
void remove_piece() {
	xSemaphoreTake(_CRITICAL_port_access_, portMAX_DELAY);
	uInt8 vp1 = safe_ReadDigitalU8(1);
	setBitValue(vp1, 4, 0);
	safe_WriteDigitalU8(1, vp1);
	xSemaphoreGive(_CRITICAL_port_access_);
}
bool has_piece() {
	uInt8 vp1 = safe_ReadDigitalU8(1);
	return getBitValue(vp1, 4);
}

bool Expired() {
	for (int i = 0; i < x_max; i++) {
		for (int j = 0; j < z_max; j++) {
			if (cells_p[i][j].time == 0)
				return true;
		}
	}
	return false;
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
	xSemaphoreGive(_CRITICAL_port_access_);
}
void move_x_left(){
	xSemaphoreTake(_CRITICAL_port_access_, portMAX_DELAY);
	uInt8 vp2 = safe_ReadDigitalU8(2);
	setBitValue(vp2, 7, 0);
	setBitValue(vp2, 6, 1);
	safe_WriteDigitalU8(2, vp2);
	xSemaphoreGive(_CRITICAL_port_access_);
}
void stop_x()
{
	xSemaphoreTake(_CRITICAL_port_access_, portMAX_DELAY);
	uInt8 vp2 = safe_ReadDigitalU8(2);
	setBitValue(vp2, 7, 0);
	setBitValue(vp2, 6, 0);
	safe_WriteDigitalU8(2, vp2);
	xSemaphoreGive(_CRITICAL_port_access_);
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
	xSemaphoreGive(_CRITICAL_port_access_);
}void move_z_down() {
	xSemaphoreTake(_CRITICAL_port_access_, portMAX_DELAY);
	uInt8 vp2 = safe_ReadDigitalU8(2);
	setBitValue(vp2, 3, 0);
	setBitValue(vp2, 2, 1);
	safe_WriteDigitalU8(2, vp2);
	xSemaphoreGive(_CRITICAL_port_access_);
}
void stop_z() {
	xSemaphoreTake(_CRITICAL_port_access_, portMAX_DELAY);
	uInt8 vp2 = safe_ReadDigitalU8(2);
	setBitValue(vp2, 3, 0);
	setBitValue(vp2, 2, 0);
	safe_WriteDigitalU8(2, vp2);
	xSemaphoreGive(_CRITICAL_port_access_);
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
	else if (actual_zTop()!=-1) {//antes nao estava
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
	xSemaphoreGive(_CRITICAL_port_access_);
}
void move_y_outside() {
	xSemaphoreTake(_CRITICAL_port_access_, portMAX_DELAY);
	uInt8 vp2 = safe_ReadDigitalU8(2);
	setBitValue(vp2, 5, 0);
	setBitValue(vp2, 4, 1);
	safe_WriteDigitalU8(2, vp2);
	xSemaphoreGive(_CRITICAL_port_access_);
}
void stop_y(){
	xSemaphoreTake(_CRITICAL_port_access_, portMAX_DELAY);
	uInt8 vp2 = safe_ReadDigitalU8(2);
	setBitValue(vp2, 5, 0);
	setBitValue(vp2, 4, 0);
	safe_WriteDigitalU8(2, vp2);
	xSemaphoreGive(_CRITICAL_port_access_);
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
	//xSemaphoreTake(sem_being_used, portMAX_DELAY);
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
	//xSemaphoreGive(sem_being_used, portMAX_DELAY);
}void goto_y_semaphore(int y_dest) {
	xSemaphoreTake(sem_being_used, portMAX_DELAY); 
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
	xSemaphoreGive(sem_being_used,portMAX_DELAY);
}
/*
*
*						Combined Functions
*
*
*/

bool put_piece() {
	if (IsAtDown() && isMiddle()/*&& cage is free*/) {
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
	if (IsAtDown() && isMiddle()&& actual_x()!=-1/*&& cage is free*/) {
		goto_y(1);
		goto_z_up();
		goto_y(2);
		goto_z_dn();
		//Give Info Pakage is taken and in transit
		return true;//teste
	}
	return false;
}



bool isAtCell(int x,int z) {
	return actual_x() == x && actual_z() == z;
}

void goto_xz_task(int x, int z, bool _wait_done = false)
{

	mov pos;
	pos.i = x;
	pos.wait = false;
	//xQueueReceive(mbx_xz, &pos, portMAX_DELAY);
	xSemaphoreTake(sem_being_used, portMAX_DELAY);
	goto_y(2);
	xQueueSend(mbx_xx, &pos, portMAX_DELAY);
	pos.i = z;
	xQueueSend(mbx_z, &pos, portMAX_DELAY);
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
	xSemaphoreGive(sem_being_used);
}
void goto_xz_call(void *) {
	TPosition pos;
	while (1) {
		xQueueReceive(mbx_xz, &pos, portMAX_DELAY);
		goto_xz_task(pos.x, pos.z, true);
	}
}
bool put_pieces_send() {
	bool i = true;
	xQueueSend(mbx_pieces, &i, portMAX_DELAY);
	if (actual_z() != -1 && actual_x() != -1 && actual_y() == 2)
		return true;
	return false;
}
bool put_pieces_check() {
	if (actual_z() != -1 && actual_x() != -1 && actual_y() == 2)
		return true;
	return false;
}
void take_piece_send() {
	bool i = false;
	xQueueSend(mbx_pieces, &i, portMAX_DELAY);
}
void piece_task(void *) {

	bool j = false;
	while (1) {
		xQueueReceive(mbx_pieces, &j, portMAX_DELAY);
		if (actual_z() != -1 && actual_x() != -1 && actual_y() == 2) {//fazer para nao dar para fazer put piece e goxz
			if (j) {
				xSemaphoreTake(sem_being_used, portMAX_DELAY);
				mov i;
				i.i = 0;
				i.wait = true;
				xQueueSend(mbx_z, &i, portMAX_DELAY);
				xSemaphoreTake(sem_z_done, portMAX_DELAY);
				i.i = 1;
				xQueueSend(mbx_y, &i, portMAX_DELAY);
				xSemaphoreTake(sem_y_done, portMAX_DELAY);
				i.i = -1;
				xQueueSend(mbx_z, &i, portMAX_DELAY);
				xSemaphoreTake(sem_z_done, portMAX_DELAY);
				i.i = 2;
				xQueueSend(mbx_y, &i, portMAX_DELAY);
				xSemaphoreTake(sem_y_done, portMAX_DELAY);
				xSemaphoreTake(sem_y_done, portMAX_DELAY);
				bool b = true;
				xQueueSend(mbx_pieces_return, &b, portMAX_DELAY);
				xSemaphoreGive(sem_being_used);
			}
			else {
				xSemaphoreTake(sem_being_used, portMAX_DELAY);
				mov i;
				i.i = 1;
				i.wait = true;
				xQueueSend(mbx_y, &i, portMAX_DELAY);
				xSemaphoreTake(sem_y_done, portMAX_DELAY);
				i.i = 0;
				xQueueSend(mbx_z, &i, portMAX_DELAY);
				xSemaphoreTake(sem_z_done, portMAX_DELAY);
				i.i = 2;
				xQueueSend(mbx_y, &i, portMAX_DELAY);
				xSemaphoreTake(sem_y_done, portMAX_DELAY);
				i.i = -1;
				xQueueSend(mbx_z, &i, portMAX_DELAY);
				xSemaphoreTake(sem_z_done, portMAX_DELAY);
				Result b;
				b.result = true;
				xQueueSend(mbx_pieces_return, &b, portMAX_DELAY);
				xSemaphoreGive(sem_being_used);
			}
		}

		bool b = false;
		xQueueSend(mbx_pieces_return, &b, portMAX_DELAY);
	}
}void piece_task_action(bool j) {

		if (actual_z() != -1 && actual_x() != -1 && actual_y() == 2) {//fazer para nao dar para fazer put piece e goxz
			if (j) {
				xSemaphoreTake(sem_being_used, portMAX_DELAY);
				mov i;
				i.i = 0;
				i.wait = true;
				xQueueSend(mbx_z, &i, portMAX_DELAY);
				xSemaphoreTake(sem_z_done, portMAX_DELAY);
				i.i = 1;
				xQueueSend(mbx_y, &i, portMAX_DELAY);
				xSemaphoreTake(sem_y_done, portMAX_DELAY);
				i.i = -1;
				xQueueSend(mbx_z, &i, portMAX_DELAY);
				xSemaphoreTake(sem_z_done, portMAX_DELAY);
				i.i = 2;
				xQueueSend(mbx_y, &i, portMAX_DELAY);
				xSemaphoreTake(sem_y_done, portMAX_DELAY);
				xSemaphoreGive(sem_being_used);
			}
			else {
				xSemaphoreTake(sem_being_used, portMAX_DELAY);
				mov i;
				i.i = 1;
				i.wait = true;
				xQueueSend(mbx_y, &i, portMAX_DELAY);
				xSemaphoreTake(sem_y_done, portMAX_DELAY);
				i.i = 0;
				xQueueSend(mbx_z, &i, portMAX_DELAY);
				xSemaphoreTake(sem_z_done, portMAX_DELAY);
				i.i = 2;
				xQueueSend(mbx_y, &i, portMAX_DELAY);
				xSemaphoreTake(sem_y_done, portMAX_DELAY);
				i.i = -1;
				xQueueSend(mbx_z, &i, portMAX_DELAY);
				xSemaphoreTake(sem_z_done, portMAX_DELAY);

				xSemaphoreGive(sem_being_used);
			}
	}
}
/*
*
*						 Tasks
*
*/
void show_menu()
{
	printf("\n ***********************Low Level***********************");
	printf("\n gxz...... goto(x,z)");
	printf("\n gy....... goto(y)");
	printf("\n grp...... goto retrive point(1,1) outside");
	printf("\n pp....... Put a part in a specific x, z position");
	printf("\n rp....... Retrieve a part from a specific x, z position");
	printf("\n ip....... Inserts Piece in cage(needs to be at insert position)");
	printf("\n ir....... Removes Piece in cage(needs to be at insert position)");
	printf("\n ***********************HIGH LEVEL**********************");
	printf("\n ppc...... Put product ref in the cell (reference, date, x, z)");
	printf("\n ppfc..... Put product ref in a free cell (reference, date)");
	printf("\n rpc...... Retrieve a product from cell(x, z)");
	printf("\n rpc...... Retrieve a product from cell(x, z)");
	printf("\n re....... Retrieve a product from cell(x, z)");
	printf("\n ***********************INFO LAYER**********************");
	printf("\n pc....... Show info on the product stored in a cell(x, z)");
	printf("\n sp....... Show info on all products at the warehouse");
	printf("\n sc....... Show all the cells (x,z) that have a given product");
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
		fgets(cmd, 20, stdin);
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
			printf("\nX="); fgets(str_x, 20, stdin); x = atoi(str_x);
			printf("\nZ="); fgets(str_z, 20, stdin); z = atoi(str_z);
			if (x >= 1 && x <= 3 && z >= 1 && z <= 3){
				TPosition t;
				t.x = x;
				t.z = z;
				xQueueSend(mbx_xz, &t, portMAX_DELAY);//goto_xz_task(x, z, false);  //try many goto_xz fast
			}
			else
				printf("\nWrong (x,z) coordinates, are you sleeping?... ");
		}
		if (!stricmp(cmd, "gy")) { //pode passar por cima de uma caixa se estiver na posição
			char str_y[20];
			int y = 0;
			printf("\nY="); fgets(str_y, 20, stdin); y = atoi(str_y);
			if (y != 1 && y != 2 && y != 3)
				printf("\nVoce quer que isto va a lua nao?\n\n\n");
			else {
				mov c;
				c.i = y;
				c.wait = true;
				xQueueSend(mbx_y_carefull, &c, portMAX_DELAY);
			}

		}
		if (!stricmp(cmd, "grp")) {
			TPosition t;
			t.x = 1;
			t.z = 1;
			xQueueSend(mbx_xz, &t, portMAX_DELAY);//goto_xz_task(x, z, false);  //try many goto_xz fast
			vTaskDelay(10);
			mov c;
			c.i =3;
			c.wait = true;
			xQueueSend(mbx_y_carefull, &c, portMAX_DELAY);

		}
		if (!stricmp(cmd, "rp")) {
			printf("\nInfo!Info!");
			char str_x[20], str_z[20];
			int x, z, time; // you can use scanf or one else you like
			printf("\nX="); fgets(str_x, 20, stdin); x = atoi(str_x);
			printf("\nZ="); fgets(str_z, 20, stdin); z = atoi(str_z);
			Task c;
			c.pos.x = x;
			c.pos.z = z;
			c.put_take = false;
			xQueueSend(mbx_req, &c, portMAX_DELAY);//falta fazer task para so meter quadno chegar ao sitio certo							
			if (!Expired())
				Turn_off_light_1();
		}
		if (!stricmp(cmd, "pp")) {
			printf("\nInfo!Info!");
			char str_x[20], str_z[20];
			int x, z, time; // you can use scanf or one else you like
			printf("\nX="); fgets(str_x, 20, stdin); x = atoi(str_x);
			printf("\nZ="); fgets(str_z, 20, stdin); z = atoi(str_z);							
			Task c;
			c.pos.x = x;
			c.pos.z = z;
			c.put_take = true;
			strcpy(c.req.id, "");
			c.req.time = -1;
			xQueueSend(mbx_req, &c, portMAX_DELAY);//falta fazer task para so meter quadno chegar ao sitio certo
		}
		if (!stricmp(cmd, "ip")){
			if (actual_x() != 1 || actual_z() != 1 || actual_y() != 3) {
				printf("\n\nYou need to be at the listed position, sorry i am doing what i can :(\n\n\n");
			}
			else
				insert_piece();
		}
		if (!stricmp(cmd, "ir")) {
			if (actual_x() != 1 || actual_z() != 1 || actual_y() != 3) {
				printf("\n\nYou need to be at the listed position, sorry i am doing what i can :(\n\n\n");
			}
			else {
				remove_piece();
			}
		}
		if (!stricmp(cmd, "re")) {
			for (int i = 0; i < x_max; i++) {
				for (int j = 0; j < z_max; j++) {
					if (cells[i][j]) {
						if (cells_p[i][j].time == 0) {
							Task c;
							c.pos.x = i+1;
							c.pos.z = j+1;
							c.put_take = false;
							xQueueSend(mbx_req, &c, portMAX_DELAY);//falta fazer task para so meter quadno chegar ao sitio certo
							cells[i][j] = 0;
							if (!Expired())
								Turn_off_light_1();
						}
					}
				}
			}


		}
		if (stricmp(cmd, "end") == 0)
		{
			safe_WriteDigitalU8(2, 0); //stop all motores;
			vTaskEndScheduler(); // terminates application
		}
		if (stricmp(cmd, "ppc") == 0) {
			printf("\nInfo!Info!");
			char str_x[20], str_z[20], id[20];
			int x, z, time; // you can use scanf or one else you like
			printf("\nX="); fgets(str_x, 20, stdin); x = atoi(str_x);
			printf("\nZ="); fgets(str_z, 20, stdin); z = atoi(str_z);
			if (x >= 1 && x <= 3 && z >= 1 && z <= 3) {
				printf("\nMOAR INFO!!!!!\nTHE CLOCK IS TICKING:");
				fgets(str_x, 20, stdin);  time = atoi(str_x);
				printf("\nMOAR MOAR MOAR!!!!\nID nbr:");
				fgets(id, 20, stdin);
				if (time < -1 || !time)
					printf("\nWoah, your clock is crayzy man!!!!\n\n\n");
				else {
					if (!cells[x - 1][z - 1]){
							for (int i = 0; i < 20; i++) {
								if (id[i] == 0) {
									id[i - 1] = 0;
									break;
								}
							}
							Task c;
							c.pos.x = x;
							c.pos.z = z;
							c.put_take = true;
							strcpy(c.req.id, id);
							c.req.time = time;
							TRequest * item = (TRequest *)malloc(sizeof(TRequest));
							strcpy(item->id, id);
							item->time = time;
							xQueueSend(mbx_req, &c, portMAX_DELAY);//falta fazer task para so meter quadno chegar ao sitio certo
							cells[x - 1][z - 1] = 1;
							cells_p[x - 1][z - 1] = *item;
					}
					else
						printf("\nHum, there is somethin in the way :\\\n\n\n");

				}
			}
			else
				printf("\nWrong (x,z) coordinates, are you sleeping?... \n\n\n");
		}
		if (stricmp(cmd, "rpc") == 0) {//take piece
			printf("\nInfo!Info!");
			char str_x[20], str_z[20];
			int x, z, time, id; // you can use scanf or one else you like
			printf("\nX="); fgets(str_x, 20, stdin); x = atoi(str_x);
			printf("\nZ="); fgets(str_z, 20, stdin); z = atoi(str_z);
			if (x >= 1 && x <= 3 && z >= 1 && z <= 3) {
				if (cells[x - 1][z - 1]) {
					Task c;
					c.pos.x = x;
					c.pos.z = z;
					c.put_take = false;
					xQueueSend(mbx_req, &c, portMAX_DELAY);//falta fazer task para so meter quadno chegar ao sitio certo
					cells[x - 1][z - 1] = 0;							
					if (!Expired())
						Turn_off_light_1();
				}
				else
					printf("\nUgh, There is nothing there man :\\\n\n\n");
			}
			else
				printf("\nWrong (x,z) coordinates, are you sleeping?... \n\n\n");
		}
		if (!stricmp(cmd, "pc")) {//piece in cell
			char str_x[20], str_z[20];
			int x, z; // you can use scanf or one else you like
			printf("\nX="); fgets(str_x, 20, stdin); x = atoi(str_x);
			printf("\nZ="); fgets(str_z, 20, stdin); z = atoi(str_z);
			if (x < 1 || x > 3 || z < 1 || z > 3)
				printf("\nWrong (x,z) coordinates, are you sleeping?... \n\n\n");
			else {
				if (!cells[x - 1][z - 1])
					printf("\n\nWOW, Its full of nothingness :D\n\n\n");
				else {
					printf("\n\nAcording to my calculations it should have some %s that", cells_p[x - 1][z - 1].id);
					if (cells_p[x - 1][z - 1].time && cells_p[x - 1][z - 1].time != -1)
						printf(" still has %d ticks left to expire\n\n\n", cells_p[x - 1][z - 1].time);
					else
						if (!cells_p[x - 1][z - 1].time)
							printf(" that is already expired. Yuch, smells bad man :\\\n\n\n");
						else if (cells_p[x - 1][z - 1].time == -1)
							printf(" will never expire yay ^^\n\n\n");
				}
			}
		}
		if (!stricmp(cmd, "sp")) {
			bool found = false;
			for (int i = 0; i < x_max; i++) {
				for (int j = 0; j < z_max; j++) {
					if (cells[i][j]) {
						printf("\nThere is some %s in cell (%d,%d) that ", cells_p[i][j].id, i+1, j+1);

						if (cells_p[i][j].time > 0)
							printf("has %d ticks to expire\n", cells_p[i][j].time);
						if (cells_p[i][j].time == -1)
							printf("will never expire\n");
						if (cells_p[i][j].time == 0)
							printf("has already expired\n");
						found = true;
					}
				}
			}
			if (!found)
				printf("\nIts all empty D:\nWe are going to be broke if this continues :(");
			putchar('\n');
			putchar('\n');
			putchar('\n');
		}
		if (!stricmp(cmd, "sc")) {
			char reciver[20];
			printf("Mirror mirror on the screen, how many of this item have you seen?\n");
			fgets(reciver, 20, stdin);
			for (int i = 0; i < 20; i++) {
				if (reciver[i] == 0) {
					reciver[i - 1] = 0;
					break;
				}
			}
			printf("\nI have seen some %s on:\n", reciver);
			bool found = false;
			for (int i = 0; i < x_max; i++) {
				for (int j = 0; j < z_max; j++) {
					if (cells[i][j]) {
						if (!stricmp(cells_p[i][j].id, reciver)) {
							printf("cell (%d,%d) and ", i+1, j+1);
							if (cells_p[i][j].time > 0)
								printf("it still has %d ticks to expire\n", cells_p[i][j].time);
							if (cells_p[i][j].time == -1)
								printf("it will never expire\n");
							if (cells_p[i][j].time == 0)
								printf("it has already expired\n");
							found = true;
						}
					}
				}
			}
			if (!found) {
				printf("Nowhere actualy :\\\n");
			}
		}
		if (!stricmp(cmd, "ppfc")) {
			printf("\nInfo!Info!");
			char str_x[20],id[20];
			int x_pos,z_pos ,time; // you can use scanf or one else you like
				printf("\nTHE CLOCK IS TICKING:");
				fgets(str_x, 20, stdin);  time = atoi(str_x);
				printf("\nMOAR MOAR MOAR!!!!\nID nbr:");
				fgets(id, 20, stdin);
				if (time < -1 || !time)
					printf("\nWoah, your clock is crayzy man!!!!\n\n\n");
				else {

					for (int i = 0; i < x_max; i++) {
						for (int j = 0; j < z_max; j++) {
							if (cells[i][j] == 0) {
								x_pos = i + 1;
								z_pos = j + 1;
								i = x_max;
								j = z_max;
							}
						}
						if (i == x_max - 1) {
							x_pos = -1;
						}
					}
					if (x_pos != -1) {
						printf("\n\nfoi escolhido o espaço (%d,%d)\n\n", x_pos, z_pos);
						if (!cells[x_pos - 1][z_pos - 1]) {
							for (int i = 0; i < 20; i++) {
								if (id[i] == 0) {
									id[i - 1] = 0;
									break;
								}
							}
							Task c;
							c.pos.x = x_pos;
							c.pos.z = z_pos;
							c.put_take = true;
							strcpy(c.req.id, id);
							c.req.time = time;
							TRequest * item = (TRequest *)malloc(sizeof(TRequest));
							strcpy(item->id, id);
							item->time = time;
							xQueueSend(mbx_req, &c, portMAX_DELAY);//falta fazer task para so meter quadno chegar ao sitio certo
							cells[x_pos - 1][z_pos - 1] = 1;
							cells_p[x_pos - 1][z_pos - 1] = *item;
						}
					}

					else
						printf("\nHum, the warehouse is full :(\n\n\n");

				}
			
		}
	}

	
}
void goto_y_task_awlwaison(void*) {
	mov c;
	while (1) {
		xQueueReceive(mbx_y_carefull,&c, portMAX_DELAY);
		goto_y_semaphore(c.i);
	}
}
void take_put_task_alwayson(void *) {
	Task c;
	while (1) {
		xQueueReceive(mbx_req, &c, portMAX_DELAY);
		if (c.put_take) {			

			//xSemaphoreTake(sem_being_used, portMAX_DELAY);
			if (!has_piece()) {
				goto_y_semaphore(2);
				goto_xz_task(1, 1, true);
				goto_y_semaphore(3);
				insert_piece();
			}
			goto_y_semaphore(2);
			goto_xz_task(c.pos.x, c.pos.z, true);
			piece_task_action(true);
			//xSemaphoreGive(sem_being_used, portMAX_DELAY);
		}
		else {
			//xSemaphoreTake(sem_being_used, portMAX_DELAY);
			if (has_piece())
				printf("\n\nIm sorry, could you please remove the pice i have on me thank you :)\n\n\n");
			else {
				goto_y_semaphore(2);
				goto_xz_task(c.pos.x, c.pos.z, true);
				piece_task_action(false);
				goto_xz_task(1, 1, true);
				goto_y_semaphore(3);
				remove_piece();
				goto_y_semaphore(2);
			}
			//xSemaphoreGive(sem_being_used, portMAX_DELAY);
		}
	}
}

void goto_x_task(void *)
{
	while (true)
	{
		mov x;
		xQueueReceive(mbx_xx, &x, portMAX_DELAY);
		goto_x(x.i);
		if(x.wait)
			xSemaphoreGive(sem_x_done);
	}
}
void goto_z_task(void *)
{
	while (true)
	{
		mov z;
		xQueueReceive(mbx_z, &z, portMAX_DELAY);
		if (z.i == 0) {
			goto_z_up();
		}
		else
			if (z.i == -1)
			goto_z_dn();
			else goto_z(z.i);
		if(z.wait)
			xSemaphoreGive(sem_z_done);
	}
}
void goto_y_task(void *)
{
	while (true)
	{	
		mov y;
		xQueueReceive(mbx_y, &y, portMAX_DELAY);
		goto_y(y.i);
		if(y.wait)
			xSemaphoreGive(sem_y_done);
	}
}
void goto_y_call(int destination) {//fazer
	//xQueueSend(mbx_y);

}
void keyChecker(void *) {
	bool left = false, right = false, up = false, down = false;
	while (1) {
		vTaskDelay(5);
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
		case 1:	if (xSemaphoreTake(sem_being_used, portMAX_DELAY)) {
			move_x_right();
			xSemaphoreGive(sem_being_used);
		}break;
		case -1:	if (xSemaphoreTake(sem_being_used, portMAX_DELAY)) {
			move_x_left();
			xSemaphoreGive(sem_being_used);
		}break;
		case 2:	if (xSemaphoreTake(sem_being_used, portMAX_DELAY)) {
			move_z_up();
			xSemaphoreGive(sem_being_used);
		}break;
		case -2:	if (xSemaphoreTake(sem_being_used, portMAX_DELAY)) {
			move_z_down();
			xSemaphoreGive(sem_being_used);
		}break;
		case 5:	if (xSemaphoreTake(sem_being_used, portMAX_DELAY)) {
			stop_z();
			xSemaphoreGive(sem_being_used);
		}break;
		case 6:if (xSemaphoreTake(sem_being_used, portMAX_DELAY)) {
			stop_x();
			xSemaphoreGive(sem_being_used);
		}break;
		}
				
		
	}
}
/*
void manager(void *) {//still need to do clock to count time and take ticks from cells with itens
	TRequest  zero;
	zero.time = -1;
	//zero.id = 0; IMPORTANTE
	for (int i = 0; i < x_max; i++) {
		for (int j = 0; j < z_max; j++) {
			cells[i][j] = 0;
			cells_p[i][j] = zero;
		}
	}
	while (1) {
		Task c;
		xQueueReceive(mbx_req, &c, portMAX_DELAY);
		if (c.pos.x > x_max || c.pos.z > z_max || c.pos.x < 0 || c.pos.z < 0)
			printf("Are you craycray? its out of bounds!!\n");
		else {
			if (cells[c.pos.x][c.pos.z] && c.put_take)
				printf("are you crazy? There is nothing in there!!!!\n");
			else {
				if (c.pos.x == 0 || c.pos.z == 0) {
					for (int i = 0; i < x_max; i++) {
						for (int j = 0; j < z_max; j++) {
							if (cells[i][j] == 0) {
								c.pos.x = i+1;
								c.pos.z = j+1;
								i = x_max;
								j = z_max;
							}
						}
						if (i == x_max - 1) {
							printf("There is no space!!\n");
							c.pos.x = -1;
						}
					}
				}
				if (c.pos.x != -1) {
					printf("entra\n");
					goto_xz_task(c.pos.x, c.pos.z,true);
					printf("passa\n");
					put_pieces_send();
					cells[c.pos.x][c.pos.z] = true;
					cells_p[c.pos.x][c.pos.z] = c.req;
				}

			}
			if (!cells[c.pos.x][c.pos.z] && !c.put_take)
				printf("are you crazy? There is somethig  there already");
			else {
				if (c.pos.x == 0 || c.pos.z == 0) {
					printf("Are you craycray? its out of bounds!!\n");
				}
				else {
					goto_xz_task(c.pos.x, c.pos.z);
					take_piece_send();
					cells[c.pos.x][c.pos.z] = false;
					cells_p[c.pos.x][c.pos.z] = zero;
					if (!Expired()) {
						Turn_off_light_1();
					}
				}
			}
			
		}
	}

}

*/
void tick_tack(void *) {
	while (1) {
		for (int i = 0; i < x_max; i++) {
			for (int j = 0; j < z_max; j++) {
				if (cells_p[i][j].id && cells[i][j]&& cells_p[i][j].time>0) {
					cells_p[i][j].time--;
				}
				if (cells_p[i][j].time == 0 && cells[i][j])
					Turn_on_light_1();
			}
		}
		vTaskDelay(1000);
	}
}


void testes(void *) {
}



/********************************************************************************
			MAIN
*/
void main(void) {
	semkit = xSemaphoreCreateCounting(10, 1);   //SEMAPHORE CREATION
	sem_x_done = xSemaphoreCreateCounting(10, 0); // synchronization semaphore
	sem_z_done = xSemaphoreCreateCounting(10, 0);  // synchronization semaphore
	sem_y_done = xSemaphoreCreateCounting(10, 0);
	_CRITICAL_port_access_ = xSemaphoreCreateCounting(10, 1); // exclusive access/resource semaphore 
	sem_being_used = xSemaphoreCreateCounting(10, 1);


															  // maximum number of messages, size of each message
	mbx_pieces = xQueueCreate(10, sizeof(bool));
	sem_x_mov = xQueueCreate(10, sizeof(int));
	mbx_xx = xQueueCreate(10, sizeof(mov));
	mbx_z = xQueueCreate(10, sizeof(mov));
	mbx_y = xQueueCreate(10, sizeof(mov));
	mbx_y_carefull = xQueueCreate(10, sizeof(mov));
	mbx_xz = xQueueCreate(10, sizeof(TPosition));
	mbx_req = xQueueCreate(10, sizeof(Task));
	mbx_pieces_return = xQueueCreate(10, sizeof(Result));
	xTaskCreate(goto_x_task, "v_gotox_task", 100, NULL, 0, NULL);
	xTaskCreate(goto_z_task, "v_gotoz_task", 100, NULL, 0, NULL);
	xTaskCreate(goto_y_task, "v_gotoz_task", 100, NULL, 0, NULL);
	xTaskCreate(task_storage_services, "task_storage_services", 100, NULL, 0, NULL);
	//xTaskCreate(testes, "t", 100, NULL, 0, NULL);
	//xTaskCreate(keyChecker, "key_Checker", 100, NULL, 0, NULL);
	//xTaskCreate(motor_works, "motors", 100, NULL, 0, NULL);
	xTaskCreate(take_put_task_alwayson, "takes", 100, NULL, 0, NULL);
	xTaskCreate(piece_task, "piece_task", 100, NULL, 0, NULL);
	xTaskCreate(tick_tack, "tick", 100, NULL, 0, NULL);
	xTaskCreate(goto_xz_call, "xz", 100, NULL, 0, NULL);
	xTaskCreate(goto_y_task_awlwaison, "y", 100, NULL, 0, NULL);


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
