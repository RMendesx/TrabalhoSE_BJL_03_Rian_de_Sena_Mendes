#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/pio.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "lib/buzzer.h"
#include "lib/matriz_led.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include <stdio.h>

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

#define LED_VERDE 11
#define LED_VERMELHO 13
#define BUZZER_A 10
#define BUZZER_B 21
#define BOTAO_A 5
#define BOTAO_B 6

// Trecho para modo BOOTSEL com botão B
void gpio_irq_handler(uint gpio, uint32_t events)
{
    reset_usb_boot(0, 0);
}

int modoAtual = 0;
int estadoSemaforo = 0;

// Tarefa: alterna entre modo normal e noturno
void vTaskBotao(void *param)
{
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);

    while (true)
    {
        if (gpio_get(BOTAO_A) == 0) // Pressionado (nível baixo)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            if (gpio_get(BOTAO_A) == 0)
            {
                // Alterna o modo
                if (modoAtual == 1)
                {
                    modoAtual = 0; // Modo Normal
                    printf("Modo Normal\n");
                }
                else
                {
                    modoAtual = 1; // Modo Noturno
                    printf("Modo Noturno\n");
                }

                // Espera soltar o botão antes de continuar
                while (gpio_get(BOTAO_A) == 0)
                {
                    vTaskDelay(pdMS_TO_TICKS(20));
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// Tarefa: altera o Display OLED
void vDisplayTask(void *param)
{
    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA);                                        // Pull up the data line
    gpio_pull_up(I2C_SCL);                                        // Pull up the clock line
    ssd1306_t ssd;                                                // Inicializa a estrutura do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd);                                         // Configura o display
    ssd1306_send_data(&ssd);                                      // Envia os dados para o display
    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    bool cor = true;

    while (true)
    {
        if (modoAtual == 0)
        {
            ssd1306_fill(&ssd, !cor);                         // Limpa o display
            ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);     // Desenha um retângulo
            ssd1306_line(&ssd, 3, 15, 123, 15, cor);          // Desenha uma linha
            ssd1306_line(&ssd, 3, 27, 123, 27, cor);          // Desenha uma linha
            ssd1306_draw_string(&ssd, "FreeRTOS", 33, 6);     // Desenha uma string
            ssd1306_draw_string(&ssd, "Modo NORMAL", 23, 17); // Desenha uma string
            if (estadoSemaforo == 0)
            {
                ssd1306_draw_string(&ssd, "VERDE", 45, 36); // Desenha uma string
                ssd1306_draw_string(&ssd, "SIGA", 49, 44);  // Desenha uma string
            }
            else if (estadoSemaforo == 1)
            {
                ssd1306_draw_string(&ssd, "AMARELO", 38, 36); // Desenha uma string
                ssd1306_draw_string(&ssd, "ATENCAO", 38, 44); // Desenha uma string
            }
            else
            {
                ssd1306_draw_string(&ssd, "VERMELHO", 35, 36); // Desenha uma string
                ssd1306_draw_string(&ssd, "PARE", 51, 44);     // Desenha uma string
            }

            ssd1306_send_data(&ssd); // Atualiza o display
            sleep_ms(1);
        }
        else
        {
            ssd1306_fill(&ssd, !cor);                           // Limpa o display
            ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);       // Desenha um retângulo
            ssd1306_line(&ssd, 3, 15, 123, 15, cor);            // Desenha uma linha
            ssd1306_line(&ssd, 3, 27, 123, 27, cor);            // Desenha uma linha
            ssd1306_draw_string(&ssd, "FreeRTOS", 33, 6);       // Desenha uma string
            ssd1306_draw_string(&ssd, "Modo NOTURNO", 18, 17);  // Desenha uma string
            ssd1306_draw_string(&ssd, "AMARELO", 38, 36);       // Desenha uma string
            ssd1306_draw_string(&ssd, "MUITA ATENCAO", 12, 44); // Desenha uma string
            ssd1306_send_data(&ssd);                            // Atualiza o display
            sleep_ms(1);
        }
    }
}

// Tarefa: gera sons para acessibilidade
void vTaskBuzzer(void *param)
{
    vTaskDelay(pdMS_TO_TICKS(200)); // Tempo para estabilizar o sistema

    gpio_init(BUZZER_A);
    gpio_init(BUZZER_B);
    gpio_set_dir(BUZZER_A, GPIO_OUT);
    gpio_set_dir(BUZZER_B, GPIO_OUT);

    while (true)
    {
        if (modoAtual == 0) // Modo normal
        {
            switch (estadoSemaforo)
            {
            case 0: // Verde
                buzzer_on(BUZZER_A, 4000);
                buzzer_on(BUZZER_B, 4000);
                vTaskDelay(pdMS_TO_TICKS(1000));
                buzzer_off(BUZZER_A);
                buzzer_off(BUZZER_B);
                vTaskDelay(pdMS_TO_TICKS(250));
                break;

            case 1: // Amarelo
                buzzer_on(BUZZER_A, 3000);
                buzzer_on(BUZZER_B, 3000);
                vTaskDelay(pdMS_TO_TICKS(100));
                buzzer_off(BUZZER_A);
                buzzer_off(BUZZER_B);
                vTaskDelay(pdMS_TO_TICKS(300));
                break;

            case 2: // Vermelho
                buzzer_on(BUZZER_A, 5000);
                buzzer_on(BUZZER_B, 5000);
                vTaskDelay(pdMS_TO_TICKS(500));
                buzzer_off(BUZZER_A);
                buzzer_off(BUZZER_B);
                vTaskDelay(pdMS_TO_TICKS(1500));
                break;
            }
        }
        else // Modo noturno
        {
            buzzer_on(BUZZER_A, 2000); // Tom mais baixo
            buzzer_on(BUZZER_B, 2000);
            vTaskDelay(pdMS_TO_TICKS(100));
            buzzer_off(BUZZER_A);
            buzzer_off(BUZZER_B);
            vTaskDelay(pdMS_TO_TICKS(1900));
        }
    }
}

// Tarefa: controla LEDs do semáforo
void delayComVerificacao(int total_ms)
{
    int elapsed = 0;
    while (elapsed < total_ms)
    {
        if (modoAtual != 0)
            return; // Sai do delay se modo mudar
        vTaskDelay(pdMS_TO_TICKS(100));
        elapsed += 100;
    }
}

void vTaskSemaforo(void *param)
{
    gpio_init(LED_VERDE);
    gpio_init(LED_VERMELHO);
    gpio_set_dir(LED_VERDE, GPIO_OUT);
    gpio_set_dir(LED_VERMELHO, GPIO_OUT);

    while (true)
    {
        if (modoAtual == 0)
        {
            estadoSemaforo = 0;
            gpio_put(LED_VERDE, 1);
            gpio_put(LED_VERMELHO, 0);
            delayComVerificacao(5000);
            if (modoAtual != 0)
                continue;

            estadoSemaforo = 1;
            gpio_put(LED_VERDE, 1);
            gpio_put(LED_VERMELHO, 1);
            delayComVerificacao(3000);
            if (modoAtual != 0)
                continue;

            estadoSemaforo = 2;
            gpio_put(LED_VERDE, 0);
            gpio_put(LED_VERMELHO, 1);
            delayComVerificacao(4000);
        }
        else
        {
            gpio_put(LED_VERMELHO, 1);
            gpio_put(LED_VERDE, 1);
            vTaskDelay(pdMS_TO_TICKS(500));

            gpio_put(LED_VERMELHO, 0);
            gpio_put(LED_VERDE, 0);
            vTaskDelay(pdMS_TO_TICKS(1500));
        }
    }
}

// Tarefa: controla a matriz de LEDs
void vTaskMatriz(void *param)
{
    npInit(LED_PIN); // Inicializa a matriz de LEDs
    int estadoAnterior = -1;
    int modoAnterior = -1;

    while (true)
    {
        if (modoAtual != modoAnterior)
        {
            modoAnterior = modoAtual;
            estadoAnterior = -1; // Força a atualização
        }

        if (modoAtual == 0)
        {
            if (estadoSemaforo != estadoAnterior)
            {
                estadoAnterior = estadoSemaforo;
                switch (estadoSemaforo)
                {
                case 0:
                    printColor(0, 255, 0); // Verde
                    break;
                case 1:
                    printColor(255, 255, 0); // Amarelo
                    break;
                case 2:
                    printColor(255, 0, 0); // Vermelho
                    break;
                }
            }
        }
        else
        {
            printColor(255, 255, 0); // Amarelo no modo noturno
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // Verifica a cada 100 ms
    }
}

int main()
{
    // Para ser utilizado o modo BOOTSEL com botão B
    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);
    gpio_set_irq_enabled_with_callback(BOTAO_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    // Fim do trecho para modo BOOTSEL com botão B

    stdio_init_all();

    xTaskCreate(vTaskBotao, "Botao Task", 256, NULL, 1, NULL);
    xTaskCreate(vTaskSemaforo, "Semaforo Task", 256, NULL, 1, NULL);
    xTaskCreate(vTaskBuzzer, "Buzzer Task", 256, NULL, 1, NULL);
    xTaskCreate(vDisplayTask, "Display Task", 256, NULL, 1, NULL);
    xTaskCreate(vTaskMatriz, "Matriz Task", 256, NULL, 1, NULL);
    vTaskStartScheduler();
    panic_unsupported();
}