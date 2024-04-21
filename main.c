/*
 * atmega88.c
 *
 * Created: 2023-12-30 오전 6:10:15
 * Author : kaon
 */ 

#define F_CPU 8000000
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>

#define LCD_DDR DDRC
#define LCD_PORT PORTC
#define ENABLE LCD_PORT |= 0x02
#define DISABLE LCD_PORT &= ~0x02

#define SLEEP 60

uint8_t cur = 0x00;
uint8_t pre = 0x00;

uint8_t x = 0;
uint8_t y = 0;
uint8_t select = 0;
uint8_t page = 0;
uint8_t push = 0x00;

char str[14];
uint8_t sleep = 0;
uint8_t time[5] = {0, 0, 0, 0, 0};

uint8_t i = 0;

uint8_t cg[16] = {
	0x07, 0x07, 0x0e, 0x0e, 0x0e, 0x0e, 0x11, 0x00,
	0x07, 0x07, 0x0e, 0x0e, 0x0e, 0x0e, 0x0a, 0x00
};

char ex[3][16] ={
	{"G - DINO GAME"},
	{"C - TIME CHANGE"},
	{"T - NOW TIME"}
};

////////////////////////////////////////////////////////////////////////////////RTC 코드

void rtc_clk(){
	PORTD |= 0x02;
	_delay_us(50);
	PORTD &= ~(0x02);
	_delay_us(50);
}

void rtc_write(uint8_t add, uint8_t data){
	uint8_t RTCD = ((data / 10) << 4) + (data%10);
	PORTD |= 0x01;
	_delay_us(50);
	for(i = 0; i < 8; i++){
		PORTD = (0x01 | (((add & (0x01 << i)) >> i) << 2));
		rtc_clk();
	}
	_delay_us(50);
	for(i = 0; i < 8; i++){
		PORTD = (0x01 | (((RTCD & (0x01 << i)) >> i) << 2));
		rtc_clk();
	}
	_delay_us(50);
	PORTD &= ~(0x01);
}

uint8_t rtc_read(uint8_t add){
	uint8_t RTCD = 0x00;
	PORTD |= 0x01;
	_delay_us(50);
	for(i = 0; i < 8; i++){
		PORTD = (0x01 | (((add & (0x01 << i)) >> i) << 2));
		rtc_clk();
	}
	PORTD &= ~(0x04);
	DDRD &= ~(0x04);
	_delay_us(50);
	for(i = 0; i < 8; i++){
		RTCD |= (((PIND & 0x04) >> 2) << i);
		rtc_clk();
	}
	PORTD &= ~(0x01);
	_delay_us(50);
	DDRD |= 0x04;
	return (RTCD >> 4) * 10 + (RTCD & 0x0f);
}

////////////////////////////////////////////////lcd 코드

void pulesE(){
	ENABLE;
	_delay_us(100);
	DISABLE;
	_delay_us(100);
}

void lcd_send(uint8_t cmd, uint8_t data){
	LCD_PORT = (cmd == 1 ? (((data & 0xf0) >> 2) | 0x01) : ((data & 0xf0) >> 2));
	pulesE();
	LCD_PORT = (cmd == 1 ? (((data & 0x0f) << 2) | 0x01) : ((data & 0x0f) << 2));
	pulesE();
}

void lcd_curse(uint8_t x, uint8_t y){
	lcd_send(0, 0x80 | ((0x40 * y) + x));
}

void lcd_string(char* str){
	while(*str) lcd_send(1, *str++);
}

void lcd_init(){
	_delay_ms(40);
	LCD_PORT &= 0x03;
	LCD_PORT |= 0x0c;
	pulesE();

	_delay_ms(6);
	LCD_PORT &= 0x03;
	LCD_PORT |= 0x0c;
	pulesE();

	_delay_us(300);
	LCD_PORT &= 0x03;
	LCD_PORT |= 0x0c;
	pulesE();
	
	_delay_ms(2);
	LCD_PORT &= 0x03;
	LCD_PORT |= 0x08;
	pulesE();
	
	_delay_ms(2);
	lcd_send(0, 0x28);
	lcd_send(0, 0x0e);
	lcd_send(0, 0x01);
	lcd_send(0, 0x06);
}

void lcd_cg_load(){
	lcd_send(0, 0x40);
	for(int i = 0; i < 16; i++) lcd_send(1, cg[i]);
	lcd_send(0, 0x02);
}

void lcd_clear(){
	lcd_send(0, 0x01);
	_delay_ms(2);
}

//////////////////////////////////////////////////////////////// 버튼

void page_set();
void button_active(){
	cur = PINB & 0x1f;
	 if((cur & 0x01) != 0 && (pre & 0x01) == 0){
		 push |= 0x01;
		 if(x > 0) x--;
		 else x = 15;
	 }else push &= ~(0x01);
	 if((cur & 0x02) != 0 && (pre & 0x02) == 0){
		 push |= 0x02;
		 if(x < 15) x++;
		 else x = 0;
	 }else push &= ~(0x02);
	 if((cur & 0x04) != 0 && (pre & 0x04) == 0){
		 push |= 0x04;
		 if(y == 1) y--;
		 else y = 1;
	 }else push &= ~(0x04);
	 if((cur & 0x08) != 0 && (pre & 0x08) == 0){
		 push |= 0x08;
		 if(y == 0) y++;
		 else y = 0;
	 }else push &= ~(0x08);
	 if((cur & 0x10) != 0 && (pre & 0x10) == 0){
		 select = y * 16 + x;
		 push |= 0x10;
		 page_set();
	 }else push &= ~(0x10);

	pre = cur;
}

/////////////////////////////////////////////////////////  범용


///////////////////////////////////////시계
void get_clock(){
	time[0] = rtc_read(0x81);
	time[1] = rtc_read(0x83);
	time[2] = rtc_read(0x85);
	time[4] = rtc_read(0x87);
	time[3] = rtc_read(0x89);
}

void set_clock(){
	rtc_write(0x80, time[0]);
	rtc_write(0x82, time[1]);
	rtc_write(0x84, time[2]);
	rtc_write(0x86, time[4]);
	rtc_write(0x88, time[3]);
}

void lcd_clock_set(uint8_t x, uint8_t y){
	lcd_curse(x, y);
	sprintf(str, "%02d-%02d-%02d-%02d-%02d", time[4], time[3], time[2], time[1], time[0]);
	lcd_string(str);
}

///////////////////////////////////패이지 설정

void page_set(){
	if(page == 0){
		lcd_clear();
		lcd_curse(0, 0);
		lcd_string("         G T C ?");
	}
	if((push & 0x10) != 0){
		if(page == 0 && select == 9){
			lcd_clear();
			page = 4;
			lcd_curse(0, 0);
			lcd_string("START GAME?  GO");
			lcd_curse(0, 1);
			lcd_string("<");
			while(1){
				button_active();
				if(select == 16) break;
				
				if((select == 13 || select == 14) && (push & 0x10) != 0){
					uint8_t time = 0;
					uint8_t anim = 0;
					uint8_t pos = 1;
					lcd_send(0, 0x0c);
					while(1){
						button_active();
						if(time == 3){
							time = 0;
							pos = 1;
							anim ^= 1;
						}
						if((push & 0x10) != 0) anim = time = pos = 0;
						if((push & 0x01) != 0) break;
						lcd_clear();
						lcd_curse(0, pos);
						lcd_send(1, anim);
						
						_delay_ms(50);
						time++;
					}
					lcd_clear();
					lcd_curse(0, 0);
					lcd_string("START GAME?  GO");
					lcd_curse(0, 1);
					lcd_string("<");
				}
				lcd_send(0, 0x0e);
				lcd_curse(x, y);
				_delay_ms(10);
			}
		}
		if(page == 0 && select == 11){
			uint8_t time = 0;
			lcd_clear();
			page = 2;
			lcd_curse(0, 1);
			lcd_string("<       NOW TIME");
			while(1){
				button_active();
				if(select == 16) break;
				if(time == 10){
					time = 0;
					get_clock();
					lcd_clock_set(0, 0);
				}
				lcd_curse(x, y);
				_delay_ms(10);
				time++;
			}
		}
		if(page == 0 && select == 13){
			lcd_clear();
			page = 3;
			get_clock();
			lcd_clock_set(0, 0);
			lcd_curse(0, 1);
			lcd_string("<    TIME CHANGE");
			while(1){
				button_active();
				if(select == 16) break;
				
				if(select == 1 || select == 4 || select == 7 || select == 10 || select == 13) y = 0;
				
				if((push & 0x04) != 0) {
					switch (select)
					{
						case 1:
							if(time[4] != 12) time[4]++;
							break;
						case 4:
							if(time[3] != 31) time[3]++;
							break;
						case 7:
							if(time[2] != 24) time[2]++;
							break;
						case 10:
							if(time[1] != 60) time[1]++;
							break;
						case 13:
							if(time[0] != 60) time[0]++;
							break;
					}
					lcd_clock_set(0, 0);
				}else if((push & 0x08) != 0) {
					switch (select)
					{
						case 1:
							if(time[4] != 0) time[4]--;
							break;
						case 4:
							if(time[3] != 0) time[3]--;
							break;
						case 7:
							if(time[2] != 0) time[2]--;
							break;
						case 10:
							if(time[1] != 0) time[1]--;
							break;
						case 13:
							if(time[0] != 0) time[0]--;
							break;
					}
					lcd_clock_set(0, 0);
				}
				
				lcd_curse(x, y);
				_delay_ms(10);
			}
		}
		if(page == 0 && select == 15){
			uint8_t line = 0;
			lcd_clear();
			page = 1;
			lcd_curse(0, 0);
			lcd_string(ex[line]);
			lcd_curse(0, 1);
			lcd_string("<   ");
			lcd_send(1, 0x7f);
			lcd_send(1, 0x7e);
			while(1){
				button_active();
				if(select == 16) break;
				
				if((push & 0x10) != 0){
					switch(select){
						case 20:
						lcd_clear();
						lcd_curse(0, 0);
						if(line == 0) line = 2;
						else line--;
						lcd_string(ex[line]);
						lcd_curse(0, 1);
						lcd_string("<   ");
						lcd_send(1, 0x7f);
						lcd_send(1, 0x7e);
						break;
						case 21:
						lcd_clear();
						lcd_curse(0, 0);
						if(line == 2) line = 0;
						else line++;
						lcd_string(ex[line]);
						lcd_curse(0, 1);
						lcd_string("<   ");
						lcd_send(1, 0x7f);
						lcd_send(1, 0x7e);
						break;
					}
				}
				
				lcd_curse(x, y);
				_delay_ms(10);
			}
		}
		if((page == 1 || page == 2 || page == 3 || page == 4) && select == 16){
			if(page == 3) set_clock();
			page = 0;
			page_set();
		}
	}
}


/////////////////////////////////////////////////////////////////메인

void setup(){
	LCD_DDR = 0xff;
	LCD_PORT = 0x00;
	
	 DDRB = 0x1f;
	 DDRD = 0x07;
	 
	 TCCR0A = 0;
	 TCCR0B = 0x05;
	 TIMSK0 = 0x01;
	 TCNT0 = 5;
	 
	 sei();
}

int main(void)
{
	setup();
	
	lcd_init();
	lcd_cg_load();
	lcd_clear();
	
	page_set();
	
    while (1) 
    {
 		button_active();
		lcd_curse(x, y);
		_delay_ms(10);
    }
}

ISR(TIMER0_OVF_vect){
	TCNT0 = 5;
	if(sleep >= SLEEP * 100){
		sleep = 0;
		lcd_clear();
		lcd_curse(0, 1);
		lcd_string("Click Anything!!");
		lcd_send(0, 0x0c);
		while(1){
			button_active();
			if(push != 0) break;
			if(sleep >= 5){
				sleep = 0;
				get_clock();
				lcd_clock_set(1, 0);
			}
			_delay_ms(10);
			sleep++;
		}
		lcd_send(0, 0x0e);
		sleep = 0;
		lcd_clear();
		page_set();
	}else if(push != 0) sleep = 0;
	else sleep++;
}

