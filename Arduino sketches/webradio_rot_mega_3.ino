// Demo using DHCP and DNS to perform a web client request.
// 2011-06-08 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

#include <EtherCard.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>


#include <MemoryFree.h>
long memOutTime = 0;

long resetTime = 60000*240; //restart interface after 240 minutes (preventing memory leaks)
void(* resetFunc) (void) = 0; //declare reset function @ address 0

//VARS_BEGIN => LCD
LiquidCrystal_I2C lcd(0x20, 6, 5, 4, 0, 1, 2, 3, 7, NEGATIVE);
String msg;
String msg2;
char lcd_line1[16];
char lcd_line2[16];

char p_buffer[256];
#define P(str) (strcpy_P(p_buffer, PSTR(str)), p_buffer)
//VARS_BEGIN => LCD


//VARS_BEGIN => Ethernet
static uint32_t blockReq = 0;
byte Ethernet::buffer[2048];
static uint32_t timer = 0;
static uint32_t timer_lcd;
static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };
static byte myip[] = { 192,168,1,122 };
static byte gwip[] = { 192,168,1,1 };
static byte dnsip[] = { 8,8,8,8 };
//VARS_END => Ethernet


//VARS_BEGIN => Rotary

//these pins can not be changed 2/3 are special pins
enum PinAssignments {
  encoderPinA = 3,   // rigth
  encoderPinB = 2,   // left
  buttonPin = 19    // another two pins
};

long lastDebounceTime = 0;
long lastDebounceTimeRot = 0;

volatile unsigned int encoderPos = 0;  // a counter for the dial
unsigned int lastReportedPos = 1;   // change management

// interrupt service routine vars
boolean A_set = false;              
boolean B_set = false;
boolean C_set = false;
//VARS_END => Rotary


//VARS_BEGIN Menu
char playstate = '0';

char station_title[13] = "0";
int station_title_id = 0;
int station_title_id_req = 0;
int station_count = 5;
int station_state = 1;

int web_state = 3;  
//0 = load song info
//1 = load ip info

int menu_state = 0;  
//0 = show song info
//1 = show menu -> play/stop
//2 = show menu -> stations
//3 = show menu -> ipinfo
//4 = show menu -> verlassen
//5 = show menu -> ipinfo -> show ip
//6 = show menu -> ipinfo -> zurueck

String menu_line1 = '\0';
String menu_line2 = '\0';
char menu_ip[16] = "0";
//VARS_END Menu

// called when the client request is complete
static void my_callback (byte status, word off, word len) {
  msg = '\0';
  msg2 = '\0';

  Serial.println(P(">>>"));

  Serial.println((const char*) Ethernet::buffer + 302);
   

  msg=String(((const char*) Ethernet::buffer + 302));
  
  int beginMsg = msg.indexOf(P("lighttpd/1.4.30"));
  beginMsg = beginMsg < 1 ? 0 : beginMsg+15;
  Serial.print(P("offset: ")); Serial.println(beginMsg);
  msg = msg.substring(beginMsg);
  msg.trim();
  Serial.print(P("msg"));  Serial.println(msg);
  if (msg.startsWith(P("<0>")))
  {
    Serial.print(P("msg-g: "));Serial.println(msg);
    Serial.print(P("msg2-g: "));Serial.println(msg2);
    
    //Set playlist count
    int pstate = msg.indexOf(P("<|||>"))+5;
    msg2 = msg.substring(pstate, msg.indexOf(P("<||>")));    
    char buf_count[msg2.length()+1];
    msg2.toCharArray(buf_count, msg2.length()+1);
    msg2 = '\0';
    station_count = atoi(buf_count);
    
    //Set playstate
    msg2 = msg.substring(3,msg.indexOf(P("<|||>")));
    if (msg2.startsWith(P("play")))
    {  
      
      if (playstate != '1')
      {
         playstate = '1';
      }
      if (menu_state == 7 || menu_state == 8 || menu_state == 9 || menu_state == 12)
      {
        menu_state = 0;
      }
    }
    else
    {
      if (playstate != '0')
      {
        playstate = '0';        
      }
      if (menu_state == 7 || menu_state == 8 || menu_state == 0 || menu_state == 12)
      { 
        menu_state = 9;
      }
    }
    
    //Set radio title and name
    msg2 = msg.substring(msg.indexOf(P("<|>")) + 3);
    msg = msg.substring(msg.indexOf(P("<||>"))+4, msg.indexOf(P("<|>")));
    msg += P("                ");
    
    Serial.println(sizeof(msg));
    Serial.print(P("msg-ready: "));Serial.println(msg);
    Serial.print(P("msg2-ready: "));Serial.println(msg2);
  }
  else if (msg.startsWith(P("<1>")))
  {
    //Serial.print(P("recovered ip: ")); Serial.println(msg);
    web_state = 0;
    msg = msg.substring(3);  
    msg.trim();   
    msg.toCharArray(menu_ip, 16);
    //Serial.print(P("menu_ip: "));Serial.println(menu_ip);
    msg = '\0';
    msg2 = '\0';
  }
  else if (msg.startsWith(P("<2>")))
  {
    if (web_state != 6)
    {web_state = 6;
 blockReq = millis(); }
    else
    {web_state = 0;}
    msg = msg.substring(3);  
    msg.trim();   
    msg2 = msg.substring(msg.indexOf(P("<|>")) + 3);
    msg = msg.substring(0, msg.indexOf(P("<|>")));
    char buf_count[msg2.length()+1];
    msg2.toCharArray(buf_count, msg2.length()+1);
    msg2 = '\0';
    station_count = atoi(buf_count);
    if (msg.startsWith(P("play")))
    {  
      playstate = '1';
      if (menu_state == 7 || menu_state == 8 || menu_state == 9 || menu_state == 12)
      {
        menu_state = 0;
      }
    }
    else
    {
      
      playstate = '0';
      if (menu_state == 7 || menu_state == 8 || menu_state == 0 || menu_state == 12)
      { 
        menu_state = 9;
      }
      //menu_state = 7; 
      //web_state = 2; 
    }
    
    msg = '\0';
    msg2 = '\0';
    blockReq = 0;
  }
  else if (msg.startsWith(P("<4>")))
  {
    web_state = 0;    
    
    msg = msg.substring(3);  
    //msg.trim();
    
    msg2 = msg.substring(0,msg.indexOf(P(":")));
    
    char buf_count[msg2.length()+1];
    msg2.toCharArray(buf_count, msg2.length()+1);
    station_title_id = atoi(buf_count);
    
    msg.toCharArray(station_title, 13);
    //Serial.print(P("title:"));Serial.println(station_title);
    msg = '\0';
    msg2 = '\0';
  }
  //Serial.println(P("..."));
}

void setup () {
  
  Serial.begin(57600);
    
  //INIT_BEGIN Lcd
  lcd.begin(16, 2);
  lcd.clear();
  lcd.backlight(); 
  
  lcd.setCursor(0, 0);
  lcd.print(P("Loading"));
  lcd.setCursor(0, 1);
  lcd.print(P("Controller..."));
  //INIT_END Lcd  
  
  //INIT_BEGIN Ethernet
  Serial.println(P("\n[webClient]"));
  if (ether.begin(sizeof Ethernet::buffer, mymac, 53) == 0) 
  {    
    lcd.setCursor(0, 0);
    lcd.clear();
    lcd.print(P("Ethernet error"));
    Serial.println(P("Failed to access Ethernet controller")); 
  }
  else
  {
     Serial.println(P("Ethernet is fine and up")); 
  }
   // if (!ether.dhcpSetup())
   //   Serial.println("DHCP failed");
  ether.staticSetup(myip, gwip, dnsip);
  ether.hisip[0] = 192; ether.hisip[1] = 168; ether.hisip[2] = 1; ether.hisip[3] = 1; 
  delay(500);
  //INIT_END Ethernet
  
  Serial.println(P("Set up ipconfig"));
  
  //INIT_BEGIN Rotary
  pinMode(encoderPinA, INPUT); 
  pinMode(encoderPinB, INPUT); 
  pinMode(buttonPin, INPUT); 
  digitalWrite(encoderPinA, HIGH);
  digitalWrite(encoderPinB, HIGH);
  digitalWrite(buttonPin, HIGH);
  
    Serial.println(P("Set up pins"));  
    
  //encoder pin on interrupt 1 (pin 3)
  attachInterrupt(1, doEncoderA, CHANGE);
  //encoder pin on interrupt 0 (pin 2)
  attachInterrupt(0, doEncoderB, CHANGE);
  attachInterrupt(4, pin3func, CHANGE);
  
    Serial.println(P("Set up natural interrupts"));
  //INIT_END Rotary
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(P("Waiting for"));
  lcd.setCursor(0, 1);
  lcd.print(P("Radio-Server..."));
    Serial.println(P("Set up lcd"));
}

void loop () {
  if (millis() > resetTime)
  { resetFunc(); }
  //Receive Web Data
  ether.packetLoop(ether.packetReceive());
 
 /*
 if ((memOutTime+2000) < millis())
 {
    memOutTime = millis();
    Serial.print("freeMemory()=");
    Serial.println(freeMemory());
 }
 */

 
  if (menu_state == 0){
    //Print Songinfo -> LCD
    lcdOut();
  }
  
  if (menu_state == 1)
  {      
    lcdOutMenu1(P("Menu"));    
    lcdOutMenu2(P("< 1.Play/Stop  >"));
  }  
  if (menu_state == 2)
  {
    lcdOutMenu1(P("Menu"));    
    lcdOutMenu2(P("< 2.Station    >"));
  }
  if (menu_state == 3)
  {
    lcdOutMenu1(P("Menu"));    
    lcdOutMenu2(P("< 3.Info       >"));
  }
  if (menu_state == 4)
  {
    lcdOutMenu1(P("Menu"));    
    lcdOutMenu2(P("< 4.Exit menu  >"));
  }
  if (menu_state == 5)
  {
    lcdOutMenu1(P("Menu->IP Info"));     
    lcdOutMenu2((char*)((menu_ip[0] == P("0")[0]) ? P("< 1.Load IP... >") : menu_ip));
  }
  if (menu_state == 6)
  {
    lcdOutMenu1(P("Menu->IP Info"));    
    lcdOutMenu2(P("< 2.Back       >"));
  }
  if (menu_state == 7)
  {
    lcdOutMenu1(P("Please wait!"));    
    lcdOutMenu2(P("Loading player  "));
  }
  if (menu_state == 8)
  {
    lcdOutMenu1(P("Please wait!"));    
    lcdOutMenu2(P("Loading songinfo"));
    if (playstate=='1')
    {
      menu_state = 0;
    }
    else
    {
      menu_state = 9;
    }
  }
  if (menu_state == 9)
  {
    lcdOutMenu1(P("Status: Radio"));    
    lcdOutMenu2(P("is stopped."));
  }
  if (menu_state == 10)
  {
    lcdOutMenu1(P("Menu->Station"));
    
    if ((station_title[0] == P("0")[0]) || (station_state != station_title_id))
    {
      String station_line = String(P("< "));
      station_line += station_state;
      station_line += ": ";
      while (station_line.length() < 15)
      { station_line += " "; }
      station_line += ">";
      char buf_station[17];
      station_line.toCharArray(buf_station, 17);
      station_line = '\0';
      lcdOutMenu2(buf_station);
    }
    else
    {
      lcdOutTitle(station_title);
    }  
   
  }
  if (menu_state == 11)
  {
    lcdOutMenu1(P("Menu->Station"));     
    lcdOutMenu2(P("< 2.Back       >"));
  }
  if (menu_state == 12)
  {
    lcdOutMenu1(P("Please wait!"));    
    lcdOutMenu2(P("Loading station "));
  }

    
  //Refresh Songdata
  if (millis() > timer) {
    timer = millis() + 1000;
    //Serial.println();
    if (web_state == 0)// && playstate == '1')
    {
      //Serial.print(P("<<< REQ songinfo "));
      ether.browseUrl(PSTR("/?"), "cmd=status", PSTR("192.168.1.1"), my_callback);
    }
    else if (web_state == 1)
    {
      //Serial.print(P("<<< REQ ipinfo "));
      ether.browseUrl(PSTR("/?"), "cmd=ipinfo", PSTR("192.168.1.1"), my_callback);
    }
    else if (web_state == 2)
    {
      //Serial.print(P("<<< REQ autoplaystop "));
      ether.browseUrl(PSTR("/?"), "cmd=autoplaystop", PSTR("192.168.1.1"), my_callback);
    }
    else if (web_state == 3 && (blockReq == 0 || blockReq+4000<millis()))
    {
      blockReq = millis();
      //Serial.print(P("<<< REQ playstate "));
      ether.browseUrl(PSTR("/?"), "cmd=playstate", PSTR("192.168.1.1"), my_callback);
    }
    else if (web_state == 4)
    {
      //Serial.print(P("<<< REQ station title "));      
      if (station_title_id_req > 99)
      {
        char req[17];      
        sprintf(req, P("cmd=plitem&i=%i"), station_title_id_req);
        ether.browseUrl(PSTR("/?"), req, PSTR("192.168.1.1"), my_callback);
      }
      else if ((station_title_id_req > 9 && station_title_id_req < 100))
      {
        char req[16];      
        sprintf(req, P("cmd=plitem&i=%i"), station_title_id_req);
        ether.browseUrl(PSTR("/?"), req, PSTR("192.168.1.1"), my_callback);
      }
      else
      {
        char req[15];
        sprintf(req, P("cmd=plitem&i=%i"), station_title_id_req);
        ether.browseUrl(PSTR("/?"), req, PSTR("192.168.1.1"), my_callback);
      }
     
    }
    else if (web_state == 5)
    {
      //Serial.print(P("<<< REQ station change "));
      char req[20];
      sprintf(req, P("cmd=playstation&i=%i"), station_state);
      ether.browseUrl(PSTR("/?"), req, PSTR("192.168.1.1"), my_callback);
    }
    else if (web_state == 6 && blockReq+4000<millis())
    {
      //Serial.print(P("<<< REQ autoplay "));
      ether.browseUrl(PSTR("/?"), "cmd=play", PSTR("192.168.1.1"), my_callback);
    }
    
  }
}


void lcdOutMenu1(char* line)
{
    if (menu_line1 != line)
    {
      menu_line1 = line;
      lcd.clear();
      lcd.setCursor(0,0);    
      lcd.print(menu_line1);
      lcd.setCursor(0,1);    
      lcd.print(menu_line2);
    }
}

void lcdOutMenu2(char* line)
{
    if (menu_line2 != line)
    {
      menu_line2 = line;
      lcd.clear();
      lcd.setCursor(0,0);    
      lcd.print(menu_line1);
      lcd.setCursor(0,1);    
      lcd.print(menu_line2);
    }
}

void lcdOutTitle(char* line)
{
    while (strlen(line) < 12)
        { strcat(line," "); }
    if (menu_line2 != line)
    {
      menu_line2 = line;
      lcd.clear();
      lcd.setCursor(0,0);    
      lcd.print(menu_line1);
      lcd.setCursor(0,1);    
      lcd.print(P("< "));      
      lcd.print(menu_line2);
      lcd.print(P(" >"));
    }
}

void lcdOut()
{
 if (msg.length() > 0 || msg2.length() > 0)
  {

    //clear lines
    for (int x = 0; x < 16; x++)
    {
      lcd_line1[x] = ' '; lcd_line2[x] = ' ';
    }
    
    //print lines
    for (int i = 0; i < max(msg.length(), msg2.length())+1; i++)
    {
      if (i>15)
      {
        if (i<msg.length())
        {
          for (int x = 0; x < 15; x++)
          {
            lcd_line1[x] = lcd_line1[x+1];
          }
          lcd_line1[15] = msg.charAt(i);
        }
        else
        {
          for (int x = 0; x < 16; x++)
          {
            lcd_line1[x] = ' ';
          }
          for (int i = 0; i < min(msg.length(),16); i++)
          {
            lcd_line1[i] = msg.charAt(i);
          }
        }
        
        if (i<msg2.length())
        {
          for (int x = 0; x < 15; x++)
          {
            lcd_line2[x] = lcd_line2[x+1];
          }
          lcd_line2[15] = msg2.charAt(i);
        }
        else
        {
          for (int x = 0; x < 16; x++)
          {
            lcd_line2[x] = ' ';
          }
          for (int i = 0; i < min(msg2.length(),16); i++)
          {
            lcd_line2[i] = msg2.charAt(i);
          }
        }
       
      }
      else
      {
        if (i<msg.length()) { lcd_line1[i] = msg.charAt(i); };        
        if (i<msg2.length()) { lcd_line2[i] = msg2.charAt(i); };
      }
      //msg2.toCharArray(lcd_line2, 17);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(lcd_line1);
      lcd.setCursor(0, 1);
      lcd.print(lcd_line2);
      delay(200);
      if (menu_state != 0)
      { return; }
    }  
  }
  
  msg = '\0';
  msg2 = '\0';
}


// Interrupt on A changing state
void doEncoderA(){
  
    
  // debounce
  delay (1);  // wait a little until the bouncing is done

  // Test transition, did things really change? 
  if( digitalRead(encoderPinA) != A_set ) {  // debounce once more
    A_set = !A_set;

    // adjust counter + if A leads B
    if ( A_set && !B_set ) 
      encoderPos += 1;
      if (encoderPos != lastReportedPos && (lastDebounceTimeRot == 0 || millis() - lastDebounceTimeRot > 5))
       {  
         lastDebounceTimeRot = millis();
         Serial.print(encoderPos);
         lastReportedPos = encoderPos;
         if (menu_state > 0 && menu_state < 4)
         {           
            menu_state++; 
         }
         else if (menu_state == 4)
         { menu_state = 1; }
         else if (menu_state == 5)
         { menu_state++; }
         else if (menu_state == 6)
         { menu_state--; }
         else if (menu_state == 10)
         {
            if (station_state < station_count)
             { station_state++; station_title_id_req = station_state; web_state=4; }
            else
             { menu_state = 11; } 
         }
         else if (menu_state == 11)
         {
           if (station_count > 0)
           {
           station_state = 1;
           menu_state = 10;
                        
           web_state = 4;
           station_title_id_req = station_state;
           }
         }
       }
  }
   
}

// Interrupt on B changing state, same as A above
void doEncoderB(){
  
  
  delay (1);
  if( digitalRead(encoderPinB) != B_set ) {
    B_set = !B_set;
    //  adjust counter - 1 if B leads A
    if( B_set && !A_set ) 
      encoderPos -= 1;
      
      if (encoderPos != lastReportedPos && (lastDebounceTimeRot == 0 || millis() - lastDebounceTimeRot > 5))
   {  
     lastDebounceTimeRot = millis();
     Serial.print(encoderPos);
     lastReportedPos = encoderPos;
     if (menu_state > 1 && menu_state < 5)
     {           
        menu_state--; 
     }
     else if (menu_state == 1)
     { menu_state = 4; }
     else if (menu_state == 5)
     { menu_state++; }
     else if (menu_state == 6)
     { menu_state--; }
     else if (menu_state == 10)
     {
        if (station_state > 1)
         { station_state--; station_title_id_req = station_state; web_state=4; }
        else
         { menu_state = 11; } 
     }
     else if (menu_state == 11)
     {
       if (station_count > 0)
       {
         station_state = station_count;
         menu_state = 10;
         
         web_state = 4;
         station_title_id_req = station_state;
       }
     }
  }
   }
}

void pin3func() {
   delay (1);
    if( digitalRead(buttonPin) != C_set ) {
    C_set = !C_set;
   
      
   if (lastDebounceTime == 0 || millis() - lastDebounceTime > 250)
   {  
     lastDebounceTimeRot = millis();
     
     if ( digitalRead(buttonPin) == LOW)
        {
          
        Serial.println(P("Menubutton pressed ok"));
        if (menu_state == 0 || menu_state == 9)
        {
           menu_state = 1; 
        }
        else if (menu_state == 1)
        {
           menu_state = 7; 
           web_state = 2; 
        }
        else if (menu_state == 2)
        {
          if (station_count > 0)
          {
             menu_state = 10; 
             station_state = 1;
             station_title_id_req = 1;
             web_state = 4;
          }
          else
          {
            menu_state = 11; 
          }
        }
        else if (menu_state == 3)
        {
           menu_state = 5; 
           web_state = 1;
        }
        else if (menu_state == 4)
        {
           msg = '\0';
           msg2 = '\0';
           menu_state = 8;         
        }        
        else if (menu_state == 6)
        {
           menu_state = 3; 
           menu_ip[0] = '0';
        }
        else if (menu_state == 10)
        {
           msg = '\0';
           msg2 = '\0';
           menu_state = 12; 
           web_state = 5;
        }
        else if (menu_state == 11)
        {
           msg = '\0';
           msg2 = '\0';
           menu_state = 2; 
           station_state = '1';
        }
      }
   }
   
    }
}


