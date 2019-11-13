#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>

#include "usbdrv/usbdrv.h"

#include <util/delay.h>
#include <string.h>

//USB���� �������� ���� �Է¹��� request �޼����� ���� 
#define ERROR_CODE 0	
#define BOOT_INIT 1

static uint8_t RxBuffer[20]; //Ű����κ��� ���޹��� ���� �����ϴ� ����
static uint8_t replybuf[5]; //USB������������ ���� �����ϴ� ����
static uint8_t intbuf[8]; //USB���ͷ�Ʈ �������� ���� �����ϴ� ����
volatile unsigned char Rxflag = 0; //Ű����κ��� ���� ����� ���Ź޾����� �˸��� flag
volatile unsigned int Rxcur = 0; //RxBuffer�� ������ �����ϴ� Ŀ��
static uint8_t LedState; //capslock�� LEDstate������ ��Ÿ���� ����

void BT_Init(unsigned long xtal, unsigned long bps); //����������� ���۰��� ������ �ʱ�ȭ�ϴ� �Լ�
void BT_PutChar(char byData); //������� ���� (Ű����)�� �����͸� �����ϴ� �Լ�

// this gets called when custom control message is received
USB_PUBLIC uchar usbFunctionSetup(uchar data[8]) { 
	usbRequest_t *rq = (void *) data; //���� �����͸� rq�� ����

	replybuf[0] = rq->bRequest; //reply ������ ù �ε����� ���۹��� request������ ä��
	if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_VENDOR) { //���� request�� Ÿ���� ��ġ�� ���
		if(rq->bRequest == BOOT_INIT) //request�� BOOT_INIT�� ��� 
			replybuf[1] = BOOT_INIT; //reply������ �ι�° �ε����� BOOT_INIT���� ä��
		else //�� �ܴ� ����
			replybuf[1] = ERROR_CODE; //reply������ �ι�° �ε����� EEROR_CODE�� ä��
		usbMsgPtr = (unsigned char *) replybuf; //repl

  	return 2; //replybuffer�� �� �������� ũ���ȯ
	}
	//��ġ���� ���� ��� -> CAPSLOCK ���� 
	return USB_NO_MSG; //usbFunctionWrite�Լ� ȣ��
}
usbMsgLen_t usbFunctionWrite(uint8_t * data, uchar len) {
	if (data[0] == LedState) //data[0]�� ���� ���� LedState���� ��ġ�� ��� �Լ� ����
  	return 1;
  else //�ٸ� ��� LedState �� ����
  	LedState = data[0];
 
	BT_PutChar(LedState); //���ŵ� LedState�� ����
	BT_PutChar(0x0D);
  
	return 1; // Data read, not expecting more
}

ISR(USART_RXC_vect) { //����佺���(Ű����)�� ���� �����͸� ���۹����� ���ͷ�Ʈ �߻�
	RxBuffer[Rxcur] = UDR; //���۹��� �����͸� RxBuffer�� ����
	switch(RxBuffer[Rxcur]) { //���۹��� �������� ������ �ľ�
		case 0x0D: //���������� ��ü���� ���� �������� ���� �˸�
			if(Rxcur < 8) { //RxBuffer�� ���̰� 8���� ª�� ���
				if(strncmp("+READY",(char *)RxBuffer,6) == 0) //RxBuffer�� ������ READY�� ��� �����Ⱚ Rxflag����
					Rxflag = 0;
				else  //�� ���� ������ ���ͷ�Ʈ�������� PC�� �����ؾ��� (ERROR)
					intbuf[1] = 0x04;
			}
			else //RxBuffer�� ���̰� 8�̻��� ���
			{
				if(strncmp("+CONNECTED", (char *)RxBuffer,10) == 0) //RxBuffer�� ������ CONNECTED�� ��� 
				{	
					intbuf[1] = 0x02; //���ͷ�Ʈ�������� PC�� �ش系���� �����ؾ���
					LedState = 0x0F; //LedState�� �����Ⱚ ����
				}
				else if(strncmp("+DISCONNECTED", (char *)RxBuffer, 13) == 0) //RxBuffer�� ������ DISCONNECTED�� ��� 
					intbuf[1] = 0x03; //���ͷ�Ʈ�������� PC�� �ش系���� �����ؾ���
				else if(strncmp("+ADVERTISING", (char *)RxBuffer,12) == 0) //RxBuffer�� ������ ADVERTISING�� ��� 
					Rxflag = 0; //�����Ⱚ�̹Ƿ� Rxflag����
				else //�� ���� ������ ���ͷ�Ʈ�������� PC�� �����ؾ��� (ERROR)
					intbuf[1] = 0x04;
			}	
			Rxflag = 1; //Rxflag 1�� ����
			Rxcur = 0; //RxBuffer�� Ŀ�� 0���� �ʱ�ȭ
			break;

		case 0xEF: //Ű���忡�� ���� �������� ���� �˸�
			intbuf[0] = RxBuffer[0]; //Ư��Ű���� �Է°��� 0�� �ε����� ����
			intbuf[1] = 0x01; //PC�� Ű���忡�� ���޵� ������ �˸��� ����
			for(int i = 2; i <= Rxcur; i++) //Ű���忡 ������ Ű���� ��ĵ�ڵ带 ����
				intbuf[i] = RxBuffer[i-1]; 
			Rxcur = 0; 
			Rxflag = 1; //Rxflag 1�� ���� //RxBuffer�� Ŀ�� 0���� �ʱ�ȭ
			break;

		case 0xFE: //Ű���忡�� LedState���� ��û��
			BT_PutChar(LedState); //LedState�� ����
			BT_PutChar(0x0D);
			Rxcur = 0; //RxBuffer�� Ŀ�� 0���� �ʱ�ȭ
			break;

		default: //���� ���� �̿Ϸ�
			Rxcur++; //RxBuffer�� Ŀ�� �ű�
			break;
	}
}

int main() {
		uint16_t i;
    wdt_enable(WDTO_1S); // enable 1s watchdog timer

    usbInit();

    usbDeviceDisconnect(); // enforce re-enumeration
    for(i = 0; i<250; i++) { // wait 500 ms
        wdt_reset(); // keep the watchdog happy
        _delay_ms(2);
    }
		BT_Init(F_CPU,38400);
    usbDeviceConnect();
  	
	  sei(); // Enable interrupts after re-enumeration
	
    while(1) {
        wdt_reset(); // keep the watchdog happy
        usbPoll();
				
				if(Rxflag == 1) { //Rxflag�� 1�� ������ ���
					if(usbInterruptIsReady()) { //���ͷ�Ʈ�������� �غ�� ��� intbuf�� ���� ������ ����
						usbSetInterrupt(intbuf,8);
					}
					Rxflag = 0; //Rxflag ����
				}
    }
	
    return 0;
}

void BT_Init(unsigned long xtal, unsigned long bps) {
  unsigned long temp;

  UCSRB = (1 << TXEN) | (1 << RXEN);
  UCSRB |= (1 << RXCIE);

  temp = xtal/(bps * 16UL) - 1;

	UBRRL = temp & 0xFF;
  UBRRH = (temp >> 8) & 0xFF;
}
void BT_PutChar(char byData) {
  while(!(UCSRA & (1 << UDRE)));
  UDR = byData;
}
