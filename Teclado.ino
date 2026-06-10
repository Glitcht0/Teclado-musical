#include <Arduino.h>
#include <Keypad.h>

// ==========================================
// CONFIGURAÇÃO DA MATRIZ 4x4
// ==========================================
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

// ==========================================
// CONFIGURAÇÃO DA TECLA INDEPENDENTE (C Extra)
// ==========================================
#define PINO_C_EXTRA 5

// Variável para guardar o estado anterior (HIGH = Solto, LOW = Apertado)
bool estadoAnteriorC = HIGH; 

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Configura o pino da tecla extra usando o resistor interno do ESP32
  pinMode(PINO_C_EXTRA, INPUT_PULLUP);
  
  Serial.println("Sintetizador: Matriz e Tecla Extra Iniciados!");
}

void loop() {
  
  // --- 1. LEITURA DA MATRIZ ---
  if (teclado.getKeys()) {
    for (int i = 0; i < LIST_MAX; i++) {
      if (teclado.key[i].stateChanged) {
        
        if (teclado.key[i].kstate == PRESSED) {
          Serial.print("Nota LIGADA (Matriz): ");
          Serial.println(teclado.key[i].kchar);
        }
        
        if (teclado.key[i].kstate == RELEASED) {
          Serial.print("Nota desligada (Matriz): ");
          Serial.println(teclado.key[i].kchar);
        }
      }
    }
  }

  // --- 2. LEITURA DA TECLA INDEPENDENTE ---
  // Lê como a tecla extra está agora neste exato milissegundo
  bool estadoAtualC = digitalRead(PINO_C_EXTRA);

  // Se o estado mudou em relação à última vez que olhamos (ou seja, você apertou ou soltou)
  if (estadoAtualC != estadoAnteriorC) {
    
    // Um pequeno delay de 5ms (Debounce) para ignorar a faísca física do switch
    delay(5); 

    if (estadoAtualC == LOW) { 
      // Se caiu para LOW, significa que o botão foi apertado (conectou ao GND)
      Serial.println("Nota LIGADA (Extra): C");
    } else { 
      // Se subiu para HIGH, significa que o botão foi solto
      Serial.println("Nota desligada (Extra): C");
    }
    
    // Atualiza a memória com o estado novo para o próximo giro do loop
    estadoAnteriorC = estadoAtualC;
  }

  delay(10); // Alivia o processador
}