# AXION — Plano de Implementação (POC Localização BLE)

Identificar em qual zona (A, B, C) cada operador está, usando beacons BLE DX-CP29 + 3x M5Stack ATOM Matrix (firmware **ESP-IDF**), reportando a uma API que decide a zona e alimenta um dashboard web.

Regra do plano: cada fase tem tarefas e testes com critério objetivo. Fase só avança com todos os testes em PASS (status: `PENDENTE | EM ANDAMENTO | PASS | FAIL`). Fases 0 a 8 em PASS = desafio completo. As seções 1 a 3 são a especificação fixa; as fases apenas a implementam — qualquer mudança de valor deve ser feita primeiro aqui.

---

## 1. Hardware

### 1.1 M5Stack ATOM Matrix v1.1 — pinout completo

SoC ESP32-PICO-D4 (dual-core LX6 240 MHz, 4 MB flash, 520 KB SRAM, Wi-Fi 2.4 GHz + Bluetooth 4.2 BLE em radio unico compartilhado). USB-C com conversor serial FTDI (driver FTDI VCP no Windows).

| GPIO | Funcao | Observacoes |
|---|---|---|
| G27 | Data da matriz 5x5 WS2812C-2020 (25 LEDs) | 800 kHz, ordem de cor GRB |
| G39 | Botao frontal | Input-only, sem pull-up interno (pull-up externo na placa), ativo em LOW, debounce 30 ms por software |
| G12 | Transmissor IR | Nao usado na POC |
| G21 / G25 | I2C SCL / SDA — IMU BMI270 (addr 0x68) | Nao usado na POC; nao reutilizar os pinos |
| G26 / G32 | Porta Grove (HY2.0-4P) | Livre; reservado para expansao |
| — | Alimentacao | 5 V / 500 mA via USB-C; consumo base ~62 mA |

Restricoes elétricas:
- Brilho global dos LEDs limitado a **40/255 (~16%)**. A doc da M5Stack alerta que brilho alto danifica LEDs e o acrilico. Orcamento de corrente: 25 LEDs brancos a 100% ~ 375 mA + ESP32 em pico de radio ~ 240 mA estoura os 500 mA do USB; com glifos de ate 16 LEDs em cor unica a 16% o total fica < 100 mA.
- G39 e input-only (nao aceita pull-up interno nem output).

### 1.2 Beacon DX-CP29 (cracha do operador)

| Item | Valor |
|---|---|
| Radio | BLE 5.1 + NFC; advertising legado (iBeacon/Eddystone) — compativel com scan BLE 4.2 do ESP32 |
| Configuracao adotada | Modo **iBeacon**, intervalo de advertising **200 ms**, TX power **0 dBm** (via app DX-SMART iOS/Android) |
| Identidade | UUID unico do projeto: `A0C10A00-0000-4000-8000-000000000001` para todos; Major = id do operador (1..N); Minor = 1. Identificacao primaria pelo **MAC** (validar MAC estatico no teste 1.5) |
| Bateria | Li-ion recarregavel USB-C, ate 1 ano (menor com intervalo de 200 ms — aceitavel na POC) |
| Alcance | 60–80 m |

### 1.3 Restricao central de radio

Wi-Fi e BLE dividem o mesmo radio 2.4 GHz. Mitigacao: coexistencia por software do IDF (`CONFIG_ESP_COEX_SW_COEXIST_ENABLE=y`), scan passivo com janela 90/100 ms, POSTs curtos com keep-alive, modem sleep habilitado (`WIFI_PS_MIN_MODEM`). Impacto medido na Fase 3 (teste 3.2).

---

## 2. Decisoes de arquitetura

```
[DX-CP29] --adv BLE--> [ATOM A/B/C: scan passivo + filtro RSSI] --HTTP/JSON--> [API + SQLite]
                                                                       |--> [Dashboard web (WebSocket)]
```

1. **Decisao de zona no servidor** (nao no ATOM): cada ATOM reporta `{mac, rssi_filtrado}`; a API compara os RSSIs dos 3 ATOMs por beacon (argmax). Um ATOM sozinho nao sabe se outro enxerga o beacon mais forte; centralizar elimina dupla contagem.
2. **Filtragem em duas camadas**: mediana/EMA no firmware (reduz ruido e payload) + histerese temporal no servidor (elimina flapping — RSSI parado flutua +-10 dB).
3. **Timeout de presenca**: beacon sem leitura sai da contagem (constantes na §3.1).
4. **Sem NTP no firmware**: o servidor carimba o tempo na recepcao; o ATOM envia `seq` e `uptime_ms`.
5. **Contagem no display sem GET extra**: a resposta do `POST /api/readings` ja devolve a contagem da zona.
6. Alternativas pesquisadas (documentar em `docs/decisoes.md`): trilateracao (RSSI indoor impreciso), fingerprinting (preciso, exige mapeamento — proximo passo, nao POC), decisao no edge (dupla contagem), Kalman 1D vs EMA vs mediana (comparados na Fase 7).

Stack: firmware **ESP-IDF v5.5.x** (instalado em `C:\esp\v5.5.3`; NimBLE nativo, `esp_http_client`, cJSON, componente `espressif/led_strip`); backend **Python 3.12 + FastAPI + uvicorn + SQLite (WAL)**; dashboard **HTML/JS vanilla** servido pela propria API, tempo real via WebSocket.

---

## 3. Especificacao fixa

### 3.1 Constantes do sistema

Fonte unica: `firmware/main/axion_config.h` e `server/app/config.py`. Valores default abaixo; a Fase 7 pode ajusta-los (registrando a mudanca).

| Constante | Valor | Onde | Descricao |
|---|---|---|---|
| `BLE_SCAN_INTERVAL` | 160 (100 ms) | fw | unidades de 0.625 ms |
| `BLE_SCAN_WINDOW` | 144 (90 ms) | fw | duty ~90%, scan passivo, duplicatas NAO filtradas |
| `RSSI_WINDOW_MS` | 3000 | fw | janela deslizante por beacon |
| `RSSI_EMA_ALPHA` | 0.30 | fw | filtro alternativo (selecionavel p/ Fase 7) |
| `RSSI_OUTLIER_SIGMA` | 2.0 | fw | descarte de outlier na janela |
| `SNAPSHOT_PERIOD_MS` | 2000 | fw | periodo do POST /api/readings |
| `HTTP_TIMEOUT_MS` | 1500 | fw | timeout do esp_http_client |
| `OFFLINE_AFTER_FAILS` | 3 | fw | 3 POSTs falhos consecutivos -> estado OFFLINE; 1 sucesso recupera |
| `WIFI_BACKOFF_MS` | 1000 -> 30000 | fw | reconexao exponencial (x2) |
| `WHITELIST_REFRESH_S` | 60 | fw | alem de refresh imediato quando `whitelist_ver` muda no POST |
| `DISPLAY_LETTER_MS` / `DISPLAY_COUNT_MS` | 2000 / 2000 | fw | ciclo letra/contagem |
| `LED_BRIGHTNESS_MAX` | 40 | fw | escala 0–255 |
| `BTN_DEBOUNCE_MS` | 30 | fw | botao G39 |
| `TASK_WDT_S` | 10 | fw | watchdog de tasks |
| `HYSTERESIS_DB` | 5.0 | srv | nova zona precisa vencer a atual por >= 5 dB |
| `HYSTERESIS_HOLD_S` | 3 | srv | ... durante >= 3 s continuos |
| `PRESENCE_TIMEOUT_S` | 15 | srv | beacon some das leituras -> sai da contagem |
| `ATOM_OFFLINE_S` | 10 | srv | ATOM sem POST -> OFFLINE no dashboard |
| Beacon adv interval / TX | 200 ms / 0 dBm | beacon | configurado no app DX-SMART |

### 3.2 Display (matriz 5x5)

Mapeamento: com o USB-C para baixo, LED 0 = canto superior esquerdo, ordem linha a linha: `index = linha*5 + coluna`. Validar com teste de varredura na Fase 0 (teste 0.2); se a orientacao real divergir, corrigir por tabela de remapeamento em `display.c`, nunca nos glifos.

Cores (RGB, antes do brilho global 40/255):

| Estado | Cor | Padrao |
|---|---|---|
| Zona A | ciano (0,200,255) | glifo |
| Zona B | verde (0,220,80) | glifo |
| Zona C | laranja (255,120,0) | glifo |
| OFFLINE (sem Wi-Fi ou API) | vermelho (255,0,0) | todos os 25 LEDs |
| Boot | branco (255,255,255) | pixel central (index 12) |
| Conectando Wi-Fi | amarelo (255,200,0) | pixel central piscando 500 ms |

Maquina de estados do display: `BOOT -> WIFI_CONNECTING -> RUN | OFFLINE`. Em `RUN`: letra da zona (2 s) -> contagem (2 s) -> repete. Contagem > 9 exibe `9` (limite de 1 digito; contagem esperada na POC <= 4). Transicao para `OFFLINE` conforme `OFFLINE_AFTER_FAILS`; retorno a `RUN` no primeiro POST com sucesso.

Fonte 5x5 (`font5x5.h`, 1 = LED aceso, linha de cima primeiro):

```
A: 01110 10001 11111 10001 10001      0: 01110 10001 10001 10001 01110
B: 11110 10001 11110 10001 11110      1: 00100 01100 00100 00100 01110
C: 01111 10000 10000 10000 01111      2: 11110 00001 01110 10000 11111
                                      3: 11110 00001 00110 00001 11110
                                      4: 10010 10010 11111 00010 00010
                                      5: 11111 10000 11110 00001 11110
                                      6: 01110 10000 11110 10001 01110
                                      7: 11111 00001 00010 00100 00100
                                      8: 01110 10001 01110 10001 01110
                                      9: 01110 10001 01111 00001 01110
```

### 3.3 Contratos de API

`POST /api/readings` — request (firmware -> servidor, a cada `SNAPSHOT_PERIOD_MS`):

```json
{
  "atom_id": "atom-a",
  "zone": "A",
  "seq": 1234,
  "uptime_ms": 123456,
  "beacons": [
    { "mac": "C3:FA:7B:12:5E", "rssi": -67.2, "n": 9 }
  ]
}
```

`rssi` = valor filtrado da janela; `n` = quantidade de advertisements na janela. Beacons sem leitura na janela nao entram no array. Response `200`:

```json
{ "zone_count": 2, "whitelist_ver": 7 }
```

Se `whitelist_ver` for maior que a versao local, o ATOM chama `GET /api/beacons/active` na sequencia. Erros: `400` payload invalido, `404` atom desconhecido. Qualquer nao-200 conta como falha para `OFFLINE_AFTER_FAILS`.

| Endpoint | Metodo | Uso |
|---|---|---|
| `/api/readings` | POST | Ingestao (acima) |
| `/api/beacons/active` | GET | Whitelist p/ firmware: `{ "ver": 7, "macs": ["C3:FA:7B:12:5E", ...] }` |
| `/api/beacons` | GET / POST | Cadastro: `{operator, badge, mac, active}`; POST incrementa `whitelist_ver` |
| `/api/beacons/{id}` | PUT / DELETE | Editar / desativar; incrementa `whitelist_ver` |
| `/api/state` | GET | Estado completo: por zona (contagem + operadores), por ATOM (online/offline, last_seen) |
| `/ws` | WebSocket | Servidor envia o payload de `/api/state` a cada mudanca de estado |

### 3.4 Banco (SQLite, WAL)

```sql
CREATE TABLE beacons  (id INTEGER PRIMARY KEY, operator TEXT NOT NULL, badge TEXT UNIQUE NOT NULL,
                       mac TEXT UNIQUE NOT NULL, active INTEGER NOT NULL DEFAULT 1);
CREATE TABLE atoms    (id TEXT PRIMARY KEY, zone TEXT UNIQUE NOT NULL, last_seen REAL);
CREATE TABLE readings (id INTEGER PRIMARY KEY, server_ts REAL NOT NULL, atom_id TEXT NOT NULL,
                       mac TEXT NOT NULL, rssi REAL NOT NULL, n INTEGER NOT NULL);
CREATE TABLE presence (mac TEXT PRIMARY KEY, zone TEXT, since REAL, last_seen REAL);
CREATE INDEX idx_readings_ts ON readings(server_ts);
```

`readings` guarda tudo (bruto do ponto de vista do servidor) — e o dataset da calibracao da Fase 7. `atoms` e pre-populada com atom-a/A, atom-b/B, atom-c/C.

### 3.5 Algoritmo de zona (servidor)

Executado a cada ingestao e por timer de 1 s (para timeouts):

```
para cada beacon ativo:
  leituras = ultima leitura de cada ATOM para o beacon com idade <= PRESENCE_TIMEOUT_S
  se vazio: presence[mac] = None (sai da contagem)
  senao:
    candidata = zona do ATOM com maior rssi
    se presence[mac] e None: assume candidata direto (entrada rapida)
    senao se candidata != atual:
      troca somente se rssi(candidata) - rssi(atual) >= HYSTERESIS_DB
      continuamente por >= HYSTERESIS_HOLD_S (senao mantem atual)
contagem da zona = nº de beacons com presence = zona
qualquer mudanca de presence/contagem/status de ATOM -> broadcast no /ws
```

### 3.6 Firmware ESP-IDF

Estrutura:

```
firmware/
├── CMakeLists.txt
├── partitions.csv
├── sdkconfig.defaults            # comum
├── sdkconfig.atom-a|b|c          # identidade por dispositivo
└── main/
    ├── CMakeLists.txt
    ├── idf_component.yml         # espressif/led_strip ^3
    ├── Kconfig.projbuild         # AXION_ATOM_ID, AXION_ZONE, AXION_WIFI_SSID/PASS, AXION_API_URL
    ├── axion_config.h            # constantes da §3.1
    ├── main.c                    # boot, maquina de estados geral
    ├── ble_scan.c/.h             # NimBLE: scan passivo continuo + parse iBeacon
    ├── rssi_filter.c/.h          # janela por beacon, mediana/EMA, outliers, snapshot
    ├── net.c/.h                  # Wi-Fi STA + esp_http_client + whitelist
    ├── display.c/.h              # led_strip (RMT) + maquina de estados do display
    └── font5x5.h                 # glifos da §3.2
```

`sdkconfig.defaults` (chaves relevantes):

```
CONFIG_BT_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_BT_NIMBLE_ROLE_OBSERVER=y
CONFIG_BT_NIMBLE_ROLE_CENTRAL=y
CONFIG_BT_NIMBLE_ROLE_PERIPHERAL=n
CONFIG_BT_NIMBLE_ROLE_BROADCASTER=n
# ROLE_CENTRAL e obrigatorio no ESP32: o port forca host-based privacy
# (MYNEWT_VAL_BLE_HOST_BASED_PRIVACY=1), que so linka com o SM compilado,
# e o SM (ble_sm_alg.c) exige NIMBLE_BLE_CONNECT = central||peripheral.
CONFIG_ESP_COEX_SW_COEXIST_ENABLE=y
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_ESP_TASK_WDT_TIMEOUT_S=10
CONFIG_FREERTOS_HZ=1000
```

`partitions.csv` (sem OTA — app unica grande):

```
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xF000,  0x1000,
factory,  app,  factory, 0x10000, 0x3F0000,
```

Identidade por dispositivo — build de 3 imagens com o mesmo codigo:

```
idf.py -B build-a -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.atom-a" build flash
```

onde `sdkconfig.atom-a` define `CONFIG_AXION_ATOM_ID="atom-a"`, `CONFIG_AXION_ZONE="A"` (idem b/B, c/C). SSID/senha/URL da API ficam no `sdkconfig.defaults` local (fora do git — versionar um `sdkconfig.defaults.example`).

Tasks FreeRTOS:

| Task | Core | Prio | Funcao |
|---|---|---|---|
| NimBLE host | 0 | (default) | scan passivo continuo; callback de advertisement parseia iBeacon/MAC e grava na tabela de beacons (mutex) |
| `t_net` | 1 | 5 | Wi-Fi events, monta snapshot a cada 2 s (le tabela sob mutex), POST, whitelist |
| `t_display` | 1 | 3 | maquina de estados do display, 25 fps max |
| `main` | 1 | 1 | init, botao (debounce), watchdog feed |

BLE: `ble_gap_disc` com `passive=1, itvl=160, window=144, filter_duplicates=0`, duracao `BLE_HS_FOREVER`. Parse iBeacon: AD type 0xFF, company `0x004C`, subtipo `0x02 0x15`, extrai UUID/Major/Minor; filtro primario por MAC da whitelist. Wi-Fi: STA, DHCP, `WIFI_PS_MIN_MODEM`, reconexao com backoff (§3.1). Logs: `ESP_LOGI(TAG, ...)` com TAGs `SCAN|FILT|NET|DISP` — os testes de fase usam esses logs.

### 3.7 Repositorio

```
axion/
├── firmware/            # ESP-IDF (§3.6)
├── server/              # FastAPI: app/{main,zones,models,config,db}.py, static/ (dashboard), tests/
├── tools/               # simulador de leituras, scripts de calibracao (grid search, matriz de confusao)
├── docs/                # decisoes.md, calibracao.md, proximos-passos.md
└── PLANO_IMPLEMENTACAO.md
```

---

## 4. Fases

### Fase 0 — Toolchain e hello hardware — EM ANDAMENTO

Objetivo: compilar/gravar via ESP-IDF e controlar LED matrix + botao; beacons configurados.

Tarefas:
- [x] ESP-IDF instalado (v5.5.3 em `C:\esp\v5.5.3`); falta validar driver FTDI VCP / porta COM no primeiro flash.
- [x] `firmware/` criado conforme §3.6 (target `esp32`, partitions.csv, sdkconfig.defaults + variantes atom-a/b/c).
- [x] `display.c`: driver led_strip no G27, brilho max 40 aplicado em toda escrita, varredura de mapeamento no boot (index 0..24) e glifos da §3.2 (`font5x5.h`).
- [x] Botao G39 com debounce 30 ms por polling: cada pressao avanca um glifo da sequencia A B C 0..9 (na cor da zona).
- [ ] Configurar os 3 DX-CP29 no app DX-SMART conforme §1.2; anotar MAC/Major de cada em `docs/beacons.md`.

Comportamento do firmware no boot: banner com atom_id/zona -> varredura de LEDs (teste 0.2) -> letra da zona na cor da zona -> scan BLE ativo.

| # | Teste | PASS se |
|---|---|---|
| 0.1 | `idf.py build flash monitor` nos 3 ATOMs | grava e loga boot sem erro |
| 0.2 | Varredura de LEDs | ordem confirma mapeamento da §3.2 (ou remapeamento documentado) |
| 0.3 | Glifos | A/B/C e 0–9 legiveis nas cores da §3.2 |
| 0.4 | Botao | alterna glifo; sem bounce visivel |
| 0.5 | Beacons | app mostra os 3 com UUID/Major/intervalo/TX conforme §1.2; MACs anotados |

### Fase 1 — Scan BLE — EM ANDAMENTO

Objetivo: detectar os DX-CP29 e extrair MAC + RSSI continuamente.

Tarefas:
- [x] `ble_scan.c` conforme §3.6 (NimBLE observer-only, scan passivo continuo, parametros da §3.1, duplicatas nao filtradas).
- [x] Parse iBeacon (company 0x004C, subtipo 0x02/0x15, UUID/Major/Minor) + filtro por whitelist local em `axion_config.h`. Whitelist vazia => modo DISCOVERY: loga todo iBeacon com MAC, addr_type, UUID, Major/Minor — usar para levantar os MACs reais (teste 0.5) e preencher a whitelist. API assume na Fase 6.
- [x] Log por advertisement aceito: `SCAN: mac=<MAC> rssi=<dBm> major=<n> minor=<n>` + estatisticas a cada 10 s (`n`, taxa/s, RSSI medio por beacon — metricas dos testes 1.1/1.2/1.4).

Implementacao concluida; testes 1.1-1.5 pendentes de execucao com hardware e beacons em maos. Roteiro detalhado de execucao e troubleshooting: `docs/testes-fase1.md`.

| # | Teste | PASS se |
|---|---|---|
| 1.1 | Deteccao | cada beacon aparece no log em < 2 s apos ligar |
| 1.2 | Taxa | >= 3 advertisements/s por beacon a 1 m (adv 200 ms => teorico ~5/s) |
| 1.3 | Filtro | zero dispositivos fora da whitelist no log em 5 min |
| 1.4 | RSSI x distancia | media a 1 m > media a 8 m por >= 15 dB (valores anotados p/ Fase 7) |
| 1.5 | MAC estatico | MAC de cada beacon identico apos reboot do beacon e 24 h depois |

### Fase 2 — Filtro RSSI no firmware — EM ANDAMENTO

Objetivo: RSSI estavel por beacon por janela; snapshot pronto para envio.

Tarefas:
- [x] `rssi_filter.c`: janela `RSSI_WINDOW_MS` por beacon (ring buffer de 32 amostras), mediana (default) e EMA (selecionavel por `AXION_RSSI_FILTER` em axion_config.h), descarte de outliers `> 2 sigma`, expiracao de beacon sem leitura.
- [x] Snapshot periodico (`SNAPSHOT_PERIOD_MS`) no formato da §3.3; log `FILT: snapshot ...` a cada ciclo (emitido pela t_net, com ou sem Wi-Fi).
- [x] Tabela de beacons protegida por secao critica (escrita no callback NimBLE, leitura em `t_net`).

Implementacao concluida; testes 2.1-2.3 pendentes (usar as linhas `FILT:` do log). Roteiro detalhado: `docs/testes-fase2.md`.

| # | Teste | PASS se |
|---|---|---|
| 2.1 | Estabilidade | beacon parado a 3 m: filtrado varia <= +-3 dB em 60 s (bruto tipicamente +-10) |
| 2.2 | Responsividade | mover 1 m -> 8 m: filtrado converge em <= 5 s |
| 2.3 | Expiracao | beacon desligado some do snapshot em <= 2 janelas |

### Fase 3 — Wi-Fi + HTTP + coexistencia — EM ANDAMENTO

Objetivo: POST do snapshot a cada 2 s sem degradar o scan; deteccao de offline.

Tarefas:
- [x] `net.c`: Wi-Fi STA com reconexao/backoff exponencial (1 s -> 30 s), `esp_http_client` com keep-alive e timeout da §3.1, `WIFI_PS_MIN_MODEM` p/ coexistencia. SSID/senha/URL via `idf.py menuconfig` > menu AXION (SSID vazio = Wi-Fi desabilitado, roda so Fases 1-2). JSON via snprintf (cJSON saiu do core no IDF v6).
- [x] Estado OFFLINE por `OFFLINE_AFTER_FAILS` (`net_state()`, consumido pelo display na Fase 5; `net_zone_count()` guarda a contagem da resposta).
- [x] Contador de advertisements por janela no log (linha `diag 10s: adv_total=...` — metrica do teste 3.2).
- [x] Mock da API (`tools/mock_api.py`, stdlib: loga cada POST e responde `zone_count` = nº de beacons do payload).

Implementacao concluida; testes 3.1-3.5 pendentes. Roteiro detalhado: `docs/testes-fase3.md` (o mock detecta gaps/seq perdida sozinho; heap na linha `diag 10s`).

| # | Teste | PASS se |
|---|---|---|
| 3.1 | Entrega | mock recebe dos 3 ATOMs a cada ~2 s por 10 min, sem gaps > 6 s |
| 3.2 | Coexistencia | advertisements/min com Wi-Fi ativo >= 70% da Fase 1 |
| 3.3 | Resiliencia | derrubar Wi-Fi 1 min: flag OFFLINE em <= 6 s, recuperacao sozinha em <= 40 s |
| 3.4 | Estabilidade | 1 h sem reboot; `esp_get_free_heap_size()` estavel (sem tendencia de queda) |
| 3.5 | Watchdog | nenhum reset por task WDT durante 3.4 |

### Fase 4 — API, banco e algoritmo de zona — IMPLEMENTADA (testes 4.1/4.2 PASS)

Objetivo: backend completo com decisao de zona testada sem hardware.

Tarefas:
- [x] FastAPI + SQLite conforme §3.3/§3.4 (`server/`); seed dos 3 atoms e 3 beacons demo.
- [x] Algoritmo da §3.5 em modulo puro (`server/app/zones.py`, sem I/O) para testes unitarios.
- [x] Status de ATOM (ONLINE/OFFLINE por `ATOM_OFFLINE_S`), recompute periodico de 1 s p/ timeouts.
- [x] `tools/simulator.py`: reproduz cenarios (entrada, travessia, fronteira, beacon sumindo) contra a API real.

Status dos testes: 4.1 e 4.2 PASS (10/10 unitarios verdes + simulador end-to-end OK, validado no PC). 4.3 e 4.4 pendentes de hardware (3 ATOMs reais).

| # | Teste | PASS se |
|---|---|---|
| 4.1 | Unitarios | pytest verde: argmax, entrada rapida, histerese (dB e hold), timeout de presenca |
| 4.2 | Dupla contagem | beacon visto por 2 ATOMs conta em exatamente 1 zona (unitario + simulador) |
| 4.3 | Integracao real | 3 ATOMs + 3 beacons: `/api/state` reflete a posicao fisica real |
| 4.4 | ATOM offline | desligar 1 ATOM: OFFLINE em `/api/state` em <= 15 s |

### Fase 5 — Display definitivo — IMPLEMENTADA (testes de bancada pendentes)

Objetivo: maquina de estados da §3.2 completa nos 3 ATOMs.

Tarefas:
- [x] Ciclo letra/contagem (2 s / 2 s): usa `zone_count` da API quando ONLINE; sem API usa a contagem local (com histerese de limiar, para nao piscar entre valores).
- [x] Maquina de estados no `main.c`: NET_CONNECTING (pixel central amarelo piscando), NET_OFFLINE (matriz vermelha), NET_ONLINE/DISABLED (RUN). `net_state()` em `net.c`.
- [x] Cadencia 2 s/2 s: legivel a distancia sem parecer lenta; justificada em `docs/decisoes.md`.

Status: codigo pronto. Testes 5.1-5.3 pendentes de execucao com hardware.

| # | Teste | PASS se |
|---|---|---|
| 5.1 | Ciclo | alternancia 2 s / 2 s correta e legivel nos 3 ATOMs, cores da §3.2 |
| 5.2 | Atualizacao | mover beacon de zona: contagem nos displays das duas zonas em <= 10 s |
| 5.3 | Offline | derrubar API: matriz vermelha em <= 6 s; religar: RUN sozinho em <= 10 s |

### Fase 6 — Dashboard + cadastro — IMPLEMENTADA (testes de integracao pendentes)

Objetivo: as duas telas do PDF, com whitelist dinamica fechando o ciclo.

Tarefas:
- [x] Dashboard (`server/static/index.html`): card por ATOM (ONLINE/OFFLINE, zona, contagem, operadores presentes), via `/ws` com reconexao automatica + carga inicial por REST.
- [x] Cadastro (`server/static/beacons.html`): tabela (operador, cracha, MAC, status) + formulario novo beacon; ativar/desativar/remover (PUT/DELETE). Rejeita badge/MAC duplicado (400, validado).
- [x] Firmware: whitelist via `GET /api/beacons/active` no boot + refresh imediato quando `whitelist_ver` muda no POST + refresh periodico (`net.c` / `ble_scan_set_whitelist`); MACs hardcoded viram fallback.

Status: codigo pronto e paginas validadas no PC. Testes 6.1-6.4 pendentes de hardware.

| # | Teste | PASS se |
|---|---|---|
| 6.1 | Tempo real | mover beacon: dashboard atualiza em <= 10 s sem refresh |
| 6.2 | ATOM offline | desligar ATOM: card OFFLINE em <= 15 s |
| 6.3 | Cadastro | cadastrar beacon novo na UI: ATOM passa a reporta-lo sem reflash em <= 70 s |
| 6.4 | Desativacao | desativar beacon: sai da contagem e o ATOM para de reporta-lo |

### Fase 7 — Calibracao e validacao de precisao — PENDENTE

Objetivo: medir acuracia e fixar os parametros finais (criterio central de avaliacao do desafio).

Tarefas:
- [ ] Layout fisico: ATOMs a ~2 m de altura, centro de cada zona, longe de metal; croqui em `docs/calibracao.md`.
- [ ] Coleta: por zona, 5 posicoes x 60 s parado + 10 travessias entre zonas (dataset = tabela `readings` + anotacao de verdade-terrestre com timestamps).
- [ ] `tools/calibra.py`: grid search offline — filtro (mediana vs EMA vs Kalman 1D) x `HYSTERESIS_DB` (3/5/8) x `HYSTERESIS_HOLD_S` (2/3/5) x janela (2/3/5 s); saida = acuracia estatica, latencia de troca, trocas espurias.
- [ ] Matriz de confusao (zona real x detectada) e parametros vencedores aplicados na §3.1 (mudanca registrada).

| # | Teste | PASS se |
|---|---|---|
| 7.1 | Acuracia estatica | >= 90% com beacon parado dentro de cada zona |
| 7.2 | Transicao | troca detectada em <= 10 s apos cruzar a fronteira |
| 7.3 | Fronteira | parado na divisa: <= 1 troca de zona por minuto |
| 7.4 | Multi-beacon | 3 beacons simultaneos em zonas distintas, todos corretos por 10 min |

Se 7.1 < 90% por limitacao fisica (zonas sobrepostas), documentar causa e mitigacoes em `docs/calibracao.md` — PASS condicional (o PDF nao exige precisao perfeita; exige metodo).

### Fase 8 — Documentacao, demo e entrega — PENDENTE

Objetivo: entrega conforme os 4 criterios de avaliacao do PDF.

Tarefas:
- [ ] `README.md`: subir firmware (3 builds) + API + dashboard do zero.
- [ ] `docs/decisoes.md`: decisoes, trade-offs, alternativas descartadas, fontes.
- [ ] `docs/proximos-passos.md`: MQTT, >1 ATOM por zona, fingerprinting, OTA, autenticacao da API, MAC aleatorio.
- [ ] Roteiro de demo (5 min): entrada na Zona A -> display+dashboard -> travessia p/ B -> ATOM offline -> cadastro de beacon novo ao vivo; 2 ensaios completos.

| # | Teste | PASS se |
|---|---|---|
| 8.1 | Setup limpo | terceiro sobe API+dashboard apenas com o README |
| 8.2 | Demo E2E | roteiro executado 2x sem falha |
| 8.3 | Cobertura do PDF | identificacao por zona, contagem, display (letra/numero/offline), dashboard, cadastro — tudo demonstrado |
| 8.4 | Comunicacao | docs revisados, com fontes citadas |

---

## 5. Riscos e mitigacoes

| Risco | Mitigacao |
|---|---|
| Coexistencia Wi-Fi/BLE degrada scan | coex SW do IDF, POST curto com keep-alive, duty de scan 90%, medicao objetiva (teste 3.2) |
| RSSI instavel indoor (multipath; corpo absorve 2.4 GHz) | filtro + histerese + calibracao (Fases 2/4/7); ATOMs a ~2 m de altura |
| Zonas fisicamente proximas | reduzir TX do beacon, aumentar `HYSTERESIS_DB` (grid search da Fase 7) |
| MAC aleatorio no beacon | teste 1.5 valida MAC estatico; fallback: identificar por UUID+Major |
| Corrente/dano nos LEDs | brilho max 40/255 fixado em codigo (§1.1) |
| Payload/config divergente entre fw e servidor | contratos e constantes fixados nas §3.1/§3.3, espelhados em `axion_config.h` e `config.py` |

## 6. Referencias

- `AXION_POC_Desafio_v2.pdf` (BrSupply, v2.0)
- M5Stack ATOM Matrix v1.1 — docs oficiais: https://docs.m5stack.com/en/core/Atom-Matrix_v1.1
- DX-CP29 Card Bluetooth Beacon — manual: https://manuals.plus/ae/1005009844601366
- ESP-IDF v5.5 — NimBLE, esp_http_client, esp_wifi, coexistencia: https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/
- Componente led_strip: https://components.espressif.com/components/espressif/led_strip
- Literatura para `docs/decisoes.md`: surveys de indoor positioning RSSI (proximidade/zona vs trilateracao vs fingerprinting); filtros (mediana, EMA, Kalman 1D) para RSSI BLE
