#include <LiquidCrystal.h>

#define GOD false

#define PIN_RADAR_ECHO A0
#define PIN_RADAR_TRIG A1
#define PIN_BUZZER A2

typedef unsigned long ulong;

const int rs = 6, en = 5, d4 = 9, d5 = 10, d6 = 11, d7 = 12;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

const unsigned int
  TIMER_REDRAW_DISPLAY = 500
  , TIMER_MEASURE_RADAR = 1000
  , TIMER_BELIEVER_PRESENT = 2000
  , TIMER_INITIALIZING_PRAYER = 2000
  , TIMER_BAD_BELIEVER_LEAVING = 2000
  , TIMER_DONE_PRAYING_BEEP = 500
  , TIMER_DONE_PRAYING = 2000
  , PRAYER_TIME = 3000
  ;

enum prayer_status
{
    /**
   * Check, if someone is nearby: at some distance from radar -> next state
   */
    NO_BELIEVER,
    /**
   * Continue to measure distance and check if the person stays close long enough,
   * so we know he is not passing by but waiting for prayer.
   * 
   * timer: person stays still under the cross for 2 seconds -> next state 
   */
    BELIEVER_PRESENT,
    /**
   * Welcoming the believer. If he leaves, back to NO_BELIEVER state.
   * 
   * timer: the time we will display the welcoming lines.
   */
    INITIALIZING_PRAYER,
    /**
   * Count down for the praying. Soothing text.
   * If believer leaves before the countdown finishes: BAD_BELIEVER_LEAVING state.
   * 
   * timer: praying countdown. 
   */
    PRAYING,
    /**
   * After the praying countdown finishes, confirming beep sounds and then we go to next state.
   * 
   * timer: for the beep sound.
   */
    DONE_PRAYING_BEEP,
    /**
   * You have been forgiven kind of text. Goes to next state after the user 
   * leaves (NO_BELIEVER) or after the time is up (EXODUS).
   * 
   * timer: the time we will allow the believer to stay under the cross.
   */
    DONE_PRAYING,
    /**
   * Gets to this state is the praying is interrupted by the believer leaving. 
   * Text Dont leave, keep praying. Annoying beep.
   * From this state we can either go to PRAYING or to NO_BELIEVER.
   * 
   * timer: how long we will stay in this state. 
   */
    BAD_BELIEVER_LEAVING,
    /**
   * If the believer stays for long under the cross even after he has finished his prayer,
   * he will be told to leave and let other believers pray.
   * 
   * timer: time until switch to NO_BELIEVER.
   */
    EXODUS
} prayer_state = NO_BELIEVER;

struct timers
{
  ulong DISPLAY_REDRAW;
  ulong RADAR_MEASURE;
  ulong NO_BELIEVER;
  ulong BELIEVER_PRESENT;
  ulong INITIALIZING_PRAYER;
  ulong PRAYING;
  ulong DONE_PRAYING_BEEP;
  ulong DONE_PRAYING;
  ulong BAD_BELIEVER_LEAVING;
  ulong EXODUS;
} timers = { 0 };

float 
  radar_time = 0, 
  radar_distance = 0;
bool isBeeping = false;
int time_prayed = 0;

void setup() {
    lcd.begin(16, 2);
    lcd.print("popros pana o odpusteni!");
    pinMode(PIN_RADAR_TRIG, OUTPUT);
    pinMode(PIN_RADAR_ECHO, INPUT);
    pinMode(PIN_BUZZER, OUTPUT);
}

void loop() {
    ulong t_now = millis();

    digitalWrite(PIN_BUZZER, isBeeping ? HIGH : LOW);

    if (t_now - timers.DISPLAY_REDRAW >= TIMER_REDRAW_DISPLAY) {
        lcd.clear();
        timers.DISPLAY_REDRAW = t_now;
        switch (prayer_state) {
        case NO_BELIEVER:
            lcd.print("NO_BELIEVER");
            break;
            
        case BELIEVER_PRESENT:
            lcd.print("BELIEVER_PRESENT");
            break;
        
        case INITIALIZING_PRAYER: 
            lcd.print("INITIALIZING_PRAYER");
            break;
        
        case PRAYING:
            lcd.print("PRAYING");
            break;
            
        case DONE_PRAYING_BEEP:
            lcd.print("DONE_BEEP");
            break;
            
        case DONE_PRAYING: 
            lcd.print("DONE_PRAYING");
            break;
        
        case BAD_BELIEVER_LEAVING: 
            lcd.print("BAD_BELIEVER_LEAVING");
            break;
        
        case EXODUS:
            lcd.print("EXODUS");
            break;
        }
    }

    if (t_now - timers.RADAR_MEASURE >= TIMER_MEASURE_RADAR) {
        digitalWrite(PIN_RADAR_TRIG, LOW);
        delayMicroseconds(2);
        digitalWrite(PIN_RADAR_TRIG, HIGH);
        delayMicroseconds(10);
        digitalWrite(PIN_RADAR_TRIG, LOW);
        delayMicroseconds(2);
        radar_time = pulseIn(PIN_RADAR_ECHO, HIGH);
        radar_distance = radar_time * 340 / 20000;
    }

    switch (prayer_state) {
    case NO_BELIEVER:
        if (isBelieverPresent(radar_distance)) {
            timers.BELIEVER_PRESENT = t_now;
            prayer_state = BELIEVER_PRESENT;
        }
        break;

    case BELIEVER_PRESENT:
        if (!isBelieverPresent(radar_distance)) {
            prayer_state = NO_BELIEVER;
        } else if (t_now - timers.BELIEVER_PRESENT >= TIMER_BELIEVER_PRESENT) {
            prayer_state = INITIALIZING_PRAYER;
            timers.INITIALIZING_PRAYER = t_now;
        }
        break;

    case INITIALIZING_PRAYER:
        if (!isBelieverPresent(radar_distance)) {
            prayer_state = NO_BELIEVER;
        }
        if (t_now - timers.INITIALIZING_PRAYER >= TIMER_INITIALIZING_PRAYER) {
            timers.PRAYING = t_now;
            time_prayed = 0;
            prayer_state = PRAYING;
        }
        break;

    case PRAYING:
        if (!isBelieverPresent(radar_distance)) {
            timers.BAD_BELIEVER_LEAVING = t_now;
            prayer_state = BAD_BELIEVER_LEAVING;
            break;
        }

        time_prayed += t_now - timers.PRAYING;
        timers.PRAYING = t_now;

        if (time_prayed >= PRAYER_TIME) {
            prayer_state = DONE_PRAYING_BEEP;
            timers.DONE_PRAYING_BEEP = t_now;
            break;
        }
        break;

    case DONE_PRAYING_BEEP:
        isBeeping = true;
        if (t_now - timers.DONE_PRAYING_BEEP > TIMER_DONE_PRAYING_BEEP) {
            isBeeping = false;
            timers.DONE_PRAYING = t_now;
            prayer_state = DONE_PRAYING;
        }
        break;

    case DONE_PRAYING:
        if (t_now - timers.DONE_PRAYING > TIMER_DONE_PRAYING) {
            if (!isBelieverPresent(radar_distance)) {
                prayer_state = NO_BELIEVER;
            } else {
                prayer_state = EXODUS;
            }
        }
        break;

    case BAD_BELIEVER_LEAVING:
        isBeeping = true;
        if (isBelieverPresent(radar_distance)) {
            isBeeping = false;
            timers.PRAYING = t_now;
            prayer_state = PRAYING;
            break;
        }
        if (t_now - timers.BAD_BELIEVER_LEAVING > TIMER_BAD_BELIEVER_LEAVING) {
            isBeeping = false;
            prayer_state = NO_BELIEVER;
            break;
        }
        break;

    case EXODUS:
        isBeeping = true;
        if (!isBelieverPresent(radar_distance)) {
            isBeeping = false;
            prayer_state = NO_BELIEVER;
        }
        break;
    }
}

bool isBelieverPresent(float dist) {
    return dist < 80;
}
