# Comedouro — Alimentador de Pets com Porta RFID

Sistema de comedouro automatizado para gatos, com porta controlada por servo e leitora RFID 125 kHz serial. Apenas a tag da **Snow** abre a porta; qualquer outra tag cadastrada ou desconhecida fecha a porta.

---

## Funcionalidades e Comportamento

| Tag detectada | Ação |
|---|---|
| **Snow** (`900200000014659`) | Abre a porta (servo → 90°) |
| **Lilith** (`900200000014611`) | Fecha a porta (servo → 180°) |
| **Sophie** (`900200000014691`) | Fecha a porta (servo → 180°) |
| **Tag desconhecida** | Fecha a porta (servo → 180°) |

### Regras de operação

- A porta inicia **fechada** (180°) ao ligar o Arduino.
- O movimento do servo é **suave e gradual** (2° a cada 20 ms), sem travar o loop principal.
- Leituras RFID repetidas da **mesma tag** são ignoradas por **3 segundos** (debounce), evitando abertura/fechamento em loop.
- Se a porta **já está na posição desejada** (aberta ou fechada), uma nova leitura da mesma tag **não aciona o servo** — evita micro-movimentos ao reaproximar a tag.
- O **LED integrado** (pino 13) acende brevemente a cada tag processada com sucesso.
- O firmware **não usa `delay()`** — toda a lógica é baseada em polling e máquina de estados não-bloqueante.

---

## Requisitos de Hardware

| Componente | Função |
|---|---|
| **Arduino Uno** | Microcontrolador principal |
| **Servo MG90S** | Acionamento da porta do comedouro |
| **Leitora RFID 125 kHz serial** | Identificação das tags dos pets |
| **Fonte P4** | Alimentação do Arduino |
| **Protoboard 170 pontos** | Distribuição de 5V e GND para periféricos |

### Bibliotecas Arduino (IDE)

- `SoftwareSerial` (incluída no core do Arduino)
- `Servo` (incluída no core do Arduino)

---

## Diagrama de Ligação

### Alimentação

```
Fonte P4
  ├── (+) VIN  →  Arduino VIN (ou jack DC)
  └── (−) GND  →  Arduino GND
```

> A Fonte P4 alimenta o regulador interno do Arduino Uno, que fornece **5V** estável pelo pino `5V` e **GND** comum pelo pino `GND`.

### Distribuição via protoboard

```
Arduino 5V  ──→  Trilho (+) da protoboard 170 pontos
Arduino GND ──→  Trilho (−) da protoboard 170 pontos

Trilho (+) ──→  VCC da leitora RFID
Trilho (−) ──→  GND da leitora RFID

Trilho (+) ──→  VCC (fio vermelho) do Servo MG90S
Trilho (−) ──→  GND (fio marrom/preto) do Servo MG90S
```

> O servo e a leitora RFID são alimentados pelos trilhos da protoboard, que por sua vez recebem 5V e GND diretamente do Arduino. **Não** conecte a Fonte P4 diretamente aos periféricos — use sempre o regulador do Arduino.

### Conexões de sinal (Arduino)

| Pino Arduino | Destino | Observação |
|---|---|---|
| **D2** | TX da leitora RFID | Recepção via `SoftwareSerial` (`RFID_RX_PIN`) |
| **D3** | — | Reservado pelo `SoftwareSerial` (`RFID_TX_PIN`); não utilizado, mas obrigatório |
| **D10** | Sinal (fio laranja/amarelo) do Servo MG90S | Controle PWM do servo |
| **D13** | LED integrado | Indicador de leitura RFID processada |

### Diagrama ASCII completo

```
                    ┌─────────────────┐
                    │    Fonte P4     │
                    │   (+)      (−)  │
                    └────┬──────┬─────┘
                         │      │
                       VIN    GND
                         │      │
              ┌──────────┴──────┴──────────┐
              │       Arduino Uno        │
              │                          │
              │  D2 ◄──── TX  RFID       │
              │  D3  (reservado)         │
              │  D10 ────► SIG  Servo    │
              │  D13  LED integrado      │
              │                          │
              │  5V ──────────────┐      │
              │  GND ─────────────┼──┐   │
              └───────────────────┼──┼───┘
                                  │  │
              ┌───────────────────┴──┴───────────────────┐
              │         Protoboard 170 pontos              │
              │  Trilho (+)              Trilho (−)        │
              └──┬──────────────────────────┬──────────────┘
                 │                          │
           ┌─────┴─────┐              ┌─────┴─────┐
           │  RFID     │              │  RFID     │
           │  VCC  GND │              │  Servo    │
           │  TX       │              │  VCC  GND │
           └───────────┘              │  SIG      │
                                      └───────────┘
```

---

## Arquitetura de Software

O firmware é organizado em funções modulares com responsabilidades bem definidas. O `loop()` principal executa apenas duas tarefas em cada iteração:

```cpp
void loop() {
    updateServoPosition();  // Movimento gradual do servo
    pollRfid();             // Leitura não-bloqueante do RFID
}
```

### Módulos principais

| Módulo | Funções | Responsabilidade |
|---|---|---|
| **RFID** | `pollRfid`, `pollPacketReader`, `extractTag`, `parsePacket`, `buildTag` | Captura e decodificação de pacotes seriais |
| **Porta** | `requestDoorOpen`, `requestDoorClose` | Define o ângulo alvo do servo conforme a tag |
| **Servo** | `updateServoPosition`, `moveServoOneStep` | Movimento suave não-bloqueante |
| **Debounce** | `isDebounced`, `rememberProcessedTag` | Evita leituras duplicadas em curto intervalo |
| **Conversão** | `u16ToString`, `u64ToString`, `nationalToString` | Monta string da tag a partir do pacote binário |

### Máquina de estados RFID (não-bloqueante)

A leitura do pacote serial usa uma máquina de estados com dois estados:

```
Idle ──(byte 0xAA)──► Reading ──(byte 0xBB)──► pacote completo → Idle
  ▲                        │
  └──── timeout 200 ms ────┘
```

1. **Idle** — aguarda o byte de início `0xAA` no buffer serial.
2. **Reading** — acumula bytes até encontrar `0xBB` (fim do pacote) ou estourar o timeout de 200 ms.
3. Pacote completo é parseado: bytes 4–5 = código do país, bytes 6–10 = número nacional (12 dígitos com zero-padding).

Essa abordagem garante que o servo continue se movendo suavemente enquanto o RFID é lido, sem nenhuma chamada a `delay()`.

### Protocolo do pacote RFID

| Campo | Posição no pacote | Descrição |
|---|---|---|
| Byte de início | `[0]` | `0xAA` |
| Byte de fim | último byte | `0xBB` |
| Código do país | `[4]` e `[5]` | 16 bits, big-endian |
| Número nacional | `[6]` a `[10]` | 40 bits, big-endian |
| Tag final | — | `código_país` + `número_nacional` (12 dígitos) |

Exemplo: país `9002` + nacional `000014659` → tag `900200000014659` (Snow).

---

## Tabela de Constantes de Configuração

| Constante | Valor | Descrição |
|---|---|---|
| `RFID_RX_PIN` | `2` | Pino de recepção serial do RFID |
| `RFID_TX_PIN` | `3` | Pino TX reservado pelo `SoftwareSerial` |
| `SERVO_PIN` | `10` | Pino PWM do servo |
| `STATUS_LED_PIN` | `LED_BUILTIN` | LED integrado (pino 13 no Uno) |
| `SERVO_OPEN_ANGLE` | `90` | Ângulo com porta aberta |
| `SERVO_CLOSED_ANGLE` | `180` | Ângulo com porta fechada |
| `SERVO_STEP` | `2` | Graus movidos por iteração |
| `SERVO_STEP_INTERVAL_MS` | `20` | Intervalo entre passos do servo (ms) |
| `DEBOUNCE_MS` | `3000` | Tempo de debounce por tag (ms) |
| `START_BYTE` | `0xAA` | Byte de início do pacote RFID |
| `END_BYTE` | `0xBB` | Byte de fim do pacote RFID |
| `MAX_PACKET_SIZE` | `64` | Tamanho máximo do buffer de pacote |
| `BYTE_TIMEOUT_MS` | `200` | Timeout entre bytes durante leitura (ms) |
| `NATIONAL_DEC_WIDTH` | `12` | Largura do número nacional (dígitos) |
| Baud rate RFID | `9600` | Velocidade da porta serial (`rfidSerial.begin`) |

### Tempo de movimento do servo

Com as configurações padrão, a porta leva aproximadamente **900 ms** para percorrer os 90° entre fechado e aberto:

```
90° ÷ 2°/passo = 45 passos × 20 ms = 900 ms
```

---

## Tags Cadastradas

| Pet | Constante no código | ID da tag |
|---|---|---|
| Lilith | `TAG_LILITH` | `900200000014611` |
| Snow | `TAG_SNOW` | `900200000014659` |
| Sophie | `TAG_SOPHIE` | `900200000014691` |

Para adicionar ou alterar uma tag, edite as constantes no início de `comedouro.ino` e faça novo upload para o Arduino.

---

## Instruções de Upload

1. Instale o [Arduino IDE](https://www.arduino.cc/en/software) (versão 1.8.x ou 2.x).
2. Conecte o Arduino Uno ao computador via cabo USB.
3. Abra o arquivo `comedouro.ino` no Arduino IDE.
4. Em **Ferramentas → Placa**, selecione **Arduino Uno**.
5. Em **Ferramentas → Porta**, selecione a porta COM correspondente ao Arduino.
6. Clique em **Verificar** (✓) para compilar e confirmar que não há erros.
7. Clique em **Carregar** (→) para enviar o firmware ao Arduino.
8. Após o upload, a porta deve iniciar **fechada** (servo em 180°). Aproxime a tag da Snow para testar a abertura.

> As bibliotecas `SoftwareSerial` e `Servo` já fazem parte do core do Arduino Uno — não é necessário instalar bibliotecas adicionais.

---

## Solução de Problemas

### Servo lento ou travado durante leitura RFID

**Sintoma:** o servo se move de forma irregular ou para completamente enquanto uma tag é lida.

**Causa:** versões anteriores do firmware usavam `delay()` ou leitura serial bloqueante, impedindo que `updateServoPosition()` fosse chamada com frequência suficiente.

**Solução implementada:** a leitura RFID foi reescrita como máquina de estados não-bloqueante (`pollPacketReader`). Cada iteração do `loop()` processa no máximo alguns bytes do serial e retorna imediatamente, permitindo que o servo avance 2° a cada 20 ms sem interrupção.

### Porta não abre com a tag da Snow

- Verifique se a tag exibe o ID `900200000014659` (pode ser confirmado com um leitor RFID no computador).
- Confirme que o fio **TX da leitora** está conectado ao **D2** do Arduino.
- Verifique a alimentação: trilho (+) da protoboard deve estar em 5V.
- Aguarde 3 segundos após a última leitura da mesma tag (debounce ativo).

### Porta não fecha

- Qualquer tag diferente de Snow deve fechar a porta — teste com Lilith ou Sophie.
- Verifique se o servo está recebendo alimentação adequada pelo trilho da protoboard.
- Confirme que o fio de sinal do servo está no **D10**.

### Leitora RFID não responde

- Baud rate deve ser **9600** (padrão do módulo e do firmware).
- Verifique GND comum entre Arduino, leitora e servo.
- Confirme que o pino **D3** não está sendo usado por outro componente (reservado pelo `SoftwareSerial`).
- Teste com LED integrado: se ele pisca ao aproximar uma tag, a leitora e o parsing estão funcionando.

### Servo não se move ou vibra

- O MG90S requer **5V** estáveis — alimentar pelo trilho da protoboard a partir do pino `5V` do Arduino é suficiente para um único servo.
- Verifique o fio de sinal (geralmente laranja ou amarelo) no **D10**.
- Ângulos configurados: aberto = 90°, fechado = 180°. Ajuste `SERVO_OPEN_ANGLE` e `SERVO_CLOSED_ANGLE` se a mecânica da porta exigir outros valores.

### Micro-movimento ao reaproximar a tag Snow com a porta já aberta

**Sintoma:** a porta já está aberta, mas ao afastar e aproximar a tag Snow novamente o servo se move levemente, como se tentasse abrir de novo.

**Causa:** após o debounce de 3 segundos, a tag Snow era reprocessada e o firmware não verificava se o servo já estava na posição de abertura (`currentServoAngle` e `targetServoAngle` em 90°).

**Solução implementada:** funções `isDoorAtOpenPosition()` e `isDoorAtClosedPosition()` garantem que comandos redundantes sejam ignorados. `handleDetectedTag()` só chama `requestDoorOpen()` ou `requestDoorClose()` quando há mudança de estado real.

### Tag lida repetidamente sem efeito

- O debounce de **3 segundos** bloqueia ações repetidas da mesma tag. Aguarde o intervalo ou aproxime uma tag diferente para forçar uma nova ação.

### Pacote RFID corrompido ou ignorado

- O timeout de **200 ms** entre bytes descarta pacotes incompletos automaticamente.
- Verifique interferência eletromagnética próxima ao módulo RFID.
- Certifique-se de que os fios de sinal não estão muito longos ou em paralelo com fios de alimentação de motores.

---

## Estrutura do Projeto

```
comedouro/
├── comedouro.ino   # Firmware principal
└── README.md       # Este arquivo
```

---

## Licença

Projeto de uso pessoal/doméstico. Livre para adaptar conforme necessário.
