#include <LiquidCrystal.h>
#include "LcdProgressBar.h"

#define GOD false
#define LAST_DISTANCE_COUNT 23
#define DISPLAY_COLS_NUM 16

#define PIN_RADAR_ECHO A0
#define PIN_RADAR_TRIG A1
#define PIN_BUZZER 13
#define PIN_WHITE_LED 3

typedef unsigned long ulong;

const int rs = 6, en = 5, d4 = 9, d5 = 10, d6 = 11, d7 = 12;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
LcdProgressBar lpg(&lcd, 1, DISPLAY_COLS_NUM);

const unsigned int
  TIMER_REDRAW_DISPLAY = 200
  , TIMER_MEASURE_RADAR = 30
  , TIMER_BELIEVER_PRESENT = 2500
  , TIMER_INITIALIZING_PRAYER = 2500
  , TIMER_BAD_BELIEVER_LEAVING = 3500
  , TIMER_DONE_PRAYING_BEEP = 100
  , TIMER_DONE_PRAYING = 3500
  , PRAYER_TIME = 10000
  , DISTANCE_HIGH = 70
  ;

enum prayer_state
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
  ulong PROGRESS_BAR;
} timers = { 0 };

float 
  radar_time = 0, 
  radar_distance = 0;
  
int current_last_distance = 0;
float last_distances[LAST_DISTANCE_COUNT] = { 0 };

bool isBeeping = false;
int time_prayed = 0;
bool lcd_redraw_once = false;

int whiteLightLevel = 0;
int prayer_cost = 0;

void setup() {
#ifdef DEBUG
    Serial.begin(4800);
#endif
    lcd.begin(16, 2);

    pinMode(PIN_RADAR_TRIG, OUTPUT);
    pinMode(PIN_RADAR_ECHO, INPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_WHITE_LED, OUTPUT);
    
    init_progress_bar();
}

void display_draw() {
    if (!lcd_redraw_once) {
        return;
    }
    
    lcd.clear();
    lcd_redraw_once = false;
        
    switch (prayer_state) {
    case NO_BELIEVER:
        lcd.print("No fear,");
        lcd.setCursor(0, 1);
        lcd.print("come here.");
        break;
        
    case BELIEVER_PRESENT:
        lcd.print("Connecting to");
        lcd.setCursor(0,1);
        lcd.print("god...");
        break;
    
    case INITIALIZING_PRAYER: 
        lcd.print("Welcome,");
        lcd.setCursor(0, 1);
        lcd.print("my child.");
        break;
    
    case PRAYING:
        lcd.print("Now pray.");
        lcd.setCursor(0, 1);
        lpg.draw(timers.PROGRESS_BAR);
        break;
        
    case DONE_PRAYING_BEEP:
        lcd.print("Sins forgiven.");
        break;
        
    case DONE_PRAYING: 
        lcd.print("Sins forgiven.");
        lcd.setCursor(0, 1);
        lcd.print("Price: ");
        lcd.print(prayer_cost);
        lcd.print(" EUR");
        break;
    
    case BAD_BELIEVER_LEAVING: 
        lcd.print("Come back,");
        lcd.setCursor(0, 1);
        lcd.print("sinner!");
        break;
    
    case EXODUS:
        lcd.print("Now go away!");
        break;
    }
}

void loop() {
    ulong t_now = millis();

    analogWrite(PIN_WHITE_LED, whiteLightLevel);
    digitalWrite(PIN_BUZZER, isBeeping ? HIGH : LOW);

    if (t_now - timers.DISPLAY_REDRAW >= TIMER_REDRAW_DISPLAY) {
        timers.DISPLAY_REDRAW = t_now;
        display_draw();
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
        if (whiteLightLevel > 0) {
            double diff = 2;
            whiteLightLevel = whiteLightLevel - diff < 1 ? 0 : whiteLightLevel - diff;
        }
        
        if (isBelieverPresent(radar_distance)) {
            timers.BELIEVER_PRESENT = t_now;
            set_state(BELIEVER_PRESENT);
        }
        break;

    case BELIEVER_PRESENT:
        if (whiteLightLevel < 254) {
            double diff = 1;
            whiteLightLevel = whiteLightLevel + diff > 254 ? 254 : whiteLightLevel + diff;
        }
        if (!isBelieverPresent(radar_distance)) {
            set_state(NO_BELIEVER);
        } else if (t_now - timers.BELIEVER_PRESENT >= TIMER_BELIEVER_PRESENT) {
            set_state(INITIALIZING_PRAYER);
            timers.INITIALIZING_PRAYER = t_now;
        }
        break;

    case INITIALIZING_PRAYER:
        if (!isBelieverPresent(radar_distance)) {
            set_state(NO_BELIEVER);
        }
        if (t_now - timers.INITIALIZING_PRAYER >= TIMER_INITIALIZING_PRAYER) {
            timers.PRAYING = t_now;
            time_prayed = 0;
            init_progress_bar();
            set_state(PRAYING);
        }
        break;

    case PRAYING:
        if (!isBelieverPresent(radar_distance)) {
            timers.BAD_BELIEVER_LEAVING = t_now;
            set_state(BAD_BELIEVER_LEAVING);
            break;
        }
        
        time_prayed += t_now - timers.PRAYING;
        timers.PRAYING = t_now;
        
        lpg.draw(t_now);
        
        if (time_prayed >= PRAYER_TIME) {
            timers.DONE_PRAYING_BEEP = t_now;
            set_state(DONE_PRAYING_BEEP);
            break;
        }
        break;

    case DONE_PRAYING_BEEP:
        prayer_cost = random(5, 200);
        isBeeping = true;
        if (t_now - timers.DONE_PRAYING_BEEP > TIMER_DONE_PRAYING_BEEP) {
            isBeeping = false;
            timers.DONE_PRAYING = t_now;
            set_state(DONE_PRAYING);
        }
        break;

    case DONE_PRAYING:
        if (t_now - timers.DONE_PRAYING > TIMER_DONE_PRAYING) {
            if (!isBelieverPresent(radar_distance)) {
                set_state(NO_BELIEVER);
            } else {
                set_state(EXODUS);
            }
        }
        break;

    case BAD_BELIEVER_LEAVING:
        isBeeping = true;
        if (isBelieverPresent(radar_distance)) {
            isBeeping = false;
            timers.PRAYING = t_now;
            set_state(PRAYING);
            break;
        }
        if (t_now - timers.BAD_BELIEVER_LEAVING > TIMER_BAD_BELIEVER_LEAVING) {
            isBeeping = false;
            set_state(NO_BELIEVER);
            break;
        }
        break;

    case EXODUS:
        isBeeping = true;
        if (!isBelieverPresent(radar_distance)) {
            isBeeping = false;
            set_state(NO_BELIEVER);
        }
        break;
    }
    
#ifdef DEBUG
    Serial.print(radar_distance);
    Serial.print(" ");
    Serial.print(isBelieverPresent(radar_distance) ? "present" : "absent");
    Serial.print(" light:");
    Serial.print(whiteLightLevel);
    Serial.print(" praying:");
    Serial.print(time_prayed);
    Serial.println("");
#endif
}

void init_progress_bar()
{
  timers.PROGRESS_BAR = millis();
  lpg.setMinValue(timers.PROGRESS_BAR);
  lpg.setMaxValue(timers.PROGRESS_BAR + PRAYER_TIME);
  lpg.draw(timers.PROGRESS_BAR);
}

void set_state(enum prayer_state new_state) {
    prayer_state = new_state;
    lcd_redraw_once = true;
}

bool isBelieverPresent(float dist) {
    if (current_last_distance < LAST_DISTANCE_COUNT) {
        current_last_distance++;
        return dist < DISTANCE_HIGH;
    }
    last_distances[LAST_DISTANCE_COUNT - 1] = dist;
    
    for (uint8_t i = 0; i < LAST_DISTANCE_COUNT - 1; i++) {
        last_distances[i] = last_distances[i + 1];
    }
    float dist_med = median(LAST_DISTANCE_COUNT, last_distances);
    return dist_med < DISTANCE_HIGH;
}

float mean(int m, int a[]) {
    int sum=0, i;
    for(i=0; i<m; i++)
        sum+=a[i];
    return((float)sum/m);
}

float median(int n, float x[]) {
    int temp;
    int i, j;
    // the following two loops sort the array x in ascending order
    for(i=0; i<n-1; i++) {
        for(j=i+1; j<n; j++) {
            if(x[j] < x[i]) {
                // swap elements
                temp = x[i];
                x[i] = x[j];
                x[j] = temp;
            }
        }
    }

    if(n%2==0) {
        // if there is an even number of elements, return mean of the two elements in the middle
        return((x[n/2] + x[n/2 - 1]) / 2.0);
    } else {
        // else return the element in the middle
        return x[n/2];
    }
}
