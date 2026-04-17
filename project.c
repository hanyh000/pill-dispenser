#ifndef F_CPU
#define F_CPU 16000000L
#endif

#include <avr/io.h>
#include <util/delay.h>
#include "Stepper.h"
#include "uart.h"
#include "clcd.h"

/* ������ �� / ��� ���� ������ */
#define TRIG           PB0      // Trigger ��ȣ (��� = PB0)
#define ECHO           PB1      // Echo    ��ȣ (�Է� = PB1)
#define BUZZER         PE4      // Buzzer  ��ȣ (��� = PE4)
#define SOUND_VELOCITY 340UL    // �Ҹ� �ӵ� (m/sec)
#define MAX_ECHO_COUNT 3        // �ִ� Echo ���� Ƚ��

#define SERVO_PIN PD5

/* ������ ���� ���� ���� ������ */
int state      = 0;   // ���� ���� (0: ���� ���, 1: ������ ����)
int echo_count = 0;   // Echo ���� Ƚ��

/* ���� �ʱ�ȭ  (Timer0 ? Fast PWM, 8����) */

void servo_init(void) {
    TCCR0  = (1 << WGM01) | (1 << WGM00) | (1 << CS01);
    DDRD  |= (1 << SERVO_PIN);
}

/* ������ ���� �ʱ�ȭ */
void ultrasonic_init(void) {
    DDRB |=  (1 << TRIG);
    DDRB &= ~(1 << ECHO);
}

/* ������ �Ÿ� ����  (��ȯ��: mm ����) */
uint16_t measure_distance(void) {
    uint32_t count = 0;

    PORTB &= ~(1 << TRIG);
    _delay_us(2);
    PORTB |=  (1 << TRIG);
    _delay_us(10);
    PORTB &= ~(1 << TRIG);

    while (!(PINB & (1 << ECHO)));
    while (  PINB & (1 << ECHO)) {
        _delay_us(1);
        count++;
        if (count > 60000) break;
    }

    return (uint16_t)((count * SOUND_VELOCITY) / (2UL * 10000UL));
}

/* ���� */
int main(void) {
    int i;

    USART_Init(9600);
    STEPPER_Init();
    servo_init();
    ultrasonic_init();
    DDRE |= (1 << BUZZER);

    i2c_lcd_init();
    i2c_lcd_write_string("BT Pill System");
    USART_TransmitString("System Ready\r\n");

    while (1) {

        /* state 0 : UART ���� ��� */
        if (state == 0) {

            if (USART_Receive_Ready()) {
                char rx = USART_Receive();

                if (rx == 'S') {

                    /* Timer1 ���� ���Ϳ� PWM ���� (PB5) */
                    DDRB  |= 0x20;
                    TCCR1A = 0x82;
                    TCCR1B = 0x1A;
                    OCR1A  = 3000;          // �ʱ� ��ġ (0��)
                    ICR1   = 19999;
                    PORTB |= (1 << PORTB5);

                    USART_TransmitString("Pill");
                    i2c_lcd_write_string("Working.");

                    /* ���ܸ��� ���� 60�� ȸ�� (6ĭ ���� �� ĭ �̵�) */
                    STEPPER_Rotate(STEPPER_60STEP, 1, 170);
                    i2c_lcd_write_string("Working..");
                    _delay_ms(500);
                    i2c_lcd_write_string("Working...");

                    /* �������� ���� */
                    OCR1A = 500;            // 90�� ȸ�� (�� ����)
                    _delay_ms(500);
                    i2c_lcd_write_string("take out a pill!");
                    OCR1A = 3000;           // 0�� ����
                    _delay_ms(100);
                    PORTB &= ~(1 << PORTB5);
                    OCR1A  = 0;

                    state = 1;
                }
            }

        /* state 1 : ������ ������ �� ���� */
        } else if (state == 1) {

            unsigned int distance = measure_distance();

            if (distance < 300) {

                for (i = 0; i < 30; i++) {
                    PORTE |=  (1 << BUZZER);
                    _delay_ms(1);
                    PORTE &= ~(1 << BUZZER);
                    _delay_ms(1);
                }
                _delay_ms(100);

                echo_count++;

                if (echo_count >= MAX_ECHO_COUNT) {
                    echo_count = 0;
                    state      = 0;
                }
            }
        }
    }

    return 0;
}
