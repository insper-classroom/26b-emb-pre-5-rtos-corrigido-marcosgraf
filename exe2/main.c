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

SemaphoreHandle_t xSemaphore_r;
SemaphoreHandle_t xSemaphore_g;

void led_1_task(void *p) {
  gpio_init(LED_PIN_R);
  gpio_set_dir(LED_PIN_R, GPIO_OUT);

  int delay = 250;

  while (true) {

    if (xSemaphoreTake(xSemaphore_r, pdMS_TO_TICKS(500)) == pdTRUE) {
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

  while (true) {
    if (!gpio_get(BTN_PIN_R)) {
      while (!gpio_get(BTN_PIN_R)) {
        vTaskDelay(pdMS_TO_TICKS(1));
      }
      xSemaphoreGive(xSemaphore_r);
    }
  }
}

// LED G: espera o semaforo verde. Um semaforo POR LED -- se as duas cores
// dividissem o mesmo semaforo, apertar o botao vermelho acenderia o LED que
// chegasse primeiro no xSemaphoreTake, e nao o vermelho.
void led_2_task(void *p) {
  gpio_init(LED_PIN_G);
  gpio_set_dir(LED_PIN_G, GPIO_OUT);

  int delay = 250;

  while (true) {

    // O 500 ms aqui e so o tempo maximo de espera. Se o semaforo chegar antes,
    // a task acorda na hora; se nao chegar, xSemaphoreTake devolve pdFALSE e o
    // while roda de novo. Enquanto espera, a task fica BLOQUEADA -- nao gasta
    // CPU, que e a diferenca para um while vazio.
    if (xSemaphoreTake(xSemaphore_g, pdMS_TO_TICKS(500)) == pdTRUE) {
      gpio_put(LED_PIN_G, 1);
      vTaskDelay(pdMS_TO_TICKS(delay));
      gpio_put(LED_PIN_G, 0);
      vTaskDelay(pdMS_TO_TICKS(delay));
    }
  }
}

// BTN G: mesma leitura por pooling da btn_1_task.
void btn_2_task(void *p) {
  gpio_init(BTN_PIN_G);
  gpio_set_dir(BTN_PIN_G, GPIO_IN);
  gpio_pull_up(BTN_PIN_G);

  while (true) {
    if (!gpio_get(BTN_PIN_G)) {

      // Segura aqui ate SOLTAR o botao. O vTaskDelay(1) dentro do while e o que
      // impede essa espera de virar pooling cego: a cada volta a task devolve a
      // CPU para o escalonador por 1 tick.
      while (!gpio_get(BTN_PIN_G)) {
        vTaskDelay(pdMS_TO_TICKS(1));
      }
      xSemaphoreGive(xSemaphore_g);
    }
  }
}

int main() {
  stdio_init_all();
  printf("Start RTOS \n");

  xSemaphore_r = xSemaphoreCreateBinary();
  xSemaphore_g = xSemaphoreCreateBinary();

  xTaskCreate(led_1_task, "LED_Task 1", 256, NULL, 1, NULL);
  xTaskCreate(btn_1_task, "BTN_Task 1", 256, NULL, 1, NULL);
  xTaskCreate(led_2_task, "LED_Task 2", 256, NULL, 1, NULL);
  xTaskCreate(btn_2_task, "BTN_Task 2", 256, NULL, 1, NULL);

  vTaskStartScheduler();

  while (true)
    ;
}
