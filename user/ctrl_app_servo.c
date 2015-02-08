#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "espconn.h"
#include "driver/uart.h"

#include "ctrl_app_servo.h"
#include "ctrl_platform.h"
#include "ctrl_stack.h"

os_timer_t tmrBase;

// SERVO CONTROL PIN IS AT GPIO 12 !

unsigned int servoDuration = 1500; // default: center

static void ICACHE_FLASH_ATTR ctrl_app_servo_pulse(void *arg)
{
	gpio_output_set((1<<12), 0, (1<<12), 0); // ON

	// how long should the pulse last now, from 1 to 1.5 (clockwise) and 1.5 to 2ms (counterclockwise) they say
	// prevent from servo damage
	if(servoDuration < 1000 || servoDuration > 2000) servoDuration = 1500;
	os_delay_us(servoDuration);

	gpio_output_set(0, (1<<12), (1<<12), 0); // OFF
}

static unsigned char ICACHE_FLASH_ATTR ctrl_app_message_received(tCtrlMessage *msg)
{
	// my custom app receives a MSG!

	uart0_sendStr("APP GOT NEW MSG: ");

	char tmp2[50];

	unsigned short i;
	for(i=0; i<msg->length-1-4; i++)
	{
		os_sprintf(tmp2, " 0x%X", msg->data[i]);
		uart0_sendStr(tmp2);
	}
	uart0_sendStr(".\r\n");

	// validate received data from Server (actually from Client who sent it)
	if(msg->length-1-4 != 4) {
		uart0_sendStr("Wrong data length received.\r\n");
		return 0;
	}

	char dur[5];
	dur[4] = '\0';
	os_memcpy(dur, msg->data, 4);

	servoDuration = (int)strtol(dur, (char **)NULL, 10);

	os_sprintf(tmp2, "UNPACKED %u [ms]\r\n", servoDuration);
	uart0_sendStr(tmp2);

	// lets send back the data we received to all Clients listening to this Base. Send it as notification if we received this task as a notification
	if(ctrl_platform_send(dur, msg->length-1-4, (msg->header & CH_NOTIFICATION)))
	{
		uart0_sendStr("> Failed to send back the data!\r\n");
	}

	return 0; // lets say everything is OK and we don't need the Server to backoff next message
}

// entry point to the temperature logger app
void ICACHE_FLASH_ATTR ctrl_app_init(tCtrlAppCallbacks *ctrlAppCallbacks)
{
	ctrlAppCallbacks->message_received = ctrl_app_message_received;

	uart0_sendStr("ctrl_app_init()\r\n");

	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12); // Set GPIO12 function
	gpio_output_set(0, (1<<12), (1<<12), 0);

	// base pulse at 20ms
	os_timer_disarm(&tmrBase);
	os_timer_setfn(&tmrBase, (os_timer_func_t *)ctrl_app_servo_pulse, NULL);
	os_timer_arm(&tmrBase, 20, 1); // 1 = repeat automatically
}
