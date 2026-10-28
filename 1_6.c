/*
 * ATmega128 - Dieu khien dong co DC + Menu tuong tac tren LCD
 * Ap dung dung cau truc "danh sach lien ket" trong bai P1.6 (Truong Ngoc Son)
 * https://youtu.be/rE9QHrk5K4Q
 *
 * Kien truc phan cung: External Memory (XMEM) + 74HC573 + 74154
 *   0x110B : LCD DATA   (CS11, A0=1 -> RS=1)
 *   0x110C : LCD CMD    (CS12, A0=0 -> RS=0)
 *   0x110D : MOTOR      (CS13) bit0=DC1A, bit1=DC1B
 *   0x110E : BUTTON     (CS14) doc, tich cuc muc thap
 *   PG3    : chan E cua LCD
 *
 * Dieu khien menu:
 *   UP/DOWN : "Next"/"Pre" - di chuyen giua List1/List2/List3 cua menu hien tai
 *   RIGHT   : chon muc dang highlight - neu MenuListX != NULL thi di vao,
 *             neu MenuListX == NULL thi day la muc la, thuc thi hanh dong
 *   LEFT    : quay ve menu cha (theo con tro pre)
 */
#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <util/atomic.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>   /* NULL */

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

#define PWM_STEPS  16

typedef enum { DIR_STOP = 0, DIR_FWD, DIR_REV } dir_t;

static volatile dir_t   g_dir   = DIR_STOP;
static volatile uint8_t g_speed = 8;

/* ================= XMEM ================= */
static void xmem_init(void)
{
    MCUCR |= (1 << SRE);
}

/* ================= PWM mem cho dong co (Timer0) ================= */
static void motor_init(void)
{
    MOTOR = MOTOR_STOP;
    TCCR0 = (1 << WGM01) | (1 << CS02);
    OCR0  = 31;
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

    if (out != last) {
        MOTOR = out;
        last = out;
    }
}

/* ================= LCD ================= */
static void lcd_write(volatile uint8_t *reg, uint8_t v)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        *reg = v;
        __asm__ volatile("nop\n\tnop");
        PORTG |=  (1 << PG3);
        _delay_us(1);
        PORTG &= ~(1 << PG3);
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
    lcd_cmd(0x0C);
    lcd_cmd(0x06);
    lcd_cmd(0x01); _delay_ms(2);
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

/* ================= Nut nhan (co chong doi bang phan mem) ================= */
static uint8_t buttons_read_raw(void)
{
    return (uint8_t)~BUTTON;
}

static uint8_t buttons_read(void)
{
    uint8_t a = buttons_read_raw();
    _delay_ms(3);
    uint8_t b = buttons_read_raw();

    /* Doc lai cho toi khi 2 lan doc lien tiep giong nhau (tin hieu da on dinh) */
    uint8_t tries = 0;
    while (a != b && tries < 10) {
        a = b;
        _delay_ms(3);
        b = buttons_read_raw();
        tries++;
    }
    return b;
}

/* ================= MENU: danh sach lien ket (dung nhu bai giang) ================= */
/*
 * Dung dung ten field nhu slide: pre, Title, List1/MenuList1, List2/MenuList2,
 * List3/MenuList3. LCD chi hien thi duoc 20 cot nen dung [21] de co cho ky tu NULL.
 */
typedef struct linker {
    struct linker *pre;
    char Title[21];
    char List1[21];
    struct linker *MenuList1;
    char List2[21];
    struct linker *MenuList2;
    char List3[21];
    struct linker *MenuList3;
} Menu;

/* Khai bao truoc de cac menu tro cheo lan nhau, giong slide */
extern Menu MainMenu, DKMenu, TDMenu, TTMenu;

Menu MainMenu = {
    NULL,
    "MAIN MENU",
    "Dieu khien DC", &DKMenu,
    "Cai dat toc do", &TDMenu,
    "Trang thai",     &TTMenu
};

Menu DKMenu = {
    &MainMenu,
    "DIEU KHIEN DC",
    "Quay thuan",  NULL,
    "Quay nghich", NULL,
    "Dung",        NULL
};

Menu TDMenu = {
    &MainMenu,
    "CAI DAT TOC DO",
    "Cham (4)",    NULL,
    "Vua (8)",     NULL,
    "Nhanh (16)",  NULL
};

Menu TTMenu = {
    &MainMenu,
    "TRANG THAI",
    "Xem hien tai", NULL,
    "",             NULL,
    "",             NULL
};

typedef enum { MODE_MENU = 0, MODE_STATUS } mode_t;

static Menu    *g_cur    = &MainMenu;
static uint8_t  g_cursor = 0;          /* 0=List1, 1=List2, 2=List3 */
static mode_t   g_mode   = MODE_MENU;

/* Truy cap List/MenuList theo chi so, vi cac field khong phai mang */
static const char *item_text(Menu *m, uint8_t idx)
{
    switch (idx) {
        case 0: return m->List1;
        case 1: return m->List2;
        default: return m->List3;
    }
}

static Menu *item_sub(Menu *m, uint8_t idx)
{
    switch (idx) {
        case 0: return m->MenuList1;
        case 1: return m->MenuList2;
        default: return m->MenuList3;
    }
}

static uint8_t first_valid(Menu *m)
{
    for (uint8_t i = 0; i < 3; i++)
        if (item_text(m, i)[0] != '\0') return i;
    return 0;
}

static uint8_t next_valid(Menu *m, uint8_t idx)
{
    uint8_t i = idx;
    for (uint8_t k = 0; k < 3; k++) {
        i = (i + 1) % 3;
        if (item_text(m, i)[0] != '\0') return i;
    }
    return idx;
}

static uint8_t prev_valid(Menu *m, uint8_t idx)
{
    uint8_t i = idx;
    for (uint8_t k = 0; k < 3; k++) {
        i = (i == 0) ? 2 : (uint8_t)(i - 1);
        if (item_text(m, i)[0] != '\0') return i;
    }
    return idx;
}

/* Muc la (MenuListX == NULL): quyet dinh hanh dong bang cach so sanh ten muc */
static void run_leaf_action(const char *label)
{
    if      (strcmp(label, "Quay thuan")  == 0) g_dir = DIR_FWD;
    else if (strcmp(label, "Quay nghich") == 0) g_dir = DIR_REV;
    else if (strcmp(label, "Dung")        == 0) g_dir = DIR_STOP;
    else if (strcmp(label, "Cham (4)")    == 0) g_speed = 4;
    else if (strcmp(label, "Vua (8)")     == 0) g_speed = 8;
    else if (strcmp(label, "Nhanh (16)")  == 0) g_speed = PWM_STEPS;
    else if (strcmp(label, "Xem hien tai")== 0) g_mode = MODE_STATUS;
}

static void menu_draw(Menu *m, uint8_t cursor)
{
    char buf[21];

    snprintf(buf, sizeof buf, "%-20s", m->Title);
    lcd_goto(0, 0); lcd_print(buf);

    for (uint8_t i = 0; i < 3; i++) {
        const char *txt = item_text(m, i);
        if (txt[0] == '\0')
            snprintf(buf, sizeof buf, "%-20s", "");
        else
            snprintf(buf, sizeof buf, "%c%-19s", (i == cursor) ? '>' : ' ', txt);
        lcd_goto(i + 1, 0); lcd_print(buf);
    }
}

static const char *dir_name(void)
{
    return (g_dir == DIR_FWD) ? "THUAN" : (g_dir == DIR_REV) ? "NGHICH" : "DUNG";
}

static void status_draw(void)
{
    char buf[21];

    snprintf(buf, sizeof buf, "%-20s", "TRANG THAI DONG CO");
    lcd_goto(0, 0); lcd_print(buf);

    snprintf(buf, sizeof buf, "Chieu : %-12s", dir_name());
    lcd_goto(1, 0); lcd_print(buf);

    snprintf(buf, sizeof buf, "Toc do: %2u/%-9u", g_speed, PWM_STEPS);
    lcd_goto(2, 0); lcd_print(buf);

    snprintf(buf, sizeof buf, "%-20s", "LEFT: Quay lai");
    lcd_goto(3, 0); lcd_print(buf);
}

/* ================= main ================= */
int main(void)
{
    uint8_t prev = 0;

    xmem_init();
    motor_init();
    lcd_init();
    sei();

    g_cursor = first_valid(g_cur);
    menu_draw(g_cur, g_cursor);

    for (;;) {
        uint8_t now   = buttons_read();
        uint8_t press = now & ~prev;
        prev = now;

        if (g_mode == MODE_MENU) {
            uint8_t redraw = 0;

            if (press & BTN_UP)   { g_cursor = prev_valid(g_cur, g_cursor); redraw = 1; }
            if (press & BTN_DOWN) { g_cursor = next_valid(g_cur, g_cursor); redraw = 1; }

            if ((press & BTN_RIGHT) && item_text(g_cur, g_cursor)[0] != '\0') {
                Menu *sub = item_sub(g_cur, g_cursor);
                if (sub != NULL) {
                    g_cur    = sub;
                    g_cursor = first_valid(g_cur);
                } else {
                    const char *label = item_text(g_cur, g_cursor);
                    uint8_t was_status_pick = (strcmp(label, "Xem hien tai") == 0);
                    run_leaf_action(label);
                    if (!was_status_pick && g_cur->pre != NULL) {
                        g_cur    = g_cur->pre;   /* thuc hien xong tu quay ve menu cha */
                        g_cursor = first_valid(g_cur);
                    }
                }
                redraw = 1;
            }

            if ((press & BTN_LEFT) && g_cur->pre != NULL) {
                g_cur    = g_cur->pre;
                g_cursor = first_valid(g_cur);
                redraw = 1;
            }

            if (redraw) menu_draw(g_cur, g_cursor);
        } else { /* MODE_STATUS */
            status_draw();
            if (press & BTN_LEFT) {
                g_mode = MODE_MENU;
                menu_draw(g_cur, g_cursor);
            }
        }

        _delay_ms(40);
    }
}
