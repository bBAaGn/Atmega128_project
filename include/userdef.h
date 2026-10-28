//Part 1.7
#define ON 1
#define OFF 0

//Part 1.2
#define EXMEM_ADD 0x1100
//CS offset defination
#define CS0 0 // Chip Select 0
#define CS1 1 // Chip Select 1
#define CS2 2 // Chip Select 2
#define CS3 3 // Chip Select 3
#define CS4 4 // Chip Select 4
#define CS5 5 // Chip Select 5
#define CS6 6 // Chip Select 6
#define CS7 7 // Chip Select 7
#define CS8 8 // Chip Select 8
#define CS9 9 // Chip Select 9
#define CS10 10 // Chip Select 10
#define CS11 11 // Chip Select 11
#define CS12 12 // Chip Select 12
#define CS13 13 // Chip Select 13
#define CS14 14 // Chip Select 14
#define CS15 15 // Chip Select 15
#define CS16 16 // Chip Select 16
#define CS17 17 // Chip Select 17
#define CS18 18 // Chip Select 18
#define CS19 19 // Chip Select 19


#define LCDE_H (PORTG |= (1<<3)) // LCD Enable High
#define LCDE_L (PORTG &= ~(1<<3)) // LCD Enable Low

#define LED0 *(unsigned char*) (EXMEM_ADD + CS0) 
#define LED1 *(unsigned char*) (EXMEM_ADD + CS1) 
#define LED2 *(unsigned char*) (EXMEM_ADD + CS2) 
#define LED3 *(unsigned char*) (EXMEM_ADD + CS3) 
  
#define LED7D1 *(unsigned char *)(EXMEM_ADD + CS4) 
#define LED7D2 *(unsigned char *)(EXMEM_ADD + CS5)
#define LED7D3 *(unsigned char *)(EXMEM_ADD + CS6)
#define LED7D4 *(unsigned char *)(EXMEM_ADD + CS7)

#define LED7IDCTR *(unsigned char *)(EXMEM_ADD + CS8)
#define LED7IDDATA *(unsigned char *)(EXMEM_ADD + CS9)
#define RelayMap *(unsigned char *)(EXMEM_ADD + CS10)


#define LCD_DATA *(unsigned char *)(EXMEM_ADD + CS11)
#define LCD_INS  *(unsigned char *)(EXMEM_ADD + CS12)
#define MotorDC *(unsigned char *)(EXMEM_ADD + CS13)
#define BUTTON *(unsigned char *)(EXMEM_ADD + CS14)

#define Relay_Activate (*( volatile unsigned char *)(EXMEM_ADD + CS10) = *( unsigned char *) (&Relays))
#define Motors_Activate (*(volatile unsigned char *)(EXMEM_ADD + CS13) = *( unsigned char *)(&Motors))
#define ReadKey (*(unsigned char *)(&Buttons) = *(volatile unsigned char *)(EXMEM_ADD + CS14))

#define DS18B20 0
#define OneWire_High (PORTB |= (1<<DS18B20))
#define OneWire_Low (PORTB &= ~(1<<DS18B20))
#define OneWire_Master (DDRB |= (1<<DS18B20))
#define OneWire_FREE (DDRB &= ~(1<<DS18B20))
#define OneWire_State (PINB & (1<<DS18B20))

#define STOP 0
#define FORWARD 1
#define BACKWARD 2

#define Device_Relay 0
#define Device_Motor1 1
#define Device_Motor2 2
#define Device_LED 3
#define Device_DS18B20 4

//Khai báo prototype thêm cho hàm ReadTemp
unsigned int ReadTemp(void);
int SevenSegDisplay(unsigned int value);

struct bits{
    unsigned char RL0 : 1;
    unsigned char RL1 : 1;
    unsigned char RL2 : 1;
    unsigned char RL3 : 1;
    unsigned char RL4 : 1;
    unsigned char RL5 : 1;
    unsigned char RL6 : 1;
    unsigned char RL7 : 1;        
};    

union RL{
    unsigned char relayall;
    struct bits relay;
};
union RL Relays;
// struct bits Relays;


struct Motor{
    unsigned char DC1 : 2;
    unsigned char DC2 : 2;
    unsigned char DCFree : 4;
};    
struct Motor Motors;

struct Button{
    unsigned char LEFT : 1;
    unsigned char RIGHT : 1;
    unsigned char UP : 1;
    unsigned char DOWN : 1;
    unsigned char GO : 1;
    unsigned char BTN1 : 1;
    unsigned char BTN2 : 1;
    unsigned char BTN3 : 1;        
};
struct Button Buttons;

typedef struct Linker{
    char MenuID;
    struct Linker *pre;
    char Title[20];
  // Hai dòng dưới là con trỏ kiểu cấu trúc, có thể dùng để trỏ đến các menu con hoặc các hàm xử lý
    char List1[20];
    struct Linker *Menulist1;   void (*ActivationON)(char, char); //Function pointer will point to a funnction which have 2 variables char
  // Tương tự, có thể khai báo thêm các con trỏ hàm khác cho List2 và List3 nếu muốn
    char List2[20];
    struct Linker *Menulist2;   void (*ActivationOFF)(char, char);  
    char List3[20];
    struct Linker *Menulist3;   
}Menu;
Menu MainMenu, SensorMenu, ActuatorMenu, SettingMenu, TempMenu, HumidityMenu, MotorsMenu, Motor1Menu, Motor2Menu, RelaysMenu, LedsMenu;

void ActuatorActivate(unsigned char Devices, unsigned char Status){
    switch(Devices){
        case Device_Relay:
            if(Status == ON) Relays.relayall = 0xff;
            else Relays.relayall = 0x00;
            Relay_Activate;
            break;
        case Device_Motor1:
            Motors.DC1 = Status; Motors_Activate;
            break;
        case Device_Motor2:
            Motors.DC2 = Status; Motors_Activate;
            break;
        case Device_LED:
            if(Status == ON) LED0 = 0xff;
            else LED0 = 0x00;
            break;
        case Device_DS18B20:
            if(Status == ON) SevenSegDisplay(ReadTemp());
            else { LED7D1 = 0xFF; LED7D2 = 0xFF; LED7D3 = 0xFF; LED7D4 = 0xFF; }
            break;    
    }
}

Menu MainMenu = {
    NULL,
    NULL,
    "      MAIN MENU     ",
    "     Sensors        ", &SensorMenu, NULL,//Nạp địa chỉ của menu con SensorMenu vào con trỏ Menulist1
    "     Actuator       ", &ActuatorMenu, NULL,
    "     Setting        ", &SettingMenu,
};

/*---------------------------------------*/

Menu SensorMenu = {
    NULL,
    &MainMenu,
    "      SENSORS       ",
    "     Temperature    ", &TempMenu, NULL,
    "     Humidity       ", &HumidityMenu, NULL,
    "     Date & Time    ", NULL,
};

Menu TempMenu = {
    Device_DS18B20,
    &SensorMenu,
    "    TEMPERATURE     ",
    "   ON               ", NULL, &ActuatorActivate,
    "   OFF              ", NULL, &ActuatorActivate,
    "   None             ", NULL,
};

Menu HumidityMenu = {
    NULL,
    &SensorMenu,
    "     HUMIDITY       ",
    "   ON               ", NULL, NULL,
    "   OFF              ", NULL, NULL,
    "   None             ", NULL,
};

/*---------------------------------------*/

Menu ActuatorMenu = {
    NULL,
    &MainMenu,
    "     ACTUATORS      ",
    "    Motors          ", &MotorsMenu, NULL,
    "    Relays          ", &RelaysMenu, NULL,
    "    LEDs            ", &LedsMenu,
};

Menu MotorsMenu = {
    NULL,
    &ActuatorMenu,
    "      MOTORS        ",
    "     Motor 01       ", &Motor1Menu, NULL,
    "     Motor 02       ", &Motor2Menu, NULL,
    "     None           ", NULL,
};

// Khai báo cho Menu Motor 1 và Motor 2
Menu Motor1Menu = {
    Device_Motor1,
    &MotorsMenu,
    "     MOTOR 01       ",
    "   ON               ", NULL, &ActuatorActivate,
    "   OFF              ", NULL, &ActuatorActivate,
    "   None             ", NULL,
};

Menu Motor2Menu = {
    Device_Motor2,
    &MotorsMenu,
    "     MOTOR 02       ",
    "   ON               ", NULL, &ActuatorActivate,
    "   OFF              ", NULL, &ActuatorActivate,
    "   None             ", NULL,
};

Menu RelaysMenu = {
    Device_Relay,
    &ActuatorMenu,
    "      RELAYS        ",
    "   ON               ", NULL, &ActuatorActivate,
    "   OFF              ", NULL, &ActuatorActivate,
    "   None             ", NULL,
};

Menu LedsMenu = {
    Device_LED,
    &ActuatorMenu,
    "      LEDS          ",
    "   ON               ", NULL, &ActuatorActivate,
    "   OFF              ", NULL, &ActuatorActivate,
    "   None             ", NULL,
};

/*---------------------------------------*/

Menu SettingMenu = {
    NULL,
    &MainMenu,
    "      SETTING       ",
    "    Time            ", NULL, NULL,
    "    Speed           ", NULL, NULL,
    "    Channels        ", NULL,
};
