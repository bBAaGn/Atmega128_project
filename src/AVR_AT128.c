#include <avr/io.h>
#include <stddef.h> // for NULL
#include "userdef.h"
#define F_CPU 8000000UL
#include <util/delay.h>

unsigned long led = 0x55555555;
// 0~9, A-F common anode
unsigned char Segcode[]= {0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8, 0x80, 0x90, 0x88, 0x83, 0xC6, 0xA1, 0x86, 0x8E};

int led_display(unsigned long led){
  LED0 = led & 0x000000FF;
  LED1 = (led >> 8) & 0x000000FF;
  LED2 = (led >> 16) & 0x000000FF;
  LED3 = (led >> 24) & 0x000000FF;   
  return 0;   
}

int SevenSegDisplay(unsigned int value){
    LED7D1 = Segcode[value%10];
    LED7D2 = Segcode[(value/10)%10];
    LED7D3 = Segcode[(value/100)%10];
    LED7D4 = Segcode[(value/1000)%10];
    return 0;
}

int SevenSegDisplayScan(unsigned int value){
    
    LED7IDCTR = 0b11111110; 
    LED7IDDATA = Segcode[value%10];
    _delay_ms(50); 
    LED7IDCTR = 0xff;

    LED7IDCTR = 0b11111101; 
    LED7IDDATA = Segcode[(value/10)%10];
    _delay_ms(50);
    LED7IDCTR = 0xff;

    LED7IDCTR = 0b11111011; 
    LED7IDDATA = Segcode[(value/100)%10];
    _delay_ms(50);
    LED7IDCTR = 0xff;

    LED7IDCTR = 0b11110111;
    LED7IDDATA = Segcode[(value/1000)%10];
    _delay_ms(50);
    LED7IDCTR = 0xff;
}

int LCDInit(){
  _delay_ms(50);
  LCD_INS = 0x38; // Function set: 8-bit, 2 line, 5x7 dots
  LCDE_H; _delay_ms(2);
  LCDE_L; _delay_ms(2);
  _delay_ms(200);
  LCD_INS = 0x0C; // Display ON, Cursor OFF
  LCDE_H; _delay_ms(2);
  LCDE_L; _delay_ms(2);
  _delay_ms(200);
  LCD_INS = 0x06; // Entry mode set: Increment cursor
  LCDE_H; _delay_ms(2);
  LCDE_L; _delay_ms(2);
  _delay_ms(200);
  LCD_INS = 0x01; // Clear display
  LCDE_H; _delay_ms(2);
  LCDE_L; _delay_ms(2);
  _delay_ms(20);
}

void PrintL(const char *str, unsigned char line, unsigned char col){
  unsigned char add;
  switch(line){
    case 0: add = 0x80; break;
    case 1: add = 0xC0; break;
    case 2: add = 0x94; break;
    case 3: add = 0xD4; break;    
    default: add = 0x80; break;
  }
  LCD_INS = add + col; // Set DDRAM address
  LCDE_H; _delay_ms(2);
  LCDE_L; _delay_ms(2);
  while(*str != '\0'){
    LCD_DATA = *str++;
    LCDE_H; _delay_ms(2);
    LCDE_L; _delay_ms(2);
  }
}

void MenuDisplay(Menu *menu, unsigned char select){
  PrintL(menu->Title, 0, 0);
  PrintL(menu->List1, 1, 0);
  PrintL(menu->List2, 2, 0);
  PrintL(menu->List3, 3, 0);

  PrintL(">", select, 0);
}

char OneWireReset(void){
  char Presence;
  OneWire_Master;
  OneWire_Low;
  _delay_us(480); //minimum time to signal the start of an initialization cycle
  OneWire_FREE;
  _delay_us(80); //minimum time for DS18B20 to confirm the presence of a reset signal on the bus
  Presence = OneWire_State;
  _delay_us(60);
  _delay_us(400);
  return Presence;
} 

OneWireWriteByte(unsigned char byte){
  unsigned char i;
  for(i=0;i<8;i++){
    OneWire_Master;
    if(byte & 0x01){
      OneWire_Low;
      _delay_us(15);
      OneWire_FREE;
      _delay_us(45);
     // _delay_us(50);
    }
    else{
      OneWire_Low;
      _delay_us(15);
      _delay_us(45);
      OneWire_FREE;
      _delay_us(30);  
    }
    byte = byte>>1;
  }
}

unsigned char OneWireReadByte(void){
  unsigned char i;
  unsigned byte = 0;
  for(i=0;i<8;i++){
    OneWire_Master;
    OneWire_Low;
    _delay_us(2);
    OneWire_FREE;
    _delay_us(10);
    byte = byte>>1;
    byte = byte |(OneWire_State<<7);
    _delay_us(50);
  }
  return byte;
}

unsigned int ReadTemp(void){
  unsigned char data[2];
  if(!OneWireReset()){
    OneWireWriteByte(0xCC); // skip ROM command
    OneWireWriteByte(0x44);
    OneWire_FREE;
    _delay_ms(750); //resolution 12 need min 750;
    data[0] = OneWireReset();
    OneWireWriteByte(0xCC);
    OneWireWriteByte(0xBE); //Read ScratchPad command
    data[0] = OneWireReadByte();
    data[1] = OneWireReadByte();
    return (data[0] +data[1]*256)*0.0625; // the number switch 2 byte using resolution 12
  }
  else return 0;
}

//debug 1.8
// unsigned int ReadTempRaw(unsigned char *err){
//   unsigned char lsb, msb;
//   *err = 0;
//   if(OneWireReset()){ *err = 1; return 0; }   // reset lần 1 không có presence
//   OneWireWriteByte(0xCC);
//   OneWireWriteByte(0x44);
//   _delay_ms(750);
//   if(OneWireReset()){ *err = 2; return 0; }   // reset lần 2 không có presence
//   OneWireWriteByte(0xCC);
//   OneWireWriteByte(0xBE);
//   lsb = OneWireReadByte();
//   msb = OneWireReadByte();
//   return ((unsigned int)msb << 8) | lsb;
// }


int main(void){
  MCUCR |= 0x80; // Enable external memory interface
  //Part 1.5
  DDRG = 0x0f; // Set PG3 as output for LCD Enable
  XMCRA = 0;

  // Part 1.2 test
  // for(int i = 0; i < 4; i++){
  //   led_display(led);
  //   led = ~led;
  //   _delay_ms(200);
  // }

  // Part 1.3 test
  // SevenSegDisplay(1234);
  
  // Part 1.5 test
  // LCDInit();
  // PrintL("Hello, World!", 0, 0);

  //Part 1.4 test
  // Relays.relayall = 0x55;
  // Relay_Activate;

  //Part 1.5 test
  // Motors.DC1 = FORWARD;
  // Motors.DC2 = BACKWARD;
  // Motors_Activate;

  //Part 1.6
  unsigned char select = 1;
  Menu *menu = &MainMenu;
  LCDInit();
  MenuDisplay(menu, select);

  //Part 1.8 test
  unsigned int Temp;
  unsigned char data;
  // Menu *menu = &MainMenu;
  // LCDInit();
  // PrintL("Test One-Wire",0,0);
  // if(!OneWireReset())
  //   PrintL("DS18B20 is found",1,0);
  // else
  //   PrintL("DS18B20 not found",1,0);

  // OneWireWriteByte(0x33);  
  // data = OneWireReadByte();
  // SevenSegDisplay(data);

  while(1){ 

  // Part 1.5
  // ReadKey;
  // if(!Buttons.LEFT) Motors.DC1 = FORWARD;
  // else if(!Buttons.RIGHT) Motors.DC1 = BACKWARD;
  // Motors_Activate;

  // Part 1.3  
  //  SevenSegDisplayScan(1234); 

  // Part 1.4
  // Relays.RL0 = 1;
  // Relays.RL5 = 1;
  // Relay_Activate;

  //Part 1.6 & 1.7
    ReadKey;
    if(!Buttons.UP){
      _delay_ms(20); // Chống dội khi mới nhấn (debounce)
      ReadKey;
      if(!Buttons.UP){ // Xác nhận phím vẫn được nhấn
        select = (select == 1) ? 3 : select - 1;
        MenuDisplay(menu, select);          
        // Chờ người dùng thả nút ra (tránh việc giữ nút bị lặp liên tục quá nhanh)
        while(!Buttons.UP){
          ReadKey;
        }
        _delay_ms(20); // Chống dội khi nhả nút
      }
    }
    if(!Buttons.DOWN){ 
      _delay_ms(20); // Chống dội khi mới nhấn
      ReadKey;
      if(!Buttons.DOWN){ // Xác nhận phím vẫn được nhấn
        select = (select == 3) ? 1 : select + 1;
        MenuDisplay(menu, select);            
        // Chờ người dùng thả nút ra
        while(!Buttons.DOWN){
          ReadKey;
        }
        _delay_ms(20); // Chống dội khi nhả nút
      }
    }   
    if(!Buttons.RIGHT){
      _delay_ms(20); // Chống dội
      ReadKey;
      if(!Buttons.RIGHT){
        switch(select){
        case 1: 
          if(menu->Menulist1 != NULL) { menu = menu->Menulist1; select = 1; } 
          break;
        case 2: 
          if(menu->Menulist2 != NULL) { menu = menu->Menulist2; select = 1; } 
          break;
        case 3: 
          if(menu->Menulist3 != NULL) { menu = menu->Menulist3; select = 1; } 
          break;
        }
        MenuDisplay(menu, select);      
        // Chờ người dùng thả nút ra
        while(!Buttons.RIGHT){
        ReadKey;
        }
      _delay_ms(20);
      }
    } 
    if(!Buttons.LEFT){
      _delay_ms(20);
      ReadKey;
      if(!Buttons.LEFT){
        if(menu->pre != NULL){
        menu = menu->pre;
        select = 1; // Reset lại dòng chọn khi quay lại menu trước
        }
        MenuDisplay(menu, select);     
        // Chờ người dùng thả nút ra
        while(!Buttons.LEFT){
        ReadKey;
        }
        _delay_ms(20);
      }
    }
    if(!Buttons.GO){
      while(!Buttons.GO){
        ReadKey;
        switch (select){
          case 1: 
            if(menu->ActivationON != NULL) menu->ActivationON(menu->MenuID,ON);
            break;
          case 2: 
            if(menu ->ActivationOFF != NULL) menu ->ActivationOFF(menu->MenuID,OFF);
            break;
        }
      }
    } 
    
    //Part 1.8 test
    // Temp = ReadTemp();
    // SevenSegDisplay(Temp);

    //Part 1.8 debug
    // unsigned char err;
    // unsigned int raw = ReadTempRaw(&err);
    // LED7D1 = Segcode[raw & 0x0F];
    // LED7D2 = Segcode[(raw >> 4) & 0x0F];
    // LED7D3 = Segcode[(raw >> 8) & 0x0F];
    // LED7D4 = Segcode[err];
  }
}  
