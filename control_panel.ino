/*
   =====================================================================================

      Filename:  control_panel.ino

      Description:

          Version:  2.0
          Created:  02/05/2024
         Revision:  none
         Compiler:  gcc

           Author:  Sebastien Chassot (sinux), seba.ptl@sinux.net
         Modified:  Alexandre Rosenberg, Maxime Borges <contatc@maximeborg.es>
          Company:  PostTenebrasLab, the Geneva's hackerspace   posttenebraslab.ch

   =====================================================================================
     Alexandre Rosenberg - Added basic serial control (speed: 115200)
     "get 1", "get 2" for galva 1 and 2 value
     "set 1 x" to set galva 1 to value x, "set 2 x" for galva 2

     Alexandre Rosenberg - tweaked value and scale to non linear
     to match with the printed design.

     Maxime Borges - Big cleanup
      - Use increment of 15min for the time left galvo and 1 person for the people galvo
      - Use map to properly align the galvos with the printed graphics
      - Refactor variables/constant, standardize format


*/

#include <SPI.h>
#include <Bounce.h>

#include <Dhcp.h>
#include <EthernetClient.h>
#include <Dns.h>
#include <Ethernet.h>

// See `config.example.h` for an example of config
#include "config.h"


// Ethernet Shield
// MAC address of the Ethernet shield
byte mac[] = { 0x90, 0xA2, 0xDA, 0x00, 0x2C, 0x86 };

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
EthernetClient client;

// Uncomment to switch to debug mode; this will allow to move the galvos and see corresponding values to align them properly using the maps
// #define DEBUG

// Pins
const uint8_t PIN_LED_POWER = A0;
const uint8_t PIN_LED_R = A1;
const uint8_t PIN_LED_G = A2;
const uint8_t PIN_LED_B = A3;

const uint8_t PIN_PWR_SWITCH = 7;

const uint8_t PIN_LEV_L_L = 8;
const uint8_t PIN_LEV_L_R = 9;
const uint8_t PIN_LEV_R_L = 3;
const uint8_t PIN_LEV_R_R = 2;

const uint8_t PIN_GALV_L = 6;
const uint8_t PIN_GALV_R = 5;

// Const 
const uint32_t QUARTER_HOUR_IN_MS = (uint32_t)15 * 60 * 1000;
// Period of power LED blink when the lab is not open
const uint32_t BLINK_PERIOD_MS = 2000;
// Duration of the `on` period for lab closed blink
const uint32_t BLINK_ON_DURATION_MS = 500;
// Fast blink period when the lab is about to close
const uint32_t ALMOST_CLOSED_BLINK_PERIOD_MS = 500;

// In quarter of hours
const uint8_t T_QUARTERS_LEFT_SUBDIV = 4;
const uint8_t T_QUARTERS_LEFT_MAX = 16 * T_QUARTERS_LEFT_SUBDIV;
const uint8_t PEOPLE_MAX = 14;

const uint32_t LEVER_TIME_LEFT_INTERVAL_MS = 100;
const uint32_t LEVER_PEOPLE_INTERVAL_MS = 300;

// Macros
#define LED_POWER_On (digitalWrite(PIN_LED_POWER, HIGH))
#define LED_POWER_Off (digitalWrite(PIN_LED_POWER, LOW))
#define LED_R_On (digitalWrite(PIN_LED_R, LOW))
#define LED_R_Off (digitalWrite(PIN_LED_R, HIGH))
#define LED_B_On (digitalWrite(PIN_LED_B, LOW))
#define LED_B_Off (digitalWrite(PIN_LED_B, HIGH))
#define LED_G_On (digitalWrite(PIN_LED_G, LOW))
#define LED_G_Off (digitalWrite(PIN_LED_G, HIGH))

// Levers and switch are using the debounce library `Bounce`
// We are using 20ms of debounce time
Bounce pwr_switch = Bounce(PIN_PWR_SWITCH, 20);
Bounce lev_r_r = Bounce(PIN_LEV_R_R, 20);
Bounce lev_r_l = Bounce(PIN_LEV_R_L, 20);
Bounce lev_l_r = Bounce(PIN_LEV_L_R, 20);
Bounce lev_l_l = Bounce(PIN_LEV_L_L, 20);

// Time of the last change of the levers
uint32_t last_lever_left_change_ms = 0;
uint32_t last_lever_right_change_ms = 0;

// Time left in quarter of hours
uint8_t t_quarters_left = 0;
uint8_t n_person = 0;

// Map for the time left lever to make it properly aligned with the graphics
const uint8_t t_quarters_left_map[] = {
  0,
  26,
  50,
  63,
  76,
  89,
  102,
  117,
  129,
  143,
  159,
  173,
  189,
  204,
  219,
  237,
  254,
};

// Map for the person lever to make it properly aligned with the graphics
const uint8_t n_person_map[] = {
  0,
  24,
  54,
  69,
  85,
  102,
  119,
  135,
  151,
  170,
  187,
  205,
  222,
  235,
  251,
};

// Reference time to decrease the time left
uint32_t reftime_ms = 0;
// Last time we blinked the power LED
uint32_t last_led_blink_ms = 0;

// Used to send update when there is change using the levers
// Reset to 0 once the update is sent
uint32_t last_change_ms = 0;
bool wait_for_stabilize = false;

// Statically allocated string to store data to send to the server
// It is reused every time to avoid reallocating
String post_data = String(256);

void printIPAddress() {
  Serial.print("My IP address: ");
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    Serial.print(Ethernet.localIP()[thisByte], DEC);
    Serial.print(".");
  }
  Serial.println();
}

void updateStatus() {
  String open_closed;
  String txt_open_closed;
  String txt_ppl;

  if (t_quarters_left == 0) {
    open_closed = "closed";
    txt_open_closed = "The lab is closed.";
  } else if (t_quarters_left > 0 and t_quarters_left <= 4) {
    open_closed = "open";
    txt_open_closed = "The lab is open - remaining time is " + String(t_quarters_left * 15) + " min.";
  } else {
    open_closed = "open";
    auto str_hours_left = String(t_quarters_left / T_QUARTERS_LEFT_SUBDIV);
    auto n_quarters_in_hour = t_quarters_left % T_QUARTERS_LEFT_SUBDIV;
    auto str_minutes_left = (n_quarters_in_hour > 0) ? String(n_quarters_in_hour*15) : String();
    txt_open_closed = "The lab is open - planned to be open for " + str_hours_left + "h" + str_minutes_left + ".";
  }

  if (n_person == 0) {
    txt_ppl = "Nobody here !";
  } else if (n_person == 1) {
    txt_ppl = "One lonely hacker in the space.";
  } else {
    txt_ppl = "There are " + String(n_person) + " hackers in the space.";
  }

  post_data = "api_key=";
  post_data += API_KEY;
  post_data += "&status=" + txt_open_closed + " " + txt_ppl + "  [Set by PTL control panel]";
  post_data += "&open_closed=" + open_closed;
  post_data += "&submit=Submit";

  if (client.connect(SERVER_HOSTNAME, SERVER_PORT)) {
    Serial.println("connected - status update");

    // Send a HTTP POST request
    client.println("POST /change_status HTTP/1.1");
    client.println(String("Host: ") + SERVER_HOSTNAME);
    client.println("User-Agent: ControlPanel/2.0");
    client.print("Content-Length: ");
    client.println(post_data.length());
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.println();
    client.println(post_data);
    Serial.println("disconnecting.");
    client.stop();
  } else {
    // No connection to the server
    Serial.println("connection failed");
  }
}


void update_buttons_debounce() {
  pwr_switch.update();
  lev_r_r.update();
  lev_r_l.update();
  lev_l_r.update();
  lev_l_l.update();
}


// Main functions
void setup() {
  pinMode(PIN_PWR_SWITCH, INPUT_PULLUP);
  pinMode(PIN_LEV_L_L, INPUT_PULLUP);
  pinMode(PIN_LEV_L_R, INPUT_PULLUP);
  pinMode(PIN_LEV_R_L, INPUT_PULLUP);
  pinMode(PIN_LEV_R_R, INPUT_PULLUP);

  pinMode(PIN_LED_POWER, OUTPUT);
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);

  LED_POWER_On;
  LED_R_Off;
  LED_G_Off;
  LED_B_On;

  // Open serial communications and wait for port to open:
  // initialize serial:
  Serial.begin(115200);
  Serial.println("Started Serial");

  #ifndef DEBUG

    // [Eth Shield] Start the Network connection
    if (Ethernet.begin(mac) == 0) {
      Serial.println("Failed to configure Network DHCP");
      // no point in carrying on, so do nothing forevermore
      while (true);
    }

    // [Eth Shield] Give a second to initialize:
    delay(1000);
    Serial.println("Configured IP using DHCP");
    printIPAddress();
    
    LED_B_Off;
  #endif
}

void loop() {
  update_buttons_debounce();

  #ifndef DEBUG
    // Left lever
    if (millis() - last_lever_left_change_ms > LEVER_TIME_LEFT_INTERVAL_MS) {
      if (!lev_l_r.read()) {
        t_quarters_left = min(t_quarters_left + 1, T_QUARTERS_LEFT_MAX);
        last_lever_left_change_ms = millis();
        last_change_ms = millis();
      } else if (!lev_l_l.read()) {
        t_quarters_left = t_quarters_left - min(t_quarters_left, 1);
        last_lever_left_change_ms = millis();
        last_change_ms = millis();
      }
    }

    // Right lever
    if (millis() - last_lever_right_change_ms > LEVER_PEOPLE_INTERVAL_MS) {
      if (!lev_r_r.read()) {
        n_person = min(n_person + 1, PEOPLE_MAX);
        last_lever_right_change_ms = millis();
        last_change_ms = millis();
      } else if (!lev_r_l.read()) {
        n_person = n_person - min(n_person, 1);
        last_lever_right_change_ms = millis();
        last_change_ms = millis();
      }
    }

    // Reset the galvos if the main switch is set to off for 2s
    if (pwr_switch.read() && (pwr_switch.duration() > 2000)) {
      t_quarters_left = 0;
      n_person = 0;
    }

    // If space is open, there should be at least one person !
    if ((t_quarters_left > 0) && (n_person == 0)) {
      n_person = 1;
    }

    // If there is no time left, set number of people to 0
    if (t_quarters_left == 0) {
      n_person = 0;
    }

    // Write time left to left galvo
    auto t_quarters_left_map_index = t_quarters_left / T_QUARTERS_LEFT_SUBDIV;
    auto time_mod_index = t_quarters_left % T_QUARTERS_LEFT_SUBDIV;
    auto t_quarters_left_mapped_value = t_quarters_left_map[t_quarters_left_map_index];
    if (time_mod_index != 0) {
      t_quarters_left_mapped_value += (t_quarters_left_map[t_quarters_left_map_index + 1] - t_quarters_left_map[t_quarters_left_map_index]) / T_QUARTERS_LEFT_SUBDIV * time_mod_index;
    }
    analogWrite(PIN_GALV_L, t_quarters_left_mapped_value);

    // Write person to right galvo
    auto n_person_mapped_value = n_person_map[n_person];
    analogWrite(PIN_GALV_R, n_person_mapped_value);
  
    // Decrement time every 15 minutes
    if ((millis() - reftime_ms) >= QUARTER_HOUR_IN_MS) {
      t_quarters_left -= min(t_quarters_left, 1);
      reftime_ms = millis();
      last_change_ms = millis();
    }


    // Blink the red LED when the lab is closed
    if (t_quarters_left == 0) {
      if ((millis() - last_led_blink_ms) > (BLINK_PERIOD_MS + BLINK_ON_DURATION_MS)) {
        LED_POWER_Off;
        last_led_blink_ms = millis();
      } else if ((millis() - last_led_blink_ms) > BLINK_PERIOD_MS) {
        LED_POWER_On;
      }
      LED_R_Off;
      LED_G_Off;
      LED_B_Off;
    } else {
      // Green if more than 1h30 left
      if (t_quarters_left > 6) {
        LED_R_Off;
        LED_G_On;
        LED_B_Off;
      }
      // Orange if 30min left
      else if (t_quarters_left > 2) {
        LED_R_On;
        LED_G_On;
        LED_B_Off;
      } else {
        // Fast blink red LED when less than 30min left
        static bool almost_closed_led_status = LOW;
        if (millis() - last_led_blink_ms > ALMOST_CLOSED_BLINK_PERIOD_MS) {
          if (almost_closed_led_status == HIGH) {
            LED_R_Off;
            almost_closed_led_status = LOW;
          } else {
            LED_R_On;
            almost_closed_led_status = HIGH;
          }
          LED_G_Off;
          LED_B_Off;
          last_led_blink_ms = millis();
        }
      }
      LED_POWER_Off;
    }

    // Update status when changes stabilized
    if (last_change_ms != 0 && millis() - last_change_ms > 2000) {
      updateStatus();
      last_change_ms = 0;
    }

  #else

    // Debug via serial
    if (Serial.available()) {
      auto s = Serial.readStringUntil('\n');
      if (s.startsWith("t")) {
        t_quarters_left = s.substring(1).toInt();
        Serial.print("new time:");
        Serial.println();

        auto t_quarters_left_map_index = t_quarters_left / T_QUARTERS_LEFT_SUBDIV;
        auto time_mod_index = t_quarters_left % T_QUARTERS_LEFT_SUBDIV;
        auto t_quarters_left_mapped_value = t_quarters_left_map[t_quarters_left_map_index];
        Serial.println(t_quarters_left_map_index);
        Serial.println(time_mod_index);
        Serial.println(t_quarters_left_mapped_value);
        if (time_mod_index != 0) {
          t_quarters_left_mapped_value += (t_quarters_left_map[t_quarters_left_map_index + 1] - t_quarters_left_map[t_quarters_left_map_index]) / T_QUARTERS_LEFT_SUBDIV * time_mod_index;
        }
        Serial.println(t_quarters_left_mapped_value);
        analogWrite(PIN_GALV_L, t_quarters_left_mapped_value);
      } else if (s.startsWith("p")) {
        n_person = s.substring(1).toInt();
        Serial.print("new people:");
        Serial.println(n_person);

        auto n_person_map_index = n_person;
        auto n_person_mapped_value = n_person_map[n_person_map_index];
        Serial.println(n_person_map_index);
        Serial.println(n_person_mapped_value);
        Serial.println(n_person_mapped_value);
        analogWrite(PIN_GALV_R, n_person_mapped_value);
      } else if (s.startsWith("T")) {
        auto time_left_mapped_value = s.substring(1).toInt();
        Serial.print("new time left val:");
        Serial.println(time_left_mapped_value);
        analogWrite(PIN_GALV_L, time_left_mapped_value);
      } else if (s.startsWith("P")) {
        auto n_person_mapped_value = s.substring(1).toInt();
        Serial.print("new people val:");
        Serial.println(n_person_mapped_value);
        analogWrite(PIN_GALV_R, n_person_mapped_value);
      }
      Serial.flush();
    }
  #endif
}
