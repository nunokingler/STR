// ConsoleApplication1.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "NIDAQmx.h"
#include "stdafx.h"
#include<conio.h>
#include<stdlib.h>
#include "interface.h"
#include "ConsoleApplication1.h"
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

xSemaphoreHandle Semkit;


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
uInt8 read_port(int porto)
{
	uInt8 aa = 0;
	xSemaphoreTake(Semkit, portMAX_DELAY);
	aa = ReadDigitalU8(porto);
	xSemaphoreGive(Semkit);
	return(aa);
}
void write_port(int porto, uInt8 value)
{
	uInt8 aa = 0;
	xSemaphoreTake(Semkit, portMAX_DELAY);
	WriteDigitalU8(porto, value);
	xSemaphoreGive(Semkit);
}




/**
 *	FUNCOES X
 *
 */
void move_x_right()
{
	uInt8 vp2 = ReadDigitalU8(2);
	setBitValue(vp2, 6, 0);
	setBitValue(vp2, 7, 1);
	WriteDigitalU8(2, vp2);
}
void move_x_left(){
	uInt8 vp2 = ReadDigitalU8(2);
	setBitValue(vp2, 7, 0);
	setBitValue(vp2, 6, 1);
	WriteDigitalU8(2, vp2);
}
void stop_x()
{
	uInt8 vp2 = ReadDigitalU8(2);
	setBitValue(vp2, 7, 0);
	setBitValue(vp2, 6, 0);
	WriteDigitalU8(2, vp2);
}
bool is_moving_left()
{
	int vp2 = ReadDigitalU8(2);
	return getBitValue(vp2, 6);
}
bool is_moving_right()
{
	int vp2 = ReadDigitalU8(2);
	return getBitValue(vp2, 7);
}
bool is_at_x(int Pos)
{
	int vp0 = ReadDigitalU8(0);
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
	int vp0 = ReadDigitalU8(0);
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
			printf("foi para a direita\n");
		}
		else if (actual_x() > x_dest) {
			move_x_left();
			printf("foi para a esquerda\n");
		}
		//while position not reached             
		while (actual_x() != x_dest) {
			_sleep(10);
		}
		printf("parou\n");
		stop_x();   // arrived.
	}
}/*
 *			FUNTIONS Z
 *
 */

bool IsAtUp() {
	uInt8 vp0 = ReadDigitalU8(0);//ver se tem algum bit de cima activado
	uInt8 vp1 = ReadDigitalU8(1);
	return !getBitValue(vp0, 6) || !getBitValue(vp1, 0) || !getBitValue(vp1, 2);
}
int actual_z()
{	//uses the sensor below each cell only, not the upper one
	int vp1 = ReadDigitalU8(1);
	int vp0 = ReadDigitalU8(0);
	if (!getBitValue(vp1, 3))  		return(1);
	if (!getBitValue(vp1, 1))  		return(2);
	if (!getBitValue(vp0, 7))  		return(3);
	return(-1);
}

bool IsAtDown() {
	return actual_z()!=-1 && actual_z()!=0;
}
bool isAtz(int z) {
	return actual_z() == z;
}
	
void move_z_up() {
	uInt8 vp2 = ReadDigitalU8(2);
	setBitValue(vp2, 3, 1);
	setBitValue(vp2, 2, 0);
	WriteDigitalU8(2, vp2);
}void move_z_down() {
	uInt8 vp2 = ReadDigitalU8(2);
	setBitValue(vp2, 3, 0);
	setBitValue(vp2, 2, 1);
	WriteDigitalU8(2, vp2);
}
void stop_z() {
	uInt8 vp2 = ReadDigitalU8(2);
	setBitValue(vp2, 3, 0);
	setBitValue(vp2, 2, 0);
	WriteDigitalU8(2, vp2);
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
	int vp0 = ReadDigitalU8(0);
	int vp1 = ReadDigitalU8(1);
	if (!getBitValue(vp0, 6) || !getBitValue(vp1, 0) || !getBitValue(vp1, 2))
		return true;
	return false;
}
/*
*				Funtions Y
*
*/
void move_y_inside() {
	uInt8 vp2 = ReadDigitalU8(2);
	setBitValue(vp2, 5, 1);
	setBitValue(vp2, 4, 0);
	WriteDigitalU8(2, vp2);
}
void move_y_outside() {
	uInt8 vp2 = ReadDigitalU8(2);
	setBitValue(vp2, 5, 0);
	setBitValue(vp2, 4, 1);
	WriteDigitalU8(2, vp2);
}
void stop_y(){
	uInt8 vp2 = ReadDigitalU8(2);
	setBitValue(vp2, 5, 0);
	setBitValue(vp2, 4, 0);
	WriteDigitalU8(2, vp2);
}
int actual_y() {//3 for outside, 1 for inside
	int vp0 = ReadDigitalU8(0);
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


/********************************************************************************
			MAIN
*/
void vTaskCode_1(void * pvParameters)
{
	for (;; ) {
		printf("\nHello from TASK_1");
		// Although the kernel is in preemptive mode, 
		// we should help switch to another
		// task with e.g. vTaskDelay(0) 
		vTaskDelay(0);
	}
}
void vTaskCode_2(void * pvParameters)
{
	for (;; )
	{
		printf("\nHello from TASK_2. I will sleep longer...");
		vTaskDelay(10);
	}
}
void vTaskHorizontal(void * pvParameters)
{
	while (TRUE)
	{
		//go right
		uInt8 aa = read_port(2);
		write_port(2, (aa & (0xff - 0x40)) | 0x80);

		// wait till last sensor
		while ((read_port(0) & 0x01)) { vTaskDelay(0); }

		// go left		
		aa = read_port(2);
		write_port(2, (aa & (0xff - 0x80)) | 0x40);

		// wait till last sensor
		while ((read_port(0) & 0x04)) { vTaskDelay(0); }
	}
}

void vTaskVertical(void * pvParameters)
{
	while (TRUE)
	{
		//go up
		uInt8 aa = read_port(2);
		write_port(2, (aa & (0xff - 0x04)) | 0x08);

		// wait till z=3		
		while ((read_port(0) & 0x40)) { vTaskDelay(1); }

		// go down		
		aa = read_port(2);
		write_port(2, (aa & (0xff - 0x08)) | 0x04);

		// wait till z=1		
		while ((read_port(1) & 0x08)) { vTaskDelay(1); }
	}
}




void main(void) {
	create_DI_channel(0);
	create_DI_channel(1);
	create_DO_channel(2);
	Semkit = xSemaphoreCreateCounting(1, 1);   //SEMAPHORE CREATION
	xTaskCreate(vTaskHorizontal, "TaskHoriz", 100, NULL, 0, NULL);
	xTaskCreate(vTaskVertical, "TaskVertcal", 100, NULL, 0, NULL);

	vTaskStartScheduler();

	WriteDigitalU8(2, 0);  // all bits of port 2 set to z => stop all motors
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
