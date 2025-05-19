#include <Bounce2.h>
#include <EEPROM.h>
#include <virtuabotixRTC.h>
#include <math.h>

// Pins
#define BUTTON_PIN 2
#define PWM_PIN    9     // controle PWM da lâmpada
#define LED_PIN   13

#define RTC_CLK 3       // CLK e DAT invertidos no módulo
#define RTC_DAT 7
#define RTC_RST 8

// Timing
#define LONG_PRESS_MS      1000     // 1 s long press
#define DIAGNOSTIC_MS      5000     // 5 s diagnostic press
#define ADJUST_DURATION_S  10       // fade curto: 10 s
#define FADE_DURATION_S    3600UL   // fade longo: 1 h
#define UPDATE_INTERVAL_MS 20       // loop a cada 20 ms

// EEPROM
#define EEPROM_BRIGHTNESS_ADDR  0

// Curva perceptual (gamma) para suavizar variação em baixos níveis
#define GAMMA_EXPONENT         2.0f

enum States { OFF, FADE_ON, ON, FADE_OFF, ADJUSTING };
States state = OFF;

Bounce button = Bounce();
virtuabotixRTC myRTC(RTC_DAT, RTC_CLK, RTC_RST);

unsigned long pressStartTime = 0;
bool buttonActive            = false;

int currentBrightness = 0;
int initialBrightness = 0;
int targetBrightness  = 0;
int longPressTarget   = 0;

unsigned long fadeStartTime  = 0;
unsigned long fadeDurationMs = 0;

unsigned long lastLEDToggle  = 0;
unsigned long lastLoopUpdate = 0;

bool increasing      = true;
int  lastHourChecked = -1;

// ——— Helpers ——— 

// mantém valor entre lo e hi
int constrainValue(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

// calcula duração proporcional de fade
unsigned long calcProportionalFadeDuration(int startVal, int endVal, unsigned long fullDurS) {
  int dist = abs(endVal - startVal);
  if (dist == 0) return 0;
  float frac = (float)dist / 100.0f;
  return (unsigned long)(frac * fullDurS + 0.5f);
}

// inicia fade de startVal→endVal em durS segundos
void startFade(int startVal, int endVal, unsigned long durS) {
  if (startVal == endVal) {
    fadeDurationMs = 0;
    return;
  }
  initialBrightness = startVal;
  targetBrightness  = endVal;
  fadeStartTime     = millis();
  fadeDurationMs    = durS * 1000UL;
}

// gera duty invertido (hardware ativo alto) com curva gamma
uint8_t computeDuty(int brightness) {
  float n = brightness / 100.0f;
  uint8_t duty = (uint8_t)(pow(n, GAMMA_EXPONENT) * 255 + 0.5f);
  // invertido porque o módulo ativa MOSFET em nível alto
  return 255 - duty;
}

// ——— Setup ——— 

void setup() {
  // 1) PWM ~3.9 kHz em OC1A (pino 9)
  pinMode(PWM_PIN, OUTPUT);
  TCCR1B = (TCCR1B & 0b11111000) | 0x02;

  // 2) LED indicador e botão
  pinMode(LED_PIN, OUTPUT); digitalWrite(LED_PIN, LOW);
  button.attach(BUTTON_PIN, INPUT_PULLUP);
  button.interval(10);

  // 3) Carrega brilho salvo
  int saved = EEPROM.read(EEPROM_BRIGHTNESS_ADDR);
  saved = constrainValue(saved, 0, 100);
  currentBrightness = saved;
  longPressTarget   = (saved == 0) ? 0 : 100;

  // 4) Ajusta RTC para 23:00 toda vez que liga
  myRTC.setDS1302Time(0, 0, 23, 6, 29, 3, 2025);
  myRTC.updateTime();
  int h = myRTC.hours;

  // 5) Estado inicial conforme horário
  if (h >= 20 || h < 11) {
    // forçar OFF imediato
    currentBrightness = 0;
    fadeDurationMs    = 0;
    state             = OFF;
    analogWrite(PWM_PIN, computeDuty(0));
  }
  else {
    // entre 11h e 20h → ON imediato
    currentBrightness = 100;
    fadeDurationMs    = 0;
    state             = ON;
    analogWrite(PWM_PIN, computeDuty(100));
  }

  lastHourChecked = h;
}

// ——— Loop ——— 

void loop() {
  unsigned long now = millis();
  if (now - lastLoopUpdate < UPDATE_INTERVAL_MS) return;
  lastLoopUpdate = now;

  button.update();
  handleButton();
  updateBrightness();
  updateLED();
  checkAutomaticFade();
}

// ——— Botão ——— 

void handleButton() {
  if (button.rose()) {
    buttonActive   = true;
    pressStartTime = millis();
  }
  if (button.fell() && buttonActive) {
    buttonActive = false;
    unsigned long dur = millis() - pressStartTime;

    if (dur >= DIAGNOSTIC_MS)      performDiagnosticBlink();
    else if (dur >= LONG_PRESS_MS) togglePowerState();
    else                           adjustBrightness();
  }
}

// liga/desliga com fade proporcional
void togglePowerState() {
  if (currentBrightness == longPressTarget)
    longPressTarget = (longPressTarget == 0) ? 100 : 0;

  unsigned long needS = calcProportionalFadeDuration(currentBrightness, longPressTarget, FADE_DURATION_S);
  startFade(currentBrightness, longPressTarget, needS);

  if (longPressTarget == 100) { state = FADE_ON;  increasing = true;  }
  else                        { state = FADE_OFF; increasing = false; }
}

// ajuste manual ±20%
void adjustBrightness() {
  if (increasing) {
    targetBrightness = constrainValue(currentBrightness + 20, 0, 100);
    if (targetBrightness == 100) increasing = false;
  } else {
    targetBrightness = constrainValue(currentBrightness - 20, 0, 100);
    if (targetBrightness == 0) increasing = true;
  }

  startFade(currentBrightness, targetBrightness, ADJUST_DURATION_S);

  if (fadeDurationMs == 0) {
    EEPROM.update(EEPROM_BRIGHTNESS_ADDR, currentBrightness);
    state = (currentBrightness == 0) ? OFF : ON;
  } else {
    longPressTarget = (targetBrightness > currentBrightness) ? 100 : 0;
    state = ADJUSTING;
  }
}

// ——— Atualiza brilho ——— 

void updateBrightness() {
  if (fadeDurationMs == 0) return;

  unsigned long elapsed = millis() - fadeStartTime;
  float prog = (float)elapsed / (float)fadeDurationMs;
  if (prog > 1.0f) prog = 1.0f;

  float interp = initialBrightness + (targetBrightness - initialBrightness) * prog;
  currentBrightness = constrainValue((int)(interp + 0.5f), 0, 100);

  analogWrite(PWM_PIN, computeDuty(currentBrightness));

  if (prog >= 1.0f || currentBrightness == targetBrightness) {
    currentBrightness = targetBrightness;
    fadeDurationMs    = 0;
    EEPROM.update(EEPROM_BRIGHTNESS_ADDR, currentBrightness);
    if (currentBrightness == longPressTarget)
      state = (currentBrightness == 0) ? OFF : ON;
    else
      state = (currentBrightness > 0) ? ON : OFF;
  }
}

// ——— LED de status ——— 

void updateLED() {
  switch (state) {
    case FADE_ON:
    case FADE_OFF:
      digitalWrite(LED_PIN, HIGH);
      break;
    case ADJUSTING:
      if (millis() - lastLEDToggle > 250) {
        lastLEDToggle = millis();
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      }
      break;
    default:
      digitalWrite(LED_PIN, LOW);
  }
}

// ——— Fade automático ——— 

void checkAutomaticFade() {
  myRTC.updateTime();
  int h = myRTC.hours;
  if (h == lastHourChecked) return;

  if (h == 11 && state != FADE_ON && currentBrightness < 100) {
    unsigned long d = calcProportionalFadeDuration(currentBrightness, 100, FADE_DURATION_S);
    startFade(currentBrightness, 100, d);
    state = FADE_ON; increasing = true; longPressTarget = 100;
  }
  if (h == 20 && state != FADE_OFF && currentBrightness > 0) {
    unsigned long d = calcProportionalFadeDuration(currentBrightness, 0, FADE_DURATION_S);
    startFade(currentBrightness, 0, d);
    state = FADE_OFF; increasing = false; longPressTarget = 0;
  }

  lastHourChecked = h;
}

// ——— Diagnóstico ——— 

void performDiagnosticBlink() {
  int blinks = (currentBrightness == 0) ? 2
             : (currentBrightness == 100) ? 4
             : 0;
  for (int i = 0; i < blinks; i++) {
    digitalWrite(LED_PIN, HIGH); delay(200);
    digitalWrite(LED_PIN, LOW);  delay(200);
  }
}
