# Vuesp ESP-NOW Client

ESP32-C3-клиент с кнопкой и температурой. Отправляет события на Vuesp ESP-NOW Hub
по текстовому протоколу VLP/1 и принимает команды обратно.

## Документация

Подробное описание протокола, распиновки и ограничений — в [DOCS.md](./DOCS.md).

## Схема

```text
button / temperature -> client (ESP32-C3) --ESP-NOW--> hub (ESP32) -> vuesp-hub
```

## Поведение

- при старте рассылает `HELLO` и ждёт `HELLO_ACK` от хаба;
- при нажатии кнопки отправляет `EVT k=btn`;
- раз в 30 секунд отправляет `EVT k=temp`;
- раз в 30 секунд отправляет `HB`;
- принимает `CMD` и подтверждает их `ACK`;
- выполняет команды `relay`, `led`, `output` для указанного GPIO.

## Настройка

В `src/espnow/espnow.cpp`:

```cpp
#define ESPNOW_CHANNEL 6
#define CLIENT_BUTTON_GPIO 4
```

Канал должен совпадать с хабом. Кнопка подключается между GPIO 4 и GND,
используется внутренняя подтяжка.

## Сборка

```bash
pio run -e esp32-c3-devkitm-1
pio run -e esp32-c3-devkitm-1 -t upload
pio device monitor -b 115200
```

## VLP/1

```text
VLP1|HELLO|src=8A1B2C3D|fw=0.1.0|board=esp32-c3|caps=btn,temp|ch=6
VLP1|EVT|src=8A1B2C3D|seq=42|up=123456|k=btn|gpio=4|val=1
VLP1|CMD|dst=8A1B2C3D|mid=7|action=relay|gpio=12|val=1
VLP1|ACK|src=8A1B2C3D|mid=7|ok=1
```

## Ограничения

- Нет deep sleep в текущей версии: клиент постоянно слушает команды.
- Для батарейного режима нужно добавить сон и очередь команд на хабе.
- `temperatureRead()` отдаёт внутреннюю температуру чипа, а не внешнего датчика.
  Для DS18B20 замените чтение температуры в `espnowLoop()`.
