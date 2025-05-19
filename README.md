# Aquarium-lighting v2

Automação e controle avançado das luzes de um aquário usando Arduino UNO, módulo JZ-MOS e PWM para dimming suave, com controle manual e transições automáticas baseadas em RTC, armazenamento em EEPROM e diagnóstico integrado.

---

## Português

### 🔧 Funcionalidades Principais (v2)

- **Controle de brilho por PWM** via **módulo JZ-MOS** em vez de servo, para resposta rápida e maior eficiência
- **Curva perceptual (Gamma)** para mapeamento não linear de 0–100% de brilho, garantindo ajustes suaves em baixos níveis
- **Fades proporcionais**: tempo de transição proporcional à distância entre níveis atuais e alvo
- **Ajuste manual** via botão:
  - Curto (<1 s): incrementa/decrementa 20% de brilho (alternando entre subir e descer)
  - Longo (≥1 s): liga/desliga com fade proporcional à distância do nível atual
  - Muito longo (≥5 s): modo diagnóstico via piscadas do LED externo
- **Transições automáticas** com RTC DS1302:
  - 11:00 AM → fade até 100%
  - 08:00 PM → fade até 0%
- **Sincronização de horário**: ao ligar, o RTC é inicializado para 23:00 em 29/03/2025 (ajustável)
- **Memória de brilho**: nível atual salvo e recarregado da EEPROM a cada final de fade
- **Indicador de estado** com LED externo no pino 13:
  - FADE_ON/FADE_OFF → LED sempre aceso
  - ADJUSTING → LED piscando a 4 Hz
  - OFF/ON estático → LED apagado
- **Taxa de atualização do loop** configurada em 20 ms (50 Hz), para responsividade

### 📦 Requisitos de Hardware

- Arduino UNO (ou compatível)
- **Módulo JZ-MOS para controle de motor** (usa MOSFET interno e driver de gate)
- Lâmpada ou módulo de LED dimmable conectado à saída do JZ-MOS
- Módulo RTC DS1302
- Botão com pull-up interno
- LED externo + resistor de 220 Ω no pino 13
- Jumpers e protoboard

### 🖥️ Pinout Atualizado

| Componente      | Pino Arduino |
|-----------------|--------------|
| Botão           | D2           |
| PWM (JZ-MOS)    | D9 (OC1A)    |
| LED externo     | D13          |
| RTC CLK         | D3           |
| RTC DAT         | D7           |
| RTC RST         | D8           |

> **Observação:** o PWM no pino 9 opera a ~3,9 kHz em OC1A para minimizar flicker em lâmpadas.

### ⚙️ Detalhes de Software

- **Gamma Exponent**: `GAMMA_EXPONENT = 2.0f` — ajuste em `#define` para calibrar resposta perceptual.
- **Duração de fades**:
  - Curto (ajuste manual): `ADJUST_DURATION_S = 10` s
  - Longo (liga/desliga): `FADE_DURATION_S = 3600` s (1 h)
- **Intervalo de loop**: `UPDATE_INTERVAL_MS = 20` ms
- **Endereço EEPROM**: byte único em `0x00` para salvar nível 0–100

### 🔍 Diagnóstico

Pressione o botão por ≥5 s para o LED piscar:

- **2 piscadas**: brilho = 0%
- **4 piscadas**: brilho = 100%

### 📚 Bibliotecas Utilizadas

- [Bounce2](https://github.com/thomasfredericks/Bounce2)
- [EEPROM](https://www.arduino.cc/en/Reference/EEPROM)
- [virtuabotixRTC (DS1302)](https://github.com/virtuabotix/DS1302)

---

## English

### 🔧 Main Features (v2)

- **PWM dimming** via **JZ-MOS motor control module** replacing servo for faster response and efficient power handling
- **Perceptual gamma curve** mapping 0–100% brightness to PWM duty for smooth low-end fades
- **Proportional fades**: transition time scales with difference between current and target levels
- **Manual control** via button:
  - Short (<1 s): ±20% brightness (toggles direction)
  - Long (≥1 s): fade ON/OFF proportional to the level gap
  - Very long (≥5 s): external LED diagnostic mode
- **Automatic transitions** with DS1302 RTC:
  - 11:00 AM → fade to 100%
  - 08:00 PM → fade to 0%
- **RTC sync** on power-up to 11 PM, 03/29/2025 (configurable)
- **Brightness memory**: stores level in EEPROM after each fade
- **Status LED** on pin 13:
  - FADE_ON/FADE_OFF → LED always ON
  - ADJUSTING → LED blinking at 4 Hz
  - OFF/ON → LED OFF
- **Loop update rate**: 20 ms for responsive control

### 📦 Hardware Requirements

- Arduino UNO (or compatible)
- **JZ-MOS motor control module** (integrated MOSFET + gate driver)
- Dimmable lamp or LED module connected to JZ-MOS output
- DS1302 RTC module
- Push-button with internal pull-up
- External LED + 220 Ω resistor on pin 13
- Jumper wires & breadboard

### 🖥️ Updated Pinout

| Component     | Arduino Pin |
|---------------|-------------|
| Button        | D2          |
| PWM (JZ-MOS)  | D9 (OC1A)   |
| External LED  | D13         |
| RTC CLK       | D3          |
| RTC DAT       | D7          |
| RTC RST       | D8          |

> **Note:** PWM on D9 runs at ~3.9 kHz (OC1A) to reduce flicker.

### ⚙️ Software Details

- **Gamma Exponent**: `#define GAMMA_EXPONENT 2.0f` — tweak for perceptual curve.
- **Fade durations**:
  - Manual: `ADJUST_DURATION_S = 10` s
  - Power ON/OFF: `FADE_DURATION_S = 3600` s
- **Loop interval**: `UPDATE_INTERVAL_MS = 20` ms
- **EEPROM address**: `0x00` for one byte storing 0–100

### 🔍 Diagnostic Mode

Hold button ≥5 s to blink external LED:

- **2 blinks**: brightness = 0%
- **4 blinks**: brightness = 100%

### 📚 Libraries Used

- Bounce2
- EEPROM
- virtuabotixRTC (DS1302)
