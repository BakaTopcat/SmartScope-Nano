/*
   ENCODER
   D4, D5, D6
*/

/*
   OLED
   A4, A5
*/

/*
   ENC28J60
   D10, D11, D12, D13
*/

/* OLED */
#include "SSD1306Ascii.h"
#include "SSD1306AsciiAvrI2c.h"
#include "dosfont.h"
//#include "topcat866.h"
#define I2C_ADDRESS 0x3C
SSD1306AsciiAvrI2c oled;

/* ENCODER */
#define ENC_S1 6 //D6
#define ENC_S2 5 //D5
#define ENC_SW 4 //D4
#include "encMinim.h"
// пин clk, пин dt, пин sw, направление (0/1)
encMinim enc(ENC_S1, ENC_S2, ENC_SW, 1);

/* EEPROM */
#include <EEPROM.h>

/* MICROUART */
//#define MU_STREAM     // подключить Stream.h (readString, readBytes...)
#define MU_PRINT      // подключить Print.h (print, println)
//#define MU_TX_BUF 64  // буфер отправки. По умолч. 8. Можно отключить (0)
//#define MU_RX_BUF 64  // буфер приёма. По умолч. 8. Можно отключить (0)
#include <MicroUART.h>
MicroUART uart;

/* ETHERNET */
#include <EthernetENC.h>
EthernetClient client;

#define TOT_MONITORS 12
struct mon_t { //total size 16 bytes
  byte ipaddr[4];
  byte ab;
  char nam[11]; //actual length of string is 10 chars, 11th is \0
};
mon_t mon;
int8_t curr_mon, prev_mon; // curr_mon: from 1 to TOT_MONITORS (currently 12)

//13th char is \0
#define TOT_SCOPES 9
const char scopes[TOT_SCOPES][13] = {"AudioDbfs", "AudioDbvu", "Histogram", "ParadeRGB", "ParadeYUV", "Picture", "Vector100", "Vector75", "WaveformLuma"};
int8_t curr_scope = 5, prev_scope = 4;

//local setup pages
#define P_IP 0
#define P_MASK 1
#define P_GW 2
#define P_EXIT 3
#define SETUP_PAGES 4
const char localsetup[SETUP_PAGES][5] = {"IP", "Mask", "GW", "Exit"};

bool selection = true; //or false, that means it's in adjusting mode. We need this variable in both setup() and loop()


void setup() {
  uart.begin(57600);

  byte ip[3][4]; //ip addresses used throughout all the setup() function, including the ENC28J60 configuring
  EEPROM.get(0x00, ip); //size of ip[] is 12 bytes

  /* OLED PART */
  oled.begin(&Adafruit128x64, I2C_ADDRESS);
  //oled.setFont(System5x7);
  oled.setFont(dosfont);
  oled.clear();
  oled.println(F("SmartScope"));
  oled.println(F("Press encoder\nto enter Setup"));

  //5 seconds timeout to click the encoder to enter setup
  if (clicktimeout(5000) == false) { //'false' means the encoder has been clicked within timeout period; proceed to the setup routine
    oled.clear();
    oled.println(F("Setup"));

    /* SETUP ROUTINE */
    int8_t setup_page = 0, prev_page = -1;

    while (true) { //selecting the setup page
      enc.tick();
      if (selection) { // -------- SELECTION mode --------
        // oled.invertDisplay(false);
        setup_page = roll_enc(setup_page, 0, SETUP_PAGES + (TOT_MONITORS * 3) - 1);
        /* if (enc.isLeft()) setup_page--;
           if (enc.isRight()) setup_page++;
           if (setup_page < 0) setup_page = SETUP_PAGES + (TOT_MONITORS * 3) - 1;
           if (setup_page > SETUP_PAGES + (TOT_MONITORS * 3) - 1) setup_page = 0;*/

        if (setup_page != prev_page) { //setup cursor changed, displaying routine
          if ((setup_page >= P_IP) && (setup_page <= P_EXIT)) { //displaying IP setup pages
            oled.setCursor(0, 2);
            oled.print(localsetup[setup_page]);
            oled.clearToEOL();

            //displaying the value
            oled.setCursor(0, 4);
            if ((setup_page >= P_IP) && (setup_page <= P_GW)) {
              oled_printip(ip[setup_page], true);
            } else oled.clearToEOL();
          } else { //displaying MON setup pages
            /* monitors setup: EEPROM starts from 0x20;
               setup_page starts from SETUP_PAGES (currently from 4);
               every monitor has 3 pages on setup: ipaddr, active (a/b), name
               monitor record within EEPROM: 16 bytes */
            // curr_mon: from 1 to TOT_MONITORS (currently 12)
            curr_mon = 1 + (setup_page - SETUP_PAGES) / 3;
            oled.setCursor(0, 2);
            oled.print(F("Mon "));
            oled.print(curr_mon);
            oled.write(0x20); //space

            EEPROM.get(get_eeprommonoffset(curr_mon), mon);

            /*curr_mon   :    1          2           3
              curr_mon*3 :    3          6           9
              setup_page    : 4, 5, 6;   7, 8, 9;  10, 11, 12 */
            switch (setup_page - (curr_mon * 3)) {
              case 1:
                oled.print(F("IP"));
                oled.clearToEOL();
                oled.setCursor(0, 4);
                oled_printip(mon.ipaddr, true);
                break;
              case 2:
                oled.print(F("A/B"));
                oled.clearToEOL();
                oled.setCursor(0, 4);
                oled_printab(mon.ab);
                break;
              case 3:
                oled.print(F("Name"));
                oled.clearToEOL();
                oled.setCursor(0, 4);
                oled.print(mon.nam);
                oled.clearToEOL();
                break;
            } //of switch
          } //of else
          //remembering the cursor to check whether it's changed in next iteration
          prev_page = setup_page;
        } //of if
      } else { // -------- ADJUSTING mode --------
        // oled.invertDisplay(true);
        if ((setup_page >= P_IP) && (setup_page <= P_GW)) { // IP, Mask, GW
          if (setup_page == P_IP) { //displaying DHCP hint on IP page
            oled.setCursor(0, 6);
            oled.print(F("DHCP: 000.x.x.x"));
          } else {
            oled.setCursor(0, 6);
            oled.clearToEOL();
          }
          ip_edit(4, ip[setup_page]);
          selection = true; //going back to selection mode
        }
        if (setup_page == P_EXIT) {
          break; //this is the only point of exiting the endless loop of setup routine
        }
        if (setup_page > P_EXIT) { //MON adjusting
          switch (setup_page - (curr_mon * 3)) {
            case 1:
              oled.setCursor(0, 4);
              ip_edit(4, mon.ipaddr);
              break;
            case 2:
              oled.setCursor(0, 4);
              mon.ab = ab_edit(4, mon.ab);
              if ((curr_mon == 1) && (mon.ab == 0)) // MON 1 always active!!!
                mon.ab = 1; //set mode to A
              prev_page = setup_page - 1; // this needed to refresh the A/B/Inactive display
              break;
            case 3:
              oled.setCursor(0, 4);
              text_edit(4, mon.nam, sizeof(mon.nam));
              break;
          } //of switch
          EEPROM.put(get_eeprommonoffset(curr_mon), mon);
          selection = true; //going back to selection mode
        }
      } //exit from adjusting mode
      if (enc.isClick()) selection = !selection;
    } //of while (true)

    //local setup is over, let's write the ip data
    EEPROM.put(0x00, ip);

  } /* END OF SETUP ROUTINE */

  /* MAIN ROUTINE */
  //  oled.invertDisplay(false);
  oled.clear();

  uint8_t mac[6] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};

  if (ip[P_IP][0] == 0) { //DHCP
    uart.print(F("DHCP..."));
    oled.print(F("DHCP..."));
    Ethernet.begin(mac);
    uart.println(F("done"));
    oled.println(F("done"));
  } else { //Static
    Ethernet.begin(mac, ip[P_IP], {}, ip[P_GW], ip[P_MASK]);
    uart.println(F("Static"));
    oled.println(F("Static"));
  }

  uart.print(F("IP: "));
  uart.println(Ethernet.localIP());
  uart.print(F("Mask: "));
  uart.println(Ethernet.subnetMask());
  uart.print(F("GW: "));
  uart.println(Ethernet.gatewayIP());
  uart.print(F("DNS: "));
  uart.println(Ethernet.dnsServerIP());

  oled.println(Ethernet.localIP());
  oled.println(Ethernet.subnetMask());
  oled.println(Ethernet.gatewayIP());

  clicktimeout(5000);
  oled.clear();

  selection = true; //this needed for loop() statement
  // curr_mon: from 1 to TOT_MONITORS (currently 12)
  // curr_mon value goes to loop
  curr_mon = 1;
  prev_mon = curr_mon - 1;

} //of setup()

void loop() {

  //encoder polling
  enc.tick();
  if (selection) { // -------- SELECTION mode --------
    oled.invertDisplay(false);

    if (enc.isLeft()) {
      do {
        curr_mon--;
        if (curr_mon < 1) //get to last one
          curr_mon = TOT_MONITORS;
        //let's check whether the monitor is active
        EEPROM.get(get_eeprommonoffset(curr_mon), mon);
      } while ((mon.ab != 1) && (mon.ab != 2));
    }

    if (enc.isRight()) {
      do {
        curr_mon++;
        if (curr_mon > TOT_MONITORS) //get to last one, go to first (it's always active)
          curr_mon = 1;
        //let's check whether the monitor is active
        EEPROM.get(get_eeprommonoffset(curr_mon), mon);
      } while ((mon.ab != 1) && (mon.ab != 2));
    }

    if (curr_mon != prev_mon) { //current mon changed, displaying routine

      EEPROM.get(get_eeprommonoffset(curr_mon), mon);
      oled.clear();
      oled.print(mon.nam);
      oled.print(" (");
      oled.print(curr_mon);
      oled.print(mon.ab == 1 ? "A" : "B");
      oled.println(")");
      oled.setCursor(0, 6);
      oled_printip(mon.ipaddr, true);
      //resetting the scope to default
      curr_scope = 5; prev_scope = 4;
      //remembering the value to check whether it's changed in next iteration
      prev_mon = curr_mon;
    }

  } else { // -------- ADJUSTING mode --------
    oled.invertDisplay(true);

    curr_scope  = roll_enc(curr_scope, 0, TOT_SCOPES - 1);

    if (curr_scope != prev_scope) { //scope changed
      oled.setCursor(0, 2);
      oled.print(scopes[curr_scope]); oled.clearToEOL();

      //let's connect to the monitor and change the scope
      uart.print(F("Connecting to ")); uart.println(IPAddress(mon.ipaddr));

      if (client.connect(mon.ipaddr, 9992)) { //blackmagic 9992
        uart.println(F("Connected"));

        client.print(F("MONITOR "));
        client.println(mon.ab == 1 ? "A:" : "B:");
        client.print(F("ScopeMode: "));
        client.println(scopes[curr_scope]);
        client.println();

        client.flush(); //Waits until all outgoing characters in buffer have been sent.

        uart.println(F("Disconnected"));
        client.stop();
      } else {
        uart.println(F("Connect failed"));
      }


      //remembering the value to check whether it's changed in next iteration
      prev_scope = curr_scope;
    }

  }   //exit from adjusting mode
  if (enc.isClick()) selection = !selection;


  /*
    if (curr_mon != prev_mon) { //current mon changed, displaying routine
      EEPROM.get(get_eeprommonoffset(curr_mon), mon);
      oled.clear();
      oled.print(mon.nam);
      oled.print(" (");
      oled.print(curr_mon);
      oled.print(mon.ab == 1 ? "A" : "B");
      oled.println(")");
      //resetting the scope to default
      curr_scope = 5; prev_scope = 4;
      //remembering the value to check whether it's changed in next iteration
      prev_mon = curr_mon;
    }

    //encoder polling
    enc.tick();

    if (enc.isClick()) { //encoder clicked, let's change the monitor to next one
      do {
        curr_mon++;
        if (curr_mon > TOT_MONITORS) //get to last one, go to first (it's always active)
          curr_mon = 1;
        //let's check whether the monitor is active
        EEPROM.get(get_eeprommonoffset(curr_mon), mon);
      } while ((mon.ab != 1) && (mon.ab != 2));

    }

    if (enc.isLeft()) curr_scope--;
    if (enc.isRight()) curr_scope++;
    if (curr_scope < 0) curr_scope = TOT_SCOPES - 1;
    if (curr_scope > TOT_SCOPES - 1) curr_scope = 0;

    if (curr_scope != prev_scope) { //scope changed
      oled.setCursor(0, 2);
      oled.print(scopes[curr_scope]); oled.clearToEOL();

      //let's connect to the monitor and change the scope
      uart.print(F("Connecting to ")); uart.println(IPAddress(mon.ipaddr));

      if (client.connect(mon.ipaddr, 9992)) { //blackmagic 9992
        uart.println(F("Connected"));

        client.print(F("MONITOR "));
        client.println(mon.ab == 1 ? "A:" : "B:");
        client.print(F("ScopeMode: "));
        client.println(scopes[curr_scope]);
        client.println();

        client.flush(); //Waits until all outgoing characters in buffer have been sent.

        uart.println(F("Disconnected"));
        client.stop();
      } else {
        uart.println(F("Connect failed"));
      }

      //remembering the value to check whether it's changed in next iteration
      prev_scope = curr_scope;
    }




    /*  enc.tick();

      if (enc.isClick()) {
        oled.clear();
        uart.println(F("Client connecting..."));
        //    oled.println(F("Client connecting..."));

        if (client.connect(IPAddress(192, 168, 0, 36), 80))
        {
          uart.println(F("Client connected"));
          //      oled.println(F("Client connected"));
          client.println(F("DATA from Client"));
          client.flush(); //Waits until all outgoing characters in buffer have been sent.

          unsigned long _time = millis();

          //let's wait until there's something in, at least 1 byte
          while (client.available() == 0) {
            if (millis() - _time > 1000) {
              uart.println(F("Response timeout"));
              //         oled.println(F("Response timeout"));
              break;
            }
          }

          while (client.available() > 0) {
            byte b = client.read();
            uart.write(b);
          }

          uart.println(F("Client disconnect"));
          //      oled.println(F("Client disconnect"));
          client.stop();
        } //of if(client.connect
        else {
          //      debug(F("Client connect failed"));
          oled.println(F("Client connect failed"));
        } //of else

      } //of isClick
  */
} //of loop()


//edits the IP address. ip[] is mutable array of 'len' bytes
void text_edit(byte pos_y, char txt[], byte len) {
  byte i;
  oled.setCursor(0, pos_y);
  //let's make the text tidy
  //e.g. len = 11 : 0..9 - text; 10 - "\0"
  for (i = 0; i <= len - 2; i++)
    if ((txt[i] < 0x20) || (txt[i] >> 0x7E)) //if char is out of ASCII range...
      txt[i] = 0x20; //... replace it with spaces
  txt[len - 1] = 0; //add terminating symbol
  //let's display the name
  oled.print(txt);
  uint8_t w = oled.charSpacing(0x30); //width in pixels of symbol "0" with current font magnification factor
  for (i = 0; i <= len - 2; i++) { //i does not touch the last symbol, it's always \0
    txt[i] = char_edit(i * w, pos_y, txt[i]);
  }
} //of text_edit


//edits the char, displaying it in desired position
//pos_x in pixels; pos_y in rows
char char_edit(byte pos_x, byte pos_y, char c) {
  //available char ASCII codes: 0x20 (space) to 0x7E
  oled.setInvertMode(true);
  char prev = c - 1;
  do {
    enc.tick();
    c = roll_enc(c, 0x20, 0x7E); //char is signed!!!111 wtf
    /*    if (enc.isLeft()) c--;
        if (enc.isRight()) c++;
        if (c < 0x20) c = 0x7E;
        if (c > 0x7E) c = 0x20; */
    //value changed, displaying routine
    if (c != prev) {
      oled.setCursor(pos_x, pos_y);
      oled.write(c);
      //remembering the value to check whether it's changed in next iteration
      prev = c;
    } //of if
  } while (!enc.isClick());
  //let's print the non-inverted value
  oled.setInvertMode(false);
  oled.setCursor(pos_x, pos_y);
  oled.write(c);
  return (c);
} //of char_edit


//edits the IP address. ip[] is mutable array of 4 bytes
void ip_edit(byte pos_y, byte ip[]) {
  oled.setCursor(0, pos_y);
  //let's display the IP first
  oled_printip(ip, false);
  //lets edit it octet by octet
  uint8_t w = oled.charSpacing(0x30); //width in pixels of symbol "0" with current font magnification factor
  byte i;
  for (i = 0; i <= 3; i++) {
    ip[i] = octet_edit(i * w * 4, pos_y, ip[i]);
  }
} //of ip_edit


//edits the octet, displaying it in desired position
//pos_x in pixels; pos_y in rows
uint8_t octet_edit(byte pos_x, byte pos_y, uint8_t octet) {
  oled.setInvertMode(true);
  uint8_t prev = octet - 1;
  do {
    enc.tick();
    if (enc.isLeft()) octet--;
    if (enc.isRight()) octet++;
    //value changed, displaying routine
    if (octet != prev) {
      oled.setCursor(pos_x, pos_y);
      oled_print3(octet);
      //remembering the value to check whether it's changed in next iteration
      prev = octet;
    }
  } while (!enc.isClick());
  //let's print the non-inverted value
  oled.setInvertMode(false);
  oled.setCursor(pos_x, pos_y);
  oled_print3(octet);
  return octet;
}


void oled_printip(byte ip[], bool clreol) {
  byte i;
  for (i = 0; i <= 3; i++) {
    oled_print3(ip[i]);
    if (i != 3)
      oled.write(0x2E); //decimal dot
  }
  if (clreol)
    oled.clearToEOL();
}


//prints the value with leading zeros
void oled_print3(byte b) {
  if (b < 100) oled.write(0x30); //oled.print("0");
  if (b < 10) oled.write(0x30);  //oled.print("0");
  oled.print(b);
}


//ab: 0, 1, 2
int8_t ab_edit(byte pos_y, int8_t ab) {
  oled.setInvertMode(true);
  int8_t prev = ab - 1;
  do {
    enc.tick();
    ab = roll_enc(ab, 0, 2);
    /*    if (enc.isLeft()) ab--;
        if (enc.isRight()) ab++;
        if (ab < 0) ab = 0;
        if (ab > 2) ab = 2; */
    //value changed, displaying routine
    if (ab != prev) {
      oled.setCursor(0, pos_y);
      oled_printab(ab);
      //remembering the value to check whether it's changed in next iteration
      prev = ab;
    }
  } while (!enc.isClick());
  //let's print the non-inverted value
  oled.setInvertMode(false);
  oled.setCursor(0, pos_y);
  oled_printab(ab);
  return ab;
}


//prints A / B / Inactive
void oled_printab(byte ab) {
  if (ab == 1)
    oled.print(F("A"));
  else if (ab == 2)
    oled.print(F("B"));
  else
    oled.print(F("Inactive"));
  oled.clearToEOL();
}


//returns 'true' if there was a timeout, 'false' if encoder has been clicked within timeout period
bool clicktimeout(uint16_t timeout) {
  unsigned long _time = millis();
  do {
    enc.tick();
    if (millis() - _time > timeout) {
      return true; //timeout happened, return true
      break;
    }
  } while (!enc.isClick());
  //encoder is clicked, return false
  return false;
}

//gets EEPROM offset for monitors
//curr_mon from 1 to 12
//EEPROM starts from 0x20
int get_eeprommonoffset(byte curr_mon) {
  return (0x20 + (curr_mon - 1) * 16);
}

//polls encoder's left and right
//rolls value
int8_t roll_enc(int8_t value, int8_t _min, int8_t _max) {
  if (enc.isLeft()) value--;
  if (enc.isRight()) value++;
  if (value < _min) value = _max;
  if (value > _max) value = _min;
  return value;
}
