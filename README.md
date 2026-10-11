# Comedouro — Alimentador de Pets com Porta RFID

Sistema de comedouro automatizado para gatos, com porta controlada por servo e leitora RFID serial FDX-B de 134,2 kHz. Apenas a tag da **Snow** abre a porta; qualquer outra tag cadastrada ou desconhecida fecha a porta.

---

## Funcionalidades e Comportamento

| Tag detectada | Ação |
|---|---|
| **Snow** (`900200000014659`) | Abre a porta (servo → 105°) |
| **Lilith** (`900200000014611`) | Fecha a porta (servo → 0°) |
| **Sophie** (`900200000014691`) | Fecha a porta (servo → 0°) |
| **Tag desconhecida** | Fecha a porta (servo → 0°) |

### Regras de operação

- A porta inicia **fechada** (0°) ao ligar o Arduino.
- O movimento do servo é **suave e gradual** (2° a cada 20 ms), sem travar o loop principal.
- Leituras RFID repetidas da **mesma tag** são ignoradas por **3 segundos** (debounce), evitando abertura/fechamento em loop.
- Se a porta **já está na posição desejada** (aberta ou fechada), uma nova leitura da mesma tag **não aciona o servo** — evita micro-movimentos ao reaproximar a tag.
- O **LED integrado** (pino 13) acende brevemente a cada tag processada com sucesso.
- O firmware **não usa `delay()`** — toda a lógica é baseada em polling e máquina de estados não-bloqueante.

---

## Requisitos de Hardware

| Componente | Função |
|---|---|
| **Arduino Nano V3 (ATmega328P, 5 V)** | Microcontrolador principal |
| **Servo MG90S** | Acionamento da porta do comedouro |
| **Leitora RFID FDX-B 134,2 kHz serial** | Identificação das tags dos pets |
| **Módulo de alimentação** | Entrada P4 de 12 V e saídas reguladas de 5 V, 3,3 V e GND |
| **Fonte 12 V / 3 A** | Alimentação da entrada P4 do módulo |

### Bibliotecas Arduino (IDE)

- `SoftwareSerial` (incluída no core do Arduino)
- `Servo` (incluída no core do Arduino)

---

## Diagrama de Ligação

### Alimentação

```
Fonte 12 V / 3 A
  └── P4 → entrada do módulo de alimentação

Módulo (saída regulada de 5 V) ──→ Nano 5V e alimentação do servo
Módulo (saída de 3,3 V ou 5 V) ──→ alimentação do leitor, conforme o modelo
Módulo GND ──┬──→ Nano GND
             ├──→ GND do leitor
             └──→ GND do servo
```

> Use o trilho regulado de 5 V do módulo para alimentar o pino `5V` do Nano e o servo. Isso contorna o regulador da placa, portanto confirme que a saída é realmente regulada para 5 V. Não conecte 12 V ao pino `5V`. Confira a corrente máxima contínua e de pico da saída de 5 V do módulo: a fonte de entrada de 12 V / 3 A, por si só, não garante que essa saída suporte o servo.
>
> Ligue o positivo e o GND do servo diretamente aos trilhos de alimentação do módulo, não através do Nano nem de trilhas finas da protoboard. O sinal do servo continua ligado ao D10. Todos os GND precisam estar em comum.
>
> Ao conectar o Nano ao computador por USB para programar, desconecte a alimentação externa de 5 V do pino `5V`, a menos que a placa tenha isolamento entre USB e alimentação externa. Isso evita alimentar o USB de volta pelo Nano.

### Distribuição via protoboard

```
Módulo 5V  ──→  Nano 5V e trilho (+) para o servo
Módulo 3,3V ou 5V ──→ VCC do leitor, conforme especificação
Módulo GND ──→ Nano GND, GND do leitor e trilho (−) do servo

Trilho (+) 5V ──→  VCC (fio vermelho) do Servo MG90S
Trilho (−) GND ──→  GND (fio marrom/preto) do Servo MG90S

Saída do módulo exigida pelo leitor ──→ VCC da leitora RFID
Módulo GND ──→ GND da leitora RFID
```

> Não alimente o servo pelo pino `5V` do Nano. Se usar protoboard para o servo, confirme que os contatos e fios suportam a corrente de pico; prefira distribuir a alimentação do servo diretamente a partir do módulo. Nunca aplique 12 V às entradas de 5 V ou 3,3 V.

### Conexões de sinal (Arduino)

| Pino Arduino | Destino | Observação |
|---|---|---|
| **D2** | TX da leitora RFID | Recepção via `SoftwareSerial` (`RFID_RX_PIN`) |
| **D3** | — | Reservado pelo `SoftwareSerial` (`RFID_TX_PIN`); não utilizado, mas obrigatório |
| **D10** | Sinal (fio laranja/amarelo) do Servo MG90S | Controle PWM do servo |
| **D13** | LED integrado (`LED_BUILTIN`) | Indicador de leitura RFID processada; D13 no Nano V3 |

### Diagrama ASCII completo

```
Fonte 12 V / 3 A ──P4──► Módulo de alimentação
                             ├── 5 V ──► Nano 5V
                             ├── 5 V ──► Servo VCC (vermelho)
                             ├── 3,3 V ou 5 V ──► RFID VCC*
                             └── GND ──┬──► Nano GND
                                       ├──► Servo GND (marrom/preto)
                                       └──► RFID GND

Nano D2  ◄──── RFID TX
Nano D10 ─────► Servo SIG (laranja/amarelo)
Nano D13 ─────► LED integrado
```

`VCC*` do leitor deve receber a tensão indicada para o modelo específico (3,3 V ou 5 V). O TX do leitor deve ser uma saída serial TTL compatível com a entrada de 5 V do Nano. Não use RS-232 diretamente nos pinos do Arduino.

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
3. Pacote completo é parseado: bytes 4–5 = código do país, bytes 6–10 = número nacional (11 dígitos com zero-padding para os IDs cadastrados).

Essa abordagem garante que o servo continue se movendo suavemente enquanto o RFID é lido, sem nenhuma chamada a `delay()`.

### Protocolo do pacote RFID

| Campo | Posição no pacote | Descrição |
|---|---|---|
| Byte de início | `[0]` | `0xAA` |
| Byte de fim | último byte | `0xBB` |
| Código do país | `[4]` e `[5]` | 16 bits, big-endian |
| Número nacional | `[6]` a `[10]` | 40 bits, big-endian |
| Tag final | — | `código_país` + `número_nacional` (11 dígitos nos IDs cadastrados) |

Exemplo: país `9002` + nacional `000014659` → tag `900200000014659` (Snow).

---

## Tabela de Constantes de Configuração

| Constante | Valor | Descrição |
|---|---|---|
| `RFID_RX_PIN` | `2` | Pino de recepção serial do RFID |
| `RFID_TX_PIN` | `3` | Pino TX reservado pelo `SoftwareSerial` |
| `SERVO_PIN` | `10` | Pino PWM do servo |
| `STATUS_LED_PIN` | `LED_BUILTIN` | LED integrado (pino 13 no Nano V3) |
| `SERVO_OPEN_ANGLE` | `105` | Ângulo com porta aberta |
| `SERVO_CLOSED_ANGLE` | `0` | Ângulo com porta fechada |
| `SERVO_STEP` | `2` | Graus movidos por iteração |
| `SERVO_STEP_INTERVAL_MS` | `20` | Intervalo entre passos do servo (ms) |
| `DEBOUNCE_MS` | `3000` | Tempo de debounce por tag (ms) |
| `START_BYTE` | `0xAA` | Byte de início do pacote RFID |
| `END_BYTE` | `0xBB` | Byte de fim do pacote RFID |
| `MAX_PACKET_SIZE` | `64` | Tamanho máximo do buffer de pacote |
| `BYTE_TIMEOUT_MS` | `200` | Timeout entre bytes durante leitura (ms) |
| `NATIONAL_DEC_WIDTH` | `11` | Largura do número nacional (dígitos) |
| Baud rate RFID | `9600` | Velocidade da porta serial (`rfidSerial.begin`) |

### Tempo de movimento do servo

Com as configurações padrão, a porta leva aproximadamente **1,06 s** para percorrer os 105° entre fechado e aberto:

```
105° ÷ 2°/passo ≈ 53 passos × 20 ms ≈ 1.060 ms
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
2. Conecte o Arduino Nano V3 ao computador via cabo USB. Para evitar retorno de corrente, remova antes o fio externo de 5 V ligado ao pino `5V` do Nano.
3. Abra o arquivo `comedouro.ino` no Arduino IDE.
4. Em **Ferramentas → Placa**, selecione **Arduino AVR Boards → Arduino Nano**.
5. Em **Ferramentas → Processador**, selecione **ATmega328P**. Em clones antigos que falharem no upload, tente **ATmega328P (Old Bootloader)**.
6. Em **Ferramentas → Porta**, selecione a porta correspondente ao Nano.
7. Clique em **Verificar** (✓) para compilar e confirmar que não há erros.
8. Com a alimentação externa de 5 V do Nano ainda desconectada, clique em **Carregar** (→) para enviar o firmware.
9. Desconecte o USB, reconecte a alimentação externa de 5 V e ligue o módulo. A porta deve iniciar **fechada** (servo em 0°). Aproxime a tag da Snow para testar a abertura.

> As bibliotecas `SoftwareSerial` e `Servo` fazem parte do pacote **Arduino AVR Boards** usado pelo Nano; não é necessário instalar bibliotecas adicionais.

---

## Solução de Problemas

### Servo lento ou travado durante leitura RFID

**Sintoma:** o servo se move de forma irregular ou para completamente enquanto uma tag é lida.

**Causa:** versões anteriores do firmware usavam `delay()` ou leitura serial bloqueante, impedindo que `updateServoPosition()` fosse chamada com frequência suficiente.

**Solução implementada:** a leitura RFID foi reescrita como máquina de estados não-bloqueante (`pollPacketReader`). Cada iteração do `loop()` processa no máximo alguns bytes do serial e retorna imediatamente, permitindo que o servo avance 2° a cada 20 ms sem interrupção.

### Porta não abre com a tag da Snow

- Verifique se a tag exibe o ID `900200000014659` (pode ser confirmado com um leitor RFID no computador).
- Confirme que o fio **TX da leitora** está conectado ao **D2** do Arduino.
- Confirme a tensão de alimentação exigida pelo leitor e a tensão/corrente de saída do módulo.
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

- O MG90S requer alimentação regulada de **5V**. Alimente-o diretamente pelo módulo, dimensionado para a corrente de pico do servo; não use o pino `5V` do Nano como caminho de alimentação.
- Verifique o fio de sinal (geralmente laranja ou amarelo) no **D10**.
- Ângulos configurados: aberto = 105°, fechado = 0°. Ajuste `SERVO_OPEN_ANGLE` e `SERVO_CLOSED_ANGLE` se a mecânica da porta exigir outros valores.

### Micro-movimento ao reaproximar a tag Snow com a porta já aberta

**Sintoma:** a porta já está aberta, mas ao afastar e aproximar a tag Snow novamente o servo se move levemente, como se tentasse abrir de novo.

**Causa:** após o debounce de 3 segundos, a tag Snow era reprocessada e o firmware não verificava se o servo já estava na posição de abertura (`currentServoAngle` e `targetServoAngle` em 105°).

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
