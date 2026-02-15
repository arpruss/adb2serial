#include "adb_structures.h"
#include "adb_devices.h"
#include "keymap.h"

#undef DEBUG
//#define POWER_EMULATES_RIGHT_MOUSE

#define POLL_DELAY    5

#define EMIT_ALT_PRESS   0xFF
#define EMIT_ALT_RELEASE 0xFE

#define OTHER_LINE PB11
#define LED PC13 // blue pill
//#define LED PB12 // black pill
#define Serial Serial2

bool capsLock = false;
bool numLock = false;
bool shift = false;
bool ctrl = false;
bool alt = false;
bool apple_extended_detected = false;
bool keyboard_present = false, mouse_present = false;
unsigned pressedKey=0;
unsigned repeatTargetTime=0;
unsigned repeatDelay = 200;
unsigned repeatRate = 33;

const uint32_t flourishPause = 40;

void flourish() {
  adb_keyboard_write_leds(false,false,true);
  delay(flourishPause);
  adb_keyboard_write_leds(false,true,false);
  delay(flourishPause);
  adb_keyboard_write_leds(true,false,false);
  delay(flourishPause);
  adb_keyboard_write_leds(false,true,false);
  delay(flourishPause);
  adb_keyboard_write_leds(false,false,true);
  delay(flourishPause);
  adb_keyboard_write_leds(false,false,false);
}

void setup() {
    // Turn the led off at the beginning of setup
    pinMode(LED, OUTPUT);
    digitalWrite(LED, HIGH);
    Serial.begin(9600);
    pinMode(OTHER_LINE, OUTPUT);
    digitalWrite(OTHER_LINE, HIGH);
    //while (1) Serial.write(19);

    // Set up the ADB bus
    adb_init();

    delay(1000); // A wait for good measure, apparently AEKII can take a moment to reset

    // Initialise the ADB devices
    // Switch the keyboard to Apple Extended if available
    bool error;
    adb_data<adb_register3> reg3 = {0}, mask = {0};
    do {
      uint32_t t0 = millis();
      do {
          error = false;
          reg3.data.device_handler_id = 0x03;
          mask.data.device_handler_id = 0xFF;
          apple_extended_detected = adb_device_update_register3(ADB_ADDR_KEYBOARD, reg3, mask.raw, &error);
          delay(20);
      } while(error && millis() < t0+1000);
      if (!error) keyboard_present = true;
  
      t0 = millis();
      // Switch the mouse to higher resolution, if available
      // TODO: Apple Extended Mouse Protocol (Handler = 4)
      error = false;
      do {
        reg3.raw = 0;
        mask.raw = 0;
        reg3.data.device_handler_id = 0x02;
        mask.data.device_handler_id = 0xFF;
        adb_device_update_register3(ADB_ADDR_MOUSE, reg3, mask.raw, &error);
        delay(20);
      } while(error && millis() < t0+1000);
      if (!error) mouse_present = true;
    } while(!keyboard_present && !mouse_present);

    Serial.begin(9600);

    // Set-up successful, turn on the LED
    if (keyboard_present)
      flourish();
    digitalWrite(LED, LOW);
}

#define CTRL(x) ((x)-'a'+1)

void handlePress(uint16_t key, int repeat) {
  pressedKey = key;
  
  if (repeat) 
    repeatTargetTime = millis() + repeatRate;
  else
    repeatTargetTime = millis() + repeatDelay;
  int emit = 0;

  switch(key) {
    case KEY_LEFT_SHIFT:
    case KEY_RIGHT_SHIFT:
      shift = true;
      key = 0;
      break;
    case KEY_LEFT_CTRL:
    case KEY_RIGHT_CTRL:
      key = 0;
      ctrl = true;
      break;
    case KEY_LEFT_ALT:
    case KEY_RIGHT_ALT:
      alt = true;
      key = 0;
      break;
    case KEY_RETURN:
      emit = 10;
      break;
    case KEY_LEFT_ARROW:
      if (ctrl)
        emit = CTRL('l');
      else
        emit = CTRL('b');
      break;
    case KEY_RIGHT_ARROW:
      if (ctrl)
        emit = CTRL('r');
      else
        emit = CTRL('f');
      break;
    case KEY_DOWN_ARROW:
      emit = CTRL('n');
      break;
    case KEY_UP_ARROW:
      emit = CTRL('p');
      break;
    case KEY_ESC:
      emit = 27;
      break;
    case KEY_DELETE:
      emit = 7;
    case KEY_BACKSPACE:
      emit = 8;
      break;      
    case KEY_KP_DOT:
      emit = '.';
      break;
    case KEY_KP_ASTERISK:
      emit = '*';
      break;
    case KEY_KP_PLUS:
      emit = '+';
      break;
    case KEY_KP_SLASH:
      emit = '/';
      break;
    case KEY_KP_ENTER:
      emit = 10;
      break;
    case KEY_KP_EQUAL:
      emit = '=';
      break;
    case KEY_KP_0:
      emit = '0';
      break;
    case KEY_KP_1:
      emit = '1';
      break;
    case KEY_KP_2:
      emit = '2';
      break;
    case KEY_KP_3:
      emit = '3';
      break;
    case KEY_KP_4:
      emit = '4';
      break;
    case KEY_KP_5:
      emit = '5';
      break;
    case KEY_KP_6:
      emit = '6';
      break;
    case KEY_KP_7:
      emit = '7';
      break;
    case KEY_KP_8:
      emit = '8';
      break;
    case KEY_KP_9:
      emit = '9';
      break;
    case KEY_PAUSE:
      if (ctrl)
        emit = 3;
      break;
    default:
      if ('a' <= key && key <= 'z') {
        if (ctrl) 
          emit = key + (1 - 'a');
        else if (alt)
          emit = key + (KEY_ALT_A-'a');
        else if (shift ^ capsLock) 
          emit = key + ('A' - 'a');
        
      }
      else if (shift) {
        if ('0' <= key && key <= '9') {
          emit = ")!@#$%^&*("[key-'0'];
        }
        else {
          switch(key) {
            case '-':
              emit = '_';
              break;
            case '=':
              emit = '+';
              break;
            case '[':
              emit = '{';
              break;
            case ']':
              emit = '}';
              break;
            case '\\':
              emit = '|';
              break;
            case ';':
              emit = ':';
              break;
            case '`':
              emit = ':';
              break;
            case '\'':
              emit = '"';
              break;
            case ',':
              emit = '<';
              break;
            case '.':
              emit = '>';
              break;
            case '/':
              emit = '?';
              break;
          }
        }
      }
      break;
  }
  if (emit) {
    Serial.write(emit);
  }
  else if (32 <= key) {
    Serial.write(key);
  }
  else {
//    Serial.print(key,HEX);
  }
}

void handleRelease(uint16_t key, int repeat) {
  pressedKey = 0;
  switch(key) {
    case KEY_LEFT_SHIFT:
    case KEY_RIGHT_SHIFT:
      shift = false;
      break;
    case KEY_LEFT_CTRL:
    case KEY_RIGHT_CTRL:
      ctrl = false;
      break;
    case KEY_LEFT_ALT:
    case KEY_RIGHT_ALT:
      alt = false;
      break;
  }
}

void handleKey(uint8_t key, bool released) {
    if (key != ADB_KEY_CAPS_LOCK && key != ADB_KEY_NUM_LOCK) {
      uint16_t k = adb_keycode_to_arduino_hid[key];
      if (released) {
        pressedKey = 0;
        handleRelease(k,0);
      }
      else {
        pressedKey = k;
        handlePress(k,0);
      }
    }
    else if (key == ADB_KEY_CAPS_LOCK) {
      capsLock = !released;
    }
    else {
      if (! released)
        numLock = !numLock;
    }
}

void keyboard_handler() {
    bool error = false;

    auto key_press = adb_keyboard_read_key_press(&error);

#if 0
    if (!error) {
      char s[256];
      sprintf(s,"%x:%02x %x:%02x", key_press.data.released0, key_press.data.key0, key_press.data.released1, key_press.data.key1);
      Serial.println(s);
    }
#endif    

    if (error) {
      if (pressedKey && millis() >= repeatTargetTime) {
        handlePress(pressedKey, 1);
      }
      return;      
      // don't continue changing the hid report if there was
    }
                        // an error reading from ADB – most often it's a timeout
//    Serial.println(key_press.raw,HEX);

#ifdef POWER_EMULATES_RIGHT_MOUSE
    if (mouse_present) {
     /* if (key_press.raw == ADB_KEY_POWER_DOWN)
        Mouse.press(MOUSE_RIGHT);
      else if (key_press.raw == ADB_KEY_POWER_UP)
        Mouse.release(MOUSE_RIGHT); */
    }
#endif    
    if (key_press.raw == ADB_KEY_POWER_DOWN) {
      //kb->press(KEY_MUTE);
    }
    else if (key_press.raw == ADB_KEY_POWER_UP) {
      //kb->release(KEY_MUTE);
    }
    else 
    {
      handleKey(key_press.data.key0, key_press.data.released0);
      if (key_press.data.key1 != 0x7F)
        handleKey(key_press.data.key1, key_press.data.released1);
    }
}

void mouse_handler() {
#if 0 // TODO
    bool error = false;
    auto mouse_data = adb_mouse_read_data(&error);

    if (error || mouse_data.raw == 0) return;

    int8_t mouse_x = ADB_MOUSE_CONV_AXIS(mouse_data.data.x_offset);
    int8_t mouse_y = ADB_MOUSE_CONV_AXIS(mouse_data.data.y_offset);
    bool button = 0 == mouse_data.data.button;

    if (button) {
      if (!Mouse.isPressed())
        Mouse.press(); 
    }
    else {
      if (Mouse.isPressed())
        Mouse.release();
    }

    Mouse.move(mouse_x, mouse_y);
#endif    
}

void led_handler() {
    static bool lastCapsLock = 0;
    static bool lastNumLock = 0;
    if (capsLock != lastCapsLock || numLock != lastNumLock) {
      delayMicroseconds(500);
      adb_keyboard_write_leds(false,capsLock,numLock);
      lastCapsLock = capsLock;
      lastNumLock = numLock;
    }  
}

void loop() {
    if (keyboard_present) {
        keyboard_handler();
        led_handler();
        // Wait a tiny bit before polling again,
        // while ADB seems fairly tolerent of quick requests
        // we don't want to overwhelm USB either
        delay(POLL_DELAY);
    }
/*
    if (mouse_present) {
        mouse_handler();
        delay(POLL_DELAY);
    } */
}
