#include <Arduino.h>
#include <Keypad.h>
#include <ESP32Synth.h>
#include <ESP32SynthNotes.h> 

ESP32Synth synth;

// Definição dos pinos do I2S DAC
#define I2S_DOUT 25
#define I2S_BCK  26
#define I2S_WS   27

// CONFIGURAÇÃO DA TECLA INDEPENDENTE (C Extra)
#define PINO_C_EXTRA 5
bool estadoAnteriorC = HIGH; 

// =========================================================================
// NOVO: POTENCIÔMETRO DE VOLUME
// =========================================================================
#define PINO_POT_VOLUME 35   // Pino ADC1 (evite pinos ADC2 se usar Wi-Fi)
#define VOLUME_MIN 0
#define VOLUME_MAX 50         // Ajuste conforme o range aceito por setMasterVolume()
#define INTERVALO_LEITURA_POT 40  // ms entre leituras, evita chamadas excessivas
unsigned long ultimaLeituraPot = 0;
int volumeAtualAplicado = -1;  // força a primeira aplicação

// =========================================================================
// NOVO: BOTÃO DE TROCA DE TIMBRE
// =========================================================================
#define PINO_BOTAO_TIMBRE 34
bool estadoAnteriorTimbre = HIGH;
unsigned long ultimoDebounceTimbre = 0;
#define DEBOUNCE_TIMBRE_MS 50

// Lista de formas de onda disponíveis para alternar.
// Confira no ESP32Synth.h quais WAVE_* sua versão da lib realmente expõe.
const uint8_t wave_triangle = static_cast<uint8_t> (WAVE_TRIANGLE);
const uint8_t wave_saw = static_cast<uint8_t> (WAVE_SAW);
const uint8_t wave_sine = static_cast<uint8_t> (WAVE_SINE);


const WaveType formasOnda[] = { WAVE_SAW, WAVE_TRIANGLE, WAVE_SINE };
const uint8_t QTD_FORMAS = sizeof(formasOnda) / sizeof(formasOnda[0]);
uint8_t indiceFormaAtual = 0;

// CONFIGURAÇÃO DA MATRIZ 4x4
const byte LINS = 4;
const byte COLS = 4;
char teclas[LINS][COLS] = {
  {'5', '4', '3', '2'}, 
  {'1', 'B', 'a', 'A'}, 
  {'g', 'G', 'f', 'F'}, 
  {'E', 'd', 'D', 'c'}  
};

byte pinosLinhas[LINS] = {13, 12, 14, 32};
byte pinosColunas[COLS] = {33, 15, 4, 19};

Keypad teclado = Keypad(makeKeymap(teclas), pinosLinhas, pinosColunas, LINS, COLS);

// =========================================================================
// ESTRUTURAS E POLIFONIA
// =========================================================================
#define MAX_VOICES 4 

struct CanalSintetizador {
  char key;       
  bool inUse;     
};
CanalSintetizador meusCanais[MAX_VOICES];

// Função auxiliar com o mapeamento CORRIGIDO conforme sua especificação
float obterNota(char tecla) {
  switch (tecla) {
    // Letras Maiúsculas (Notas Naturais) e minúsculas (Sustenidos)
    case 'c': return cs4; // c minúsculo = Dó Sustenido
    case 'D': return d4;  // D maiúsculo = Ré
    case 'd': return ds4; // d minúsculo = Ré Sustenido
    case 'E': return e4;  // Mi
    case 'F': return f4;  // Fá
    case 'f': return fs4; // f minúsculo = Fá Sustenido
    case 'G': return g4;  // Sol
    case 'g': return gs4; // g minúsculo = Sol Sustenido
    case 'A': return a4;  // Lá
    case 'a': return as4; // a minúsculo = Lá Sustenido
    case 'B': return b4;  // Si

    // Sequência aguda a partir do C5
    case '1': return c5;  // Dó agudo
    case '2': return cs5; // Dó sustenido agudo
    case '3': return d5;  // Ré agudo
    case '4': return ds5; // Ré sustenido agudo
    case '5': return e5;  // Mi agudo
    
    default:  return 0; 
  }
}

// =========================================================================
// NOVO: aplica a forma de onda atual em todos os canais de voz
// =========================================================================
void aplicarTimbreEmTodosCanais() {
  WaveType novaForma = formasOnda[indiceFormaAtual];
  for (int v = 0; v < MAX_VOICES; v++) {
    synth.setWave(v, novaForma);
  }
  Serial.print("Timbre alterado para índice: ");
  Serial.println(indiceFormaAtual);
}
// =========================================================================
// NOVO: leitura equalizada (perceptual) do potenciômetro de volume
// =========================================================================
void atualizarVolume() {
  unsigned long agora = millis();
  if (agora - ultimaLeituraPot < INTERVALO_LEITURA_POT) return;
  ultimaLeituraPot = agora;

  int leituraBruta = analogRead(PINO_POT_VOLUME); // 0-4095 no ESP32
  float normalizado = leituraBruta / 4095.0f;

  // Curva quadrática: aproxima a resposta do ouvido humano (log-like),
  // fazendo o giro do potenciômetro "sentir" mais uniforme.
  float perceptual = normalizado * normalizado;

  int novoVolume = (int)(VOLUME_MIN + perceptual * (VOLUME_MAX - VOLUME_MIN));

  // Só reaplica se o valor realmente mudou, evitando chamadas redundantes
  if (novoVolume != volumeAtualAplicado) {
    volumeAtualAplicado = novoVolume;
    synth.setMasterVolume(novoVolume);
  }
}

// =========================================================================
// NOVO: leitura com debounce do botão de timbre
// =========================================================================
void verificarBotaoTimbre() {
  bool estadoAtual = digitalRead(PINO_BOTAO_TIMBRE);

  if (estadoAtual != estadoAnteriorTimbre) {
    unsigned long agora = millis();
    if (agora - ultimoDebounceTimbre > DEBOUNCE_TIMBRE_MS) {
      ultimoDebounceTimbre = agora;

      // Troca no momento em que o botão é PRESSIONADO (transição para LOW)
      if (estadoAtual == LOW) {
        indiceFormaAtual = (indiceFormaAtual + 1) % QTD_FORMAS;
        aplicarTimbreEmTodosCanais();
      }
      estadoAnteriorTimbre = estadoAtual;
    }
  }
}
// =========================================================================

void setup() {
    Serial.begin(115200);
    Serial.println("--- Inicializando Sintetizador Polifônico ---");

    synth.begin(I2S_DOUT, SMODE_I2S, I2S_BCK, I2S_WS, I2S_32BIT);
    synth.setMasterVolume(20);

    for (int i = 0; i < MAX_VOICES; i++) {
        synth.setWave(i, WAVE_SINE);
        synth.setEnv(i, 10, 200, 150, 800); 
        meusCanais[i].key = '\0';
        meusCanais[i].inUse = false;
    }
    
    pinMode(PINO_C_EXTRA, INPUT_PULLUP);

    // NOVO: inicialização do botão de timbre e ADC do potenciômetro
    pinMode(PINO_BOTAO_TIMBRE, INPUT_PULLUP);
    analogReadResolution(12); // garante 0-4095 no ESP32
    atualizarVolume();        // aplica volume inicial baseado na posição do pot

    Serial.println("Sintetizador Pronto com Escala Corrigida!");
}


void loop() {
  
  // --- 1. LEITURA DA MATRIZ (MULTITOUCH) ---
  if (teclado.getKeys()) {
    for (int i = 0; i < LIST_MAX; i++) {
      if (teclado.key[i].stateChanged) {
        char keyChar = teclado.key[i].kchar;
        float nota = obterNota(keyChar);

        if (nota == 0) continue; 

        if (teclado.key[i].kstate == PRESSED) {
          int canalAlocado = -1;
          for (int v = 0; v < MAX_VOICES; v++) {
            if (!meusCanais[v].inUse) {
              canalAlocado = v;
              break;
            }
          }

          if (canalAlocado != -1) {
            meusCanais[canalAlocado].key = keyChar;
            meusCanais[canalAlocado].inUse = true;
            synth.noteOn(canalAlocado, nota, 120);
            Serial.print("Nota LIGADA (Matriz): "); Serial.print(keyChar);
            Serial.print(" no Canal: "); Serial.println(canalAlocado);
          }
        }
        
        if (teclado.key[i].kstate == RELEASED) {
          for (int v = 0; v < MAX_VOICES; v++) {
            if (meusCanais[v].inUse && meusCanais[v].key == keyChar) {
              synth.noteOff(v); 
              meusCanais[v].inUse = false;
              meusCanais[v].key = '\0';
              Serial.print("Nota desligada (Matriz): "); Serial.print(keyChar);
              Serial.print(" do Canal: "); Serial.println(v);
            }
          }
        }
      }
    }
  }

  // --- 2. LEITURA DA TECLA INDEPENDENTE (C INICIAL DA ESCALA = c4) ---
  bool estadoAtualC = digitalRead(PINO_C_EXTRA);

  if (estadoAtualC != estadoAnteriorC) {
    delay(5); // Debounce
    char extraKeyChar = 'X'; 
    float notaExtra = c4;    // Alterado de c3 para c4 (Dó inicial perfeito antes do cs4)

    if (estadoAtualC == LOW) { 
      int canalAlocado = -1;
      for (int v = 0; v < MAX_VOICES; v++) {
        if (!meusCanais[v].inUse) {
          canalAlocado = v;
          break;
        }
      }
      if (canalAlocado != -1) {
        meusCanais[canalAlocado].key = extraKeyChar;
        meusCanais[canalAlocado].inUse = true;
        synth.noteOn(canalAlocado, notaExtra, 120);
        Serial.println("Nota LIGADA (Extra C4) no Canal: " + String(canalAlocado));
      }
    } 
    else { 
      for (int v = 0; v < MAX_VOICES; v++) {
        if (meusCanais[v].inUse && meusCanais[v].key == extraKeyChar) {
          synth.noteOff(v);
          meusCanais[v].inUse = false;
          meusCanais[v].key = '\0';
          Serial.println("Nota desligada (Extra C4) do Canal: " + String(v));
        }
      }
    }
    estadoAnteriorC = estadoAtualC;
  }

  // --- 3. NOVO: LEITURA DO POTENCIÔMETRO DE VOLUME (não bloqueante) ---
  atualizarVolume();

  // --- 4. NOVO: LEITURA DO BOTÃO DE TROCA DE TIMBRE (não bloqueante) ---
  verificarBotaoTimbre();

  delay(2); 
}
