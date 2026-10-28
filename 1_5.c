/*
 * ATmega128 - Dieu khien dong co DC (cau H), LCD LM044L (4x20), nut nhan
 * PHIEN BAN TRUOC KHI CO MENU: moi nut gan cung 1 hanh dong
 * Kien truc: External Memory (XMEM) + 74HC573 + 74154
 *
 * Ban do dia chi (A4 = 0, A0..A3 giai ma bang 74154, byte cao khong giai ma):
 *   0x110B : LCD DATA   (CS11, A0=1 -> RS=1)
 *   0x110C : LCD CMD    (CS12, A0=0 -> RS=0)
 *   0x110D : MOTOR      (CS13) bit0=DC1A, bit1=DC1B
 *   0x110E : BUTTON     (CS14) doc, tich cuc muc thap
 *
 * PG3 (chan "LCD" tren so do) = chan E cua LCD.
 */
#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <util/atomic.h>
#include <stdio.h>
#include <stdint.h>

/* ---------- Dia chi thiet bi tren bus XMEM ---------- */
#define LCD_DATA (*(volatile uint8_t *)0x110B)
#define LCD_CMD  (*(volatile uint8_t *)0x110C)
#define MOTOR    (*(volatile uint8_t *)0x110D)
#define BUTTON   (*(volatile uint8_t *)0x110E)

/* ---------- Nut nhan (sau khi dao bit: 1 = dang nhan) ---------- */
#define BTN_LEFT  (1 << 0)
#define BTN_RIGHT (1 << 1)
#define BTN_UP    (1 << 2)
#define BTN_DOWN  (1 << 3)
#define BTN_GO    (1 << 4)
#define BTN_1     (1 << 5)
#define BTN_2     (1 << 6)
#define BTN_3     (1 << 7)

/* ---------- Dong co: bit0 = DC1A, bit1 = DC1B ---------- */
#define MOTOR_FWD  0x02
#define MOTOR_REV  0x01
#define MOTOR_STOP 0x03

#define PWM_STEPS  16          /* toc do 0..16 */

typedef enum { DIR_STOP = 0, DIR_FWD, DIR_REV } dir_t;

static volatile dir_t   g_dir   = DIR_STOP;
static volatile uint8_t g_speed = 8;

/* ================= XMEM ================= */
static void xmem_init(void)
{
    MCUCR |= (1 << SRE);       /* bat giao tiep bo nho ngoai */
}

/* ================= PWM mem cho dong co (Timer0) ================= */
static void motor_init(void)
{
    MOTOR = MOTOR_STOP;
    TCCR0 = (1 << WGM01) | (1 << CS02);   /* CTC, chia 64 */
    OCR0  = 31;                           /* 256 us / lan ngat (8MHz) */
    TIMSK |= (1 << OCIE0);
}

ISR(TIMER0_COMP_vect)
{
    static uint8_t cnt = 0;
    static uint8_t last = 0xFF;
    uint8_t out;

    cnt = (cnt + 1) & (PWM_STEPS - 1);

    if (g_dir == DIR_STOP || g_speed == 0)
        out = MOTOR_STOP;
    else if (g_speed >= PWM_STEPS || cnt < g_speed)
        out = (g_dir == DIR_FWD) ? MOTOR_FWD : MOTOR_REV;
    else
        out = MOTOR_STOP;

    if (out != last) {                    /* chi ghi khi thay doi */
        MOTOR = out;
        last = out;
    }
}

/* ================= LCD ================= */
/* Phai chan ngat: ISR ghi XMEM se lam doi A0 (RS) trong U5 truoc khi E xuong */
static void lcd_write(volatile uint8_t *reg, uint8_t v)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        *reg = v;                         /* chot vao U8, dong thoi chot A0 (RS) */
        __asm__ volatile("nop\n\tnop");
        PORTG |=  (1 << PG3);             /* E = 1 */
        _delay_us(1);
        PORTG &= ~(1 << PG3);             /* E = 0 */
    }
    _delay_us(50);
}

static void lcd_cmd(uint8_t c)  { lcd_write(&LCD_CMD, c); }
static void lcd_data(uint8_t d) { lcd_write(&LCD_DATA, d); }

static void lcd_init(void)
{
    DDRG  |=  (1 << PG3);
    PORTG &= ~(1 << PG3);
    _delay_ms(50);

    lcd_cmd(0x38); _delay_ms(5);
    lcd_cmd(0x38); _delay_us(150);
    lcd_cmd(0x38);
    lcd_cmd(0x0C);                        /* bat hien thi, tat con tro */
    lcd_cmd(0x06);                        /* tu tang dia chi */
    lcd_cmd(0x01); _delay_ms(2);          /* xoa man hinh */
}

static void lcd_goto(uint8_t row, uint8_t col)
{
    static const uint8_t base[4] = { 0x00, 0x40, 0x14, 0x54 };
    lcd_cmd(0x80 | (base[row & 3] + col));
}

static void lcd_print(const char *s)
{
    while (*s) lcd_data((uint8_t)*s++);
}

/* ================= Nut nhan ================= */
static uint8_t buttons_read(void)
{
    return (uint8_t)~BUTTON;              /* 1 = dang nhan */
}

/* ================= Giao dien ================= */
static void show_status(void)
{
    char buf[21];
    const char *d = (g_dir == DIR_FWD) ? "THUAN" : (g_dir == DIR_REV) ? "NGHICH" : "DUNG";

    snprintf(buf, sizeof buf, "%-20s", "DIEU KHIEN DONG CO");
    lcd_goto(0, 0); lcd_print(buf);

    snprintf(buf, sizeof buf, "Chieu: %-12s", d);
    lcd_goto(1, 0); lcd_print(buf);

    snprintf(buf, sizeof buf, "Toc do: %2u/%-9u", g_speed, PWM_STEPS);
    lcd_goto(2, 0); lcd_print(buf);
}

static void show_buttons(uint8_t now)
{
    static const char sym[8] = { 'L', 'R', 'U', 'D', 'G', '1', '2', '3' };
    char buf[21] = "Nut: ";
    for (uint8_t i = 0; i < 8; i++)
        buf[5 + i] = (now & (1 << i)) ? sym[i] : '-';
    for (uint8_t i = 13; i < 20; i++) buf[i] = ' ';
    buf[20] = '\0';
    lcd_goto(3, 0);
    lcd_print(buf);
}

/* ================= main ================= */
int main(void)
{
    uint8_t prev = 0, shown = 0xFF;

    xmem_init();
    motor_init();
    lcd_init();
    sei();

    show_status();

    for (;;) {
        uint8_t now   = buttons_read();
        uint8_t press = now & ~prev;      /* canh xuong */
        prev = now;

        if (press) {
            if (press & BTN_UP)    g_dir = DIR_FWD;
            if (press & BTN_DOWN)  g_dir = DIR_REV;
            if (press & BTN_GO)    g_dir = DIR_STOP;
            if ((press & BTN_LEFT)  && g_speed > 0)          g_speed--;
            if ((press & BTN_RIGHT) && g_speed < PWM_STEPS)  g_speed++;
            if (press & BTN_1)     g_speed = 4;
            if (press & BTN_2)     g_speed = 8;
            if (press & BTN_3)     g_speed = PWM_STEPS;
            show_status();
        }

        if (now != shown) {
            show_buttons(now);
            shown = now;
        }

        _delay_ms(20);                    /* chong doi + chu ky quet */
    }
}
