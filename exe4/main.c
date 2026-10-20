#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "pico/stdlib.h"
#include <stdio.h>

const int BTN_PIN_R = 28;
const int BTN_PIN_G = 26;

const int LED_PIN_R = 4;
const int LED_PIN_G = 6;

// Filas que levam o DELAY da btn_task para a led_task (uma por cor).
QueueHandle_t xQueueButId;
QueueHandle_t xQueueButId_g;

// Filas que levam o AVISO DE APERTO da interrupcao para a btn_task (uma por
// cor). O README pede xQueueSendFromISR, e e aqui que ele entra.
QueueHandle_t xQueueBtnR;
QueueHandle_t xQueueBtnG;

// UM callback so atende os dois botoes -- no RP2040 o handler de GPIO e
// compartilhado. O parametro 'gpio' diz qual pino disparou, e e por ele que se
// escolhe a fila. Sem esse teste, apertar o botao verde tambem acionaria o LED
// vermelho, porque os dois pinos chamam esta mesma funcao.
//
// Dentro de ISR vale a versao FromISR da funcao, nunca a normal: a comum pode
// bloquear a task que a chamou, e interrupcao nao e task -- nao ha o que
// bloquear. Terminar em "FromISR" e a dica de que a funcao e segura aqui.
void btn_callback(uint gpio, uint32_t events) {
    int id = (int) gpio;

    if (events == 0x4) { // fall edge
        if (gpio == BTN_PIN_R) {
            xQueueSendFromISR(xQueueBtnR, &id, 0);
        }
        if (gpio == BTN_PIN_G) {
            xQueueSendFromISR(xQueueBtnG, &id, 0);
        }
    }
}

void led_1_task(void *p) {
    gpio_init(LED_PIN_R);
    gpio_set_dir(LED_PIN_R, GPIO_OUT);

    int delay = 0;

    while (true) {
        if (xQueueReceive(xQueueButId, &delay, 0)) {
            printf("%d\n", delay);
        }

        if (delay > 0) {
            gpio_put(LED_PIN_R, 1);
            vTaskDelay(pdMS_TO_TICKS(delay));
            gpio_put(LED_PIN_R, 0);
            vTaskDelay(pdMS_TO_TICKS(delay));
        }
    }
}

void btn_1_task(void *p) {
    gpio_init(BTN_PIN_R);
    gpio_set_dir(BTN_PIN_R, GPIO_IN);
    gpio_pull_up(BTN_PIN_R);
    gpio_set_irq_enabled_with_callback(BTN_PIN_R, GPIO_IRQ_EDGE_FALL, true,
                                       &btn_callback);

    int id = 0;
    int delay = 0;
    while (true) {
        // portMAX_DELAY = espera indefinidamente. A task fica BLOQUEADA ate a
        // interrupcao por alguma coisa na fila -- zero CPU enquanto isso. E a
        // grande vantagem sobre o pooling do exe3.
        if (xQueueReceive(xQueueBtnR, &id, portMAX_DELAY)) {
            if (delay < 1000) {
                delay += 100;
            } else {
                delay = 100;
            }
            printf("delay btn %d \n", delay);
            xQueueSend(xQueueButId, &delay, 0);
        }
    }
}

void led_2_task(void *p) {
    gpio_init(LED_PIN_G);
    gpio_set_dir(LED_PIN_G, GPIO_OUT);

    int delay = 0;

    while (true) {
        if (xQueueReceive(xQueueButId_g, &delay, 0)) {
            printf("%d\n", delay);
        }

        if (delay > 0) {
            gpio_put(LED_PIN_G, 1);
            vTaskDelay(pdMS_TO_TICKS(delay));
            gpio_put(LED_PIN_G, 0);
            vTaskDelay(pdMS_TO_TICKS(delay));
        }
    }
}

void btn_2_task(void *p) {
    gpio_init(BTN_PIN_G);
    gpio_set_dir(BTN_PIN_G, GPIO_IN);
    gpio_pull_up(BTN_PIN_G);
    gpio_set_irq_enabled_with_callback(BTN_PIN_G, GPIO_IRQ_EDGE_FALL, true,
                                       &btn_callback);

    int id = 0;
    int delay = 0;
    while (true) {
        if (xQueueReceive(xQueueBtnG, &id, portMAX_DELAY)) {
            if (delay < 1000) {
                delay += 100;
            } else {
                delay = 100;
            }
            printf("delay btn %d \n", delay);
            xQueueSend(xQueueButId_g, &delay, 0);
        }
    }
}

int main() {
    stdio_init_all();
    printf("Start RTOS \n");

    xQueueButId = xQueueCreate(32, sizeof(int));
    xQueueButId_g = xQueueCreate(32, sizeof(int));
    xQueueBtnR = xQueueCreate(32, sizeof(int));
    xQueueBtnG = xQueueCreate(32, sizeof(int));

    xTaskCreate(led_1_task, "LED_Task 1", 256, NULL, 1, NULL);
    xTaskCreate(btn_1_task, "BTN_Task 1", 256, NULL, 1, NULL);
    xTaskCreate(led_2_task, "LED_Task 2", 256, NULL, 1, NULL);
    xTaskCreate(btn_2_task, "BTN_Task 2", 256, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}
