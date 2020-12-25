#define HID_HS_BINTERVAL              0x01U
#define HID_FS_BINTERVAL              0x01U
#define CUSTOM_HID_HS_BINTERVAL              0x01U
#define CUSTOM_HID_FS_BINTERVAL              0x01U

#include <Arduino.h>

#define USBCON
#define USBD_USE_HID_COMPOSITE

#include <USBCompositeSerial.h>
#include <USBHID.h>
#include <usb_hid_keys.h>
#include "../../../.platformio/packages/framework-arduinoststm32-maple/STM32F1/cores/maple/wirish.h"

extern HardwareTimer Timer2;

#define LEFT_SIDE
#define SEMPAI_PART

const byte leftMatrix[6][8] = {
        {KEY_ESC,      KEY_F1,        KEY_F2,      KEY_F3,        KEY_F4,      KEY_F5,    KEY_F6,    KEY_NONE},
        {KEY_GRAVE,    KEY_EQUAL,     KEY_1,       KEY_2,         KEY_3,       KEY_4,     KEY_5,     KEY_PAGEUP},
        {KEY_TAB,      KEY_LEFTBRACE, KEY_Q,       KEY_W,         KEY_E,       KEY_R,     KEY_T,     KEY_UP},
        {KEY_CAPSLOCK, KEY_BACKSLASH, KEY_A,       KEY_S,         KEY_D,       KEY_F,     KEY_G,     KEY_LEFT},
        {KEY_LEFTSHIFT,KEY_NONE,      KEY_Z,       KEY_X,         KEY_C,       KEY_V,     KEY_B,     KEY_HOME},
        {KEY_NONE,     KEY_LEFTCTRL,  KEY_NONE,    KEY_LEFTMETA,  KEY_LEFTALT, KEY_NONE,  KEY_SPACE, KEY_NONE},
};

const byte rightMatrix[6][8] = {
        {KEY_F7,       KEY_F8,   KEY_F9,   KEY_F10,   KEY_F11,   KEY_F12,       KEY_SYSRQ,      KEY_NONE},
        {KEY_PAGEDOWN, KEY_6,    KEY_7,    KEY_8,     KEY_9,     KEY_0,         KEY_MINUS,      KEY_DELETE},
        {KEY_DOWN,     KEY_Y,    KEY_U,    KEY_I,     KEY_O,     KEY_P,         KEY_RIGHTBRACE, KEY_BACKSPACE},
        {KEY_RIGHT,    KEY_H,    KEY_J,    KEY_K,     KEY_L,     KEY_SEMICOLON, KEY_APOSTROPHE, KEY_ENTER},
        {KEY_END,      KEY_N,    KEY_M,    KEY_COMMA, KEY_DOT,   KEY_SLASH,     KEY_NONE,       KEY_RIGHTSHIFT,},
        {KEY_NONE,     KEY_NONE, KEY_NONE, KEY_NONE,  KEY_SPACE, KEY_RIGHTALT,  KEY_COMPOSE,    KEY_RIGHTCTRL}
};

#ifdef LEFT_SIDE
#define mainKeyMatrix leftMatrix
#define auxKeyMatrix rightMatrix
#endif

#ifdef RIGHT_SIDE
#define mainKeyMatrix rightMatrix
#define auxKeyMatrix leftMatrix
#endif

// for some reason PB8 is dead
// 07.10.2020 huku
byte keyGridPins[] = {PB10, PB11, PB12, PB13, PB14, PB7};

#define GPIO_PIN_0                 ((uint16_t)0x0001)  /* Pin 0 selected    */
#define GPIO_PIN_1                 ((uint16_t)0x0002)  /* Pin 1 selected    */
#define GPIO_PIN_2                 ((uint16_t)0x0004)  /* Pin 2 selected    */
#define GPIO_PIN_3                 ((uint16_t)0x0008)  /* Pin 3 selected    */
#define GPIO_PIN_4                 ((uint16_t)0x0010)  /* Pin 4 selected    */
#define GPIO_PIN_5                 ((uint16_t)0x0020)  /* Pin 5 selected    */
#define GPIO_PIN_6                 ((uint16_t)0x0040)  /* Pin 6 selected    */
#define GPIO_PIN_7                 ((uint16_t)0x0080)  /* Pin 7 selected    */
#define GPIO_PIN_8                 ((uint16_t)0x0100)  /* Pin 8 selected    */
#define GPIO_PIN_9                 ((uint16_t)0x0200)  /* Pin 9 selected    */
#define GPIO_PIN_10                ((uint16_t)0x0400)  /* Pin 10 selected   */
#define GPIO_PIN_11                ((uint16_t)0x0800)  /* Pin 11 selected   */
#define GPIO_PIN_12                ((uint16_t)0x1000)  /* Pin 12 selected   */
#define GPIO_PIN_13                ((uint16_t)0x2000)  /* Pin 13 selected   */
#define GPIO_PIN_14                ((uint16_t)0x4000)  /* Pin 14 selected   */
#define GPIO_PIN_15                ((uint16_t)0x8000)  /* Pin 15 selected   */
#define GPIO_PIN_All               ((uint16_t)0xFFFF)  /* All pins selected */

byte report[8] = {0, 0, 0, 0, 0, 0, 0, 0};
int posInReport = 2;

void sendReport() {
    byte *reportPosition = report;
    unsigned toSend = 8;
    while (posInReport <= 7) {
        report[posInReport++] = 0;
    }
    while (toSend) {
        unsigned delta = usb_hid_tx(reportPosition, toSend);
        toSend -= delta;
        reportPosition += delta;
    }
    posInReport = 2;
    report[0] = 0;
}

int addToReport(byte keycode) {
    if (keycode >= KEY_LEFTCTRL && keycode <= KEY_RIGHTMETA) {
        report[0] = 1 << (keycode & 0x0f);
        return 0;
    }
    if (posInReport > 7)
        return 1;
    report[posInReport++] = keycode;
    return 0;
}

void pollMatrix() {
    for (int i = 0; i < 6; ++i) {
        if (i > 0)
            digitalWrite(keyGridPins[i - 1], 0);
        digitalWrite(keyGridPins[i], 1);
        int lineData = GPIOA->regs->IDR;
        for (int j = 0; j < 8; ++j) {
            if (lineData & (1 << j))
                addToReport(mainKeyMatrix[i][j]);
        }
    }
    digitalWrite(keyGridPins[5], 0);
}

void pollMatrixKohai(byte *buffer) {
    for (int i = 0; i < 6; ++i) {
        if (i > 0)
            digitalWrite(keyGridPins[i - 1], 0);
        digitalWrite(keyGridPins[i], 1);
        buffer[i] = (GPIOA->regs->IDR) & 0xFF;
    }
    digitalWrite(keyGridPins[5], 0);
}

int needPollMatrix = 0;

void requestPollMatrix() {
    needPollMatrix = 1;
}

bool numLockEnabled = false;

void setup() {
    Serial1.begin(230400);
    pinMode(LED_BUILTIN, OUTPUT);

    pinMode(PA0, INPUT_PULLDOWN);
    pinMode(PA1, INPUT_PULLDOWN);
    pinMode(PA2, INPUT_PULLDOWN);
    pinMode(PA3, INPUT_PULLDOWN);
    pinMode(PA4, INPUT_PULLDOWN);
    pinMode(PA5, INPUT_PULLDOWN);
    pinMode(PA6, INPUT_PULLDOWN);
    pinMode(PA7, INPUT_PULLDOWN);

    for (int i = 0; i < 6; ++i) {
        pinMode(keyGridPins[i], OUTPUT);
        digitalWrite(keyGridPins[i], 0);
    }

#ifdef SEMPAI_PART
    USBHID.begin(HID_BOOT_KEYBOARD, 0x0, 0x1337, "HukuToc2288 aka NTWare", "The tru\" ergonocmic keyboard",
                 "00285000000000000031");
    BootKeyboard.begin(); // needed just in case you need LED support
    delay(3000);
//    for (int i = 0; i < 100; ++i) {
//        pollMatrix();
//        delay(1);
//    };
    Timer2.pause();
    Timer2.setCount(0);
    Timer2.setMode(TIMER_CH1, TIMER_OUTPUTCOMPARE);
    Timer2.setPrescaleFactor(72);
    Timer2.setOverflow(1000);
    Timer2.setCompare(TIMER_CH1, 1);      // overflow might be small
    Timer2.attachInterrupt(TIMER_CH1, requestPollMatrix);
    Timer2.refresh();
    Timer2.resume();

    if ((BootKeyboard.getLEDs() & 0x01) == 0) {
        Serial1.write('n');
        numLockEnabled = false;
    }
    if ((BootKeyboard.getLEDs() & 0x01) != 0) {
        Serial1.write('N');
        numLockEnabled = true;
    }
#endif
}

byte lastKohaiReport[6] = {0,0,0,0,0,0};

void loop() {
#ifdef SEMPAI_PART
    if (needPollMatrix) {
        sendReport();
        Serial1.write('k');
        byte leds = BootKeyboard.getLEDs();
        if (numLockEnabled && (leds & 0x01) == 0) {
            Serial1.write('n');
            numLockEnabled = false;
        }
        if (!numLockEnabled && (leds & 0x01) != 0) {
            Serial1.write('N');
            numLockEnabled = true;
        }
        needPollMatrix = 0;
        pollMatrix();
        digitalWrite(LED_BUILTIN, !(leds & 0x02));
        if (Serial1.available() >= 6) {
            for (int i = 0; i < 6; ++i) {
                byte lineData = Serial1.read();
                lastKohaiReport[i] = lineData;
                for (int j = 0; j < 8; ++j) {
                    if (lineData & (1 << j))
                        addToReport(auxKeyMatrix[i][j]);
                }
            }
        } else {
            for (int i = 0; i < 6; ++i) {
                byte lineData = lastKohaiReport[i];
                for (int j = 0; j < 8; ++j) {
                    if (lineData & (1 << j))
                        addToReport(auxKeyMatrix[i][j]);
                }
            }
        }
    }
#endif

#ifdef KOHAI_PART
    int cmd = Serial1.read();
    if (cmd < 0)
        return;
    if (cmd == 'k') {
        byte buffer[6];
        pollMatrixKohai(buffer);
        Serial1.write(buffer, 6);
    } else if (cmd == 'n') {
        digitalWrite(LED_BUILTIN, 1);
    } else if (cmd == 'N') {
        digitalWrite(LED_BUILTIN, 0);
    }
#endif
}