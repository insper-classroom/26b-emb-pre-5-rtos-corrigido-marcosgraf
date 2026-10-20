/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * EXE 5 -- dois LEDs que ligam e desligam o pisca no proprio botao.
 *
 * ESTRUTURA (a mesma do diagrama do enunciado):
 *
 *   btn_callback  --xQueueBtn-->  btn_task  --xSemaphoreLedR-->  led_r_task
 *       (ISR)                               --xSemaphoreLedY-->  led_y_task
 *
 * Por que tres camadas e nao uma so:
 *
 *   - A ISR precisa ser curta. Ela so empacota "qual pino foi" e sai.
 *   - A fila atravessa a fronteira ISR -> task. E o unico caminho seguro:
 *     dentro de interrupcao so vale a versao FromISR das funcoes.
 *   - O semaforo atravessa a fronteira task -> task, e serve para ROTEAR: cada
 *     LED espera o seu, entao o botao vermelho nunca acorda o LED amarelo.
 *
 * O estado "este LED esta piscando" e variavel LOCAL de cada led_task -- o
 * enunciado proibe global que nao seja recurso do RTOS, e aqui isso nem faz
 * falta: so a propria task precisa saber.
 */

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>


#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/irq.h"

const int BTN_PIN_R = 28;
const int BTN_PIN_Y = 21;

const int LED_PIN_R = 5;
const int LED_PIN_Y = 10;

// Meia onda do pisca: 100 ms aceso, 100 ms apagado.
const int PISCA_MS = 100;

QueueHandle_t xQueueBtn;

SemaphoreHandle_t xSemaphoreLedR;
SemaphoreHandle_t xSemaphoreLedY;


// ---------------------------------------------------------------- ISR -----

// Um callback atende os dois botoes: no RP2040 o handler de GPIO e
// compartilhado, e o parametro 'gpio' diz qual pino disparou. E esse numero que
// vai para a fila -- a ISR nao decide nada, so informa.
void btn_callback(uint gpio, uint32_t events) {
    int id = (int) gpio;

    if (events == 0x4) { // fall edge: apertou (pull-up indo para o GND)
        xQueueSendFromISR(xQueueBtn, &id, 0);
    }
}


// --------------------------------------------------------------- TASKS -----

// Recebe o pino apertado e libera o semaforo do LED correspondente.
//
// Esta task e so um despachante. Ela nao sabe se o LED esta piscando ou nao --
// quem guarda isso e a propria led_task. Por isso o semaforo aqui significa
// "houve um aperto no seu botao", e nao "ligue" nem "desligue".
void btn_task(void* p) {
    gpio_init(BTN_PIN_R);
    gpio_set_dir(BTN_PIN_R, GPIO_IN);
    gpio_pull_up(BTN_PIN_R);
    gpio_set_irq_enabled_with_callback(BTN_PIN_R, GPIO_IRQ_EDGE_FALL, true,
                                       &btn_callback);

    gpio_init(BTN_PIN_Y);
    gpio_set_dir(BTN_PIN_Y, GPIO_IN);
    gpio_pull_up(BTN_PIN_Y);
    gpio_set_irq_enabled_with_callback(BTN_PIN_Y, GPIO_IRQ_EDGE_FALL, true,
                                       &btn_callback);

    int id = 0;

    while (true) {

        // portMAX_DELAY: fica BLOQUEADA ate chegar aperto. Nao gasta CPU
        // nenhuma esperando -- nada de while vazio consultando o pino.
        if (xQueueReceive(xQueueBtn, &id, portMAX_DELAY) == pdTRUE) {

            if (id == BTN_PIN_R) {
                xSemaphoreGive(xSemaphoreLedR);
            }

            if (id == BTN_PIN_Y) {
                xSemaphoreGive(xSemaphoreLedY);
            }
        }
    }
}


// LED vermelho.
//
// O TRUQUE DESTA TASK: o timeout do xSemaphoreTake e, ao mesmo tempo, o relogio
// do pisca. Duas coisas num comando so:
//
//   - devolveu pdTRUE  -> chegou aperto: inverte o modo piscando.
//   - devolveu pdFALSE -> passaram-se 100 ms sem aperto: hora de inverter o
//                         nivel do LED, se ele estiver no modo piscando.
//
// Fazer com vTaskDelay(100) em vez do timeout nao funcionaria direito: a task
// ficaria presa no delay e so perceberia o aperto ate 100 ms depois, deixando o
// LED aceso alem da conta na hora de desligar. Bloqueada no semaforo, ela reage
// na hora.
void led_r_task(void* p) {
    gpio_init(LED_PIN_R);
    gpio_set_dir(LED_PIN_R, GPIO_OUT);
    gpio_put(LED_PIN_R, 0);

    int piscando = 0;   // modo atual
    int aceso = 0;      // nivel atual do pino, para poder inverter

    while (true) {

        if (xSemaphoreTake(xSemaphoreLedR, pdMS_TO_TICKS(PISCA_MS)) == pdTRUE) {
            // Apertaram o botao vermelho: troca de modo e comeca do apagado.
            piscando = !piscando;
            aceso = 0;
            gpio_put(LED_PIN_R, 0);
            continue;
        }

        // Timeout: completou mais 100 ms.
        if (piscando) {
            aceso = !aceso;
            gpio_put(LED_PIN_R, aceso);
        }
    }
}


// LED amarelo: identica a de cima, no outro pino e no outro semaforo.
//
// Sao duas tasks separadas de proposito: cada uma tem o seu proprio 'piscando'
// e o seu proprio relogio de 100 ms, entao uma cor nao atrapalha o ritmo da
// outra nem precisa saber que a outra existe.
void led_y_task(void* p) {
    gpio_init(LED_PIN_Y);
    gpio_set_dir(LED_PIN_Y, GPIO_OUT);
    gpio_put(LED_PIN_Y, 0);

    int piscando = 0;
    int aceso = 0;

    while (true) {

        if (xSemaphoreTake(xSemaphoreLedY, pdMS_TO_TICKS(PISCA_MS)) == pdTRUE) {
            piscando = !piscando;
            aceso = 0;
            gpio_put(LED_PIN_Y, 0);
            continue;
        }

        if (piscando) {
            aceso = !aceso;
            gpio_put(LED_PIN_Y, aceso);
        }
    }
}


int main() {
    stdio_init_all();

    // Os recursos do RTOS tem de existir ANTES das tasks que os usam: a
    // primeira coisa que btn_task faz e ligar a interrupcao, e a ISR ja pode
    // disparar com o dedo no botao durante o boot.
    xQueueBtn = xQueueCreate(32, sizeof(int));

    xSemaphoreLedR = xSemaphoreCreateBinary();
    xSemaphoreLedY = xSemaphoreCreateBinary();

    xTaskCreate(btn_task, "BTN_Task 1", 256, NULL, 1, NULL);
    xTaskCreate(led_r_task, "LED_R_Task", 256, NULL, 1, NULL);
    xTaskCreate(led_y_task, "LED_Y_Task", 256, NULL, 1, NULL);

    vTaskStartScheduler();

    while(1){}

    return 0;
}
