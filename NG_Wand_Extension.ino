#define TRIAC_PIN      0
#define STAT_LED_PIN   3
#define ACT_LED_PIN    4
#define ZERO_CROSS_PIN 1
#define PWM_IN_PIN     2

#define PWM_STOP_TIMEOUT_US 10000

void setup() {
  GIMSK = 0b01100000;    // turns on pin change interrupts
  PCMSK = (1 << ZERO_CROSS_PIN); // | (1 << PWM_IN_PIN);    // turn on interrupts on pins
  sei(); // Enable Interrupt

  attachInterrupt(0, pwmInterrupt, CHANGE);
    
  pinMode(TRIAC_PIN, OUTPUT);
  pinMode(STAT_LED_PIN, OUTPUT);
  pinMode(ACT_LED_PIN, OUTPUT);
  pinMode(ZERO_CROSS_PIN, INPUT);
  pinMode(PWM_IN_PIN, INPUT);

  digitalWrite(TRIAC_PIN, LOW);
  digitalWrite(STAT_LED_PIN, HIGH);
}

long pwm_high = 0;
long pwm_low = 0;
volatile int count = 0;
volatile byte speed_val = 0;

volatile bool last_zero_change = LOW;
volatile unsigned long zero_pk_millis = 0;
volatile unsigned long zero_pk_length = 0;
volatile unsigned long last_zero_detect = 0;
unsigned long gate_trigger_at = 0;

// Calculate PWM width
unsigned long pwm_rise_at = 0;
unsigned long pwm_fall_at = 0;
unsigned long pwm_high_time = 0;
unsigned long pwm_cycle_time = 0;
float pwm_duty = 0;
bool last_pwm = LOW;

bool stat_on = false;
unsigned long last_flash = 0;

void loop() {
  // First, trigger out gate:
  if (gate_trigger_at > 0 && micros() > gate_trigger_at) {
    gate_trigger_at = 0;
    digitalWrite(TRIAC_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIAC_PIN, LOW);
  }
  
  if (pwm_rise_at > 0 && micros() - pwm_rise_at > PWM_STOP_TIMEOUT_US) {
    speed_val = 0;
  } else if (pwm_fall_at > 0 && micros() - pwm_fall_at > PWM_STOP_TIMEOUT_US) {
    speed_val = 255;
  } else {
    speed_val = floor(255.0f * pwm_duty);
  }
  
  analogWrite(ACT_LED_PIN, speed_val);

  if (millis() - last_flash >= 1000) {
    digitalWrite(STAT_LED_PIN, !stat_on);
    stat_on = !stat_on;
    last_flash = millis();
  }
} 

// Calcualte PWM cycle time:
void pwmInterrupt() {
  unsigned long start_time = micros();
  bool pwm_value  = PINB & (1 << PWM_IN_PIN);

  if (pwm_value != last_pwm) {
    // PWM change, trigger appropriately.
    if (pwm_value == LOW) { // Rising Edge
      // finalize this cycle's calculation
      pwm_cycle_time = pwm_high_time + (start_time - pwm_fall_at);
      pwm_duty = (float)pwm_high_time / (float)pwm_cycle_time;
      pwm_rise_at = start_time;
    } else { // Falling edge
      pwm_fall_at = start_time;
      pwm_high_time = pwm_fall_at - pwm_rise_at;
    }

    last_pwm = pwm_value;
    return;
  }
}

//     ___   
// ___|   |___|

ISR(PCINT0_vect) {
  unsigned long chop_time;
  unsigned long start_time = micros();
  bool zero_value = PINB & (1 << ZERO_CROSS_PIN);

  // Detect zero crossing change:
  if (zero_value != last_zero_change) {
    last_zero_change = zero_value;

    // Adjust AC frequency on falling edge of Zero Cross:
    if (zero_value == LOW) {
      if (zero_pk_length == 0) {
        if (zero_pk_millis == 0) {
          zero_pk_millis = micros();
          return;
        } else {
          // Calculate peak-to-peak time and reset
          zero_pk_length = (micros() - zero_pk_millis) / 2;
          zero_pk_millis = 0;
        }
      }
      
      if (speed_val == 0) {
        digitalWrite(TRIAC_PIN, LOW);
      } else if (speed_val >= 255) {
        digitalWrite(TRIAC_PIN, HIGH);
      } else {  
        chop_time = (float)zero_pk_length * (1.0f - ((float)speed_val / 255));
        gate_trigger_at = start_time + chop_time;
      }
    }
  }
}
