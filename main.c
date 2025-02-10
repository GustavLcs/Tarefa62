#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "inc/ssd1306.h"
#include "inc.h"

#define DISPLAY_I2C_SDA 14
#define DISPLAY_I2C_SCL 15
#define DISPLAY_MAX_CHARS_PER_LINE 21
#define DISPLAY_LINE_HEIGHT 8

uint8_t ssd[ssd1306_buffer_length];

struct render_area frame_area = {
  start_column : 0,
  end_column : ssd1306_width - 1,
  start_page : 0,
  end_page : ssd1306_n_pages - 1
};

void display_ini();
void display_clr();
void display_str(char *string, int16_t x, int16_t y);
void display_put();

int map_value(int value, int in_min, int in_max, int out_min, int out_max) {
  return (value - in_min) * (out_max - out_min); // (in_max - in_min) + out_min
}

volatile bool button_pressed = false;

void joystick_button_irq_handler(uint gpio, uint32_t events) {
  if (gpio == 22 && (events & GPIO_IRQ_EDGE_FALL)) { // Detecta borda de descida (botão pressionado)
      button_pressed = true;
  }
}

int main() {
  stdio_init_all();
  adc_init();
  
  display_ini();
  
  adc_gpio_init(26);
  adc_gpio_init(27);
  
  adc_init();

  gpio_init(22);
  gpio_set_dir(22, GPIO_IN);
  gpio_pull_up(22);

  int option = 0;

  gpio_set_irq_enabled_with_callback(22, GPIO_IRQ_EDGE_FALL, true, &joystick_button_irq_handler); // Interrupção na borda de descida
  gpio_pull_up(22);

  while (1) {
    adc_select_input(1);

    sleep_us(2);
    int joystickY = map_value(adc_read() - 400, 65278, 240, 0, 2);

    if (joystickY == 0) {
      --option;
    } else if (joystickY == 2) {
      ++option;
    }

    if (option < 0) {
      option = 2;
    }

    if (option > 2) {
      option = 0;
    }

    display_clr();

    switch (option) {
      case 0:
        display_str("O JOYSTICK", 0, 0 * DISPLAY_LINE_HEIGHT);
        display_str("  BUZZER", 0, 1 * DISPLAY_LINE_HEIGHT);
        display_str("  LED", 0, 2 * DISPLAY_LINE_HEIGHT);
        break;
      case 1:
        display_str("  JOYSTICK", 0, 0 * DISPLAY_LINE_HEIGHT);
        display_str("O BUZZER", 0, 1 * DISPLAY_LINE_HEIGHT);
        display_str("  LED", 0, 2 * DISPLAY_LINE_HEIGHT);
        break;
      case 2:
        display_str("  JOYSTICK", 0, 0 * DISPLAY_LINE_HEIGHT);
        display_str("  BUZZER", 0, 1 * DISPLAY_LINE_HEIGHT);
        display_str("O LED", 0, 2 * DISPLAY_LINE_HEIGHT);
        break;
    }

    if (button_pressed) {
      
    }

    display_put();

    sleep_ms(100);
  }

  return 0;
}

void display_ini() {
  i2c_init(i2c1, ssd1306_i2c_clock * 1000);
  gpio_set_function(DISPLAY_I2C_SDA, GPIO_FUNC_I2C);
  gpio_set_function(DISPLAY_I2C_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(DISPLAY_I2C_SCL);

  ssd1306_init();
  calculate_render_area_buffer_length(&frame_area);
  display_clr();
}

void display_clr() {
  memset(ssd, 0, ssd1306_buffer_length);
}

void display_str(char *string, int16_t x, int16_t y) {
  int current_line = 0;
  int str_len = strlen(string);
  int pos = 0;

  while (pos < str_len) {
    int chars_to_copy = str_len - pos;
    if (chars_to_copy > DISPLAY_MAX_CHARS_PER_LINE) {
      chars_to_copy = DISPLAY_MAX_CHARS_PER_LINE;
      for (int i = chars_to_copy - 1; i >= 0; i--) {
        if (string[pos + i] == ' ') {
          chars_to_copy = i;
          break;
        }
      }
      // Se não encontrar espaço, desenha a linha completa mesmo assim
      if (chars_to_copy == 0) chars_to_copy = DISPLAY_MAX_CHARS_PER_LINE;
    }

    // Cria um buffer local para a linha atual
    char line_buffer[DISPLAY_MAX_CHARS_PER_LINE + 1];
    strncpy(line_buffer, string + pos, chars_to_copy);
    line_buffer[chars_to_copy] = '\0'; // Null terminate é crucial

    ssd1306_draw_string(ssd, x, y + (current_line * DISPLAY_LINE_HEIGHT), line_buffer);

    pos += chars_to_copy;
    while (pos < str_len && string[pos] == ' ') {
      pos++;
    }
    current_line++;
  }
}

void display_put() {
  render_on_display(ssd, &frame_area);
}

#include <stdio.h>        // Biblioteca padrão de entrada e saída
#include "hardware/adc.h" // Biblioteca para manipulação do ADC no RP2040
#include "hardware/pwm.h" // Biblioteca para controle de PWM no RP2040
#include "pico/stdlib.h"  // Biblioteca padrão do Raspberry Pi Pico

// Definição dos pinos usados para o joystick e LEDs
const int VRX = 26;          // Pino de leitura do eixo X do joystick (conectado ao ADC)
const int VRY = 27;          // Pino de leitura do eixo Y do joystick (conectado ao ADC)
const int ADC_CHANNEL_0 = 0; // Canal ADC para o eixo X do joystick
const int ADC_CHANNEL_1 = 1; // Canal ADC para o eixo Y do joystick
const int SW = 22;           // Pino de leitura do botão do joystick

const int LED_B = 13;                    // Pino para controle do LED azul via PWM
const int LED_R = 11;                    // Pino para controle do LED vermelho via PWM
const float DIVIDER_PWM = 16.0;          // Divisor fracional do clock para o PWM
const uint16_t PERIOD = 4096;            // Período do PWM (valor máximo do contador)
uint16_t led_b_level, led_r_level = 100; // Inicialização dos níveis de PWM para os LEDs
uint slice_led_b, slice_led_r;           // Variáveis para armazenar os slices de PWM correspondentes aos LEDs

// Função para configurar o joystick (pinos de leitura e ADC)
void setup_joystick()
{
  // Inicializa o ADC e os pinos de entrada analógica
  adc_init();         // Inicializa o módulo ADC
  adc_gpio_init(VRX); // Configura o pino VRX (eixo X) para entrada ADC
  adc_gpio_init(VRY); // Configura o pino VRY (eixo Y) para entrada ADC

  // Inicializa o pino do botão do joystick
  gpio_init(SW);             // Inicializa o pino do botão
  gpio_set_dir(SW, GPIO_IN); // Configura o pino do botão como entrada
  gpio_pull_up(SW);          // Ativa o pull-up no pino do botão para evitar flutuações
}

// Função para configurar o PWM de um LED (genérica para azul e vermelho)
void setup_pwm_led(uint led, uint *slice, uint16_t level)
{
  gpio_set_function(led, GPIO_FUNC_PWM); // Configura o pino do LED como saída PWM
  *slice = pwm_gpio_to_slice_num(led);   // Obtém o slice do PWM associado ao pino do LED
  pwm_set_clkdiv(*slice, DIVIDER_PWM);   // Define o divisor de clock do PWM
  pwm_set_wrap(*slice, PERIOD);          // Configura o valor máximo do contador (período do PWM)
  pwm_set_gpio_level(led, level);        // Define o nível inicial do PWM para o LED
  pwm_set_enabled(*slice, true);         // Habilita o PWM no slice correspondente ao LED
}

// Função de configuração geral
void setup()
{
  setup_joystick();                                // Chama a função de configuração do joystick
  setup_pwm_led(LED_B, &slice_led_b, led_b_level); // Configura o PWM para o LED azul
  setup_pwm_led(LED_R, &slice_led_r, led_r_level); // Configura o PWM para o LED vermelho
}

// Função para ler os valores dos eixos do joystick (X e Y)
void joystick_read_axis(uint16_t *vrx_value, uint16_t *vry_value)
{
  // Leitura do valor do eixo X do joystick
  adc_select_input(ADC_CHANNEL_0); // Seleciona o canal ADC para o eixo X
  sleep_us(2);                     // Pequeno delay para estabilidade
  *vrx_value = adc_read();         // Lê o valor do eixo X (0-4095)

  // Leitura do valor do eixo Y do joystick
  adc_select_input(ADC_CHANNEL_1); // Seleciona o canal ADC para o eixo Y
  sleep_us(2);                     // Pequeno delay para estabilidade
  *vry_value = adc_read();         // Lê o valor do eixo Y (0-4095)
}

// Função principal
void joystick()
{
  uint16_t vrx_value, vry_value, sw_value; // Variáveis para armazenar os valores do joystick (eixos X e Y) e botão
  setup();                                 // Chama a função de configuração
  printf("Joystick-PWM\n");                // Exibe uma mensagem inicial via porta serial
  // Loop principal
  while (1)
  {
    if (button_pressed) break;
    joystick_read_axis(&vrx_value, &vry_value); // Lê os valores dos eixos do joystick
    // Ajusta os níveis PWM dos LEDs de acordo com os valores do joystick
    pwm_set_gpio_level(LED_B, vrx_value); // Ajusta o brilho do LED azul com o valor do eixo X
    pwm_set_gpio_level(LED_R, vry_value); // Ajusta o brilho do LED vermelho com o valor do eixo Y

    // Pequeno delay antes da próxima leitura
    sleep_ms(100); // Espera 100 ms antes de repetir o ciclo
  }
}

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"

const uint LED = 12;            // Pino do LED conectado
const uint16_t PERIOD = 2000;   // Período do PWM (valor máximo do contador)
const float DIVIDER_PWM = 16.0; // Divisor fracional do clock para o PWM
const uint16_t LED_STEP = 100;  // Passo de incremento/decremento para o duty cycle do LED
uint16_t led_level = 100;       // Nível inicial do PWM (duty cycle)

void setup_pwm()
{
    uint slice;
    gpio_set_function(LED, GPIO_FUNC_PWM); // Configura o pino do LED para função PWM
    slice = pwm_gpio_to_slice_num(LED);    // Obtém o slice do PWM associado ao pino do LED
    pwm_set_clkdiv(slice, DIVIDER_PWM);    // Define o divisor de clock do PWM
    pwm_set_wrap(slice, PERIOD);           // Configura o valor máximo do contador (período do PWM)
    pwm_set_gpio_level(LED, led_level);    // Define o nível inicial do PWM para o pino do LED
    pwm_set_enabled(slice, true);          // Habilita o PWM no slice correspondente
}

void led()
{
    uint up_down = 1; // Variável para controlar se o nível do LED aumenta ou diminui
    setup_pwm();      // Configura o PWM
    while (1)
    {
      if (button_pressed) break;
        pwm_set_gpio_level(LED, led_level); // Define o nível atual do PWM (duty cycle)
        sleep_ms(1000);                     // Atraso de 1 segundo
        if (up_down)
        {
            led_level += LED_STEP; // Incrementa o nível do LED
            if (led_level >= PERIOD)
                up_down = 0; // Muda direção para diminuir quando atingir o período máximo
        }
        else
        {
            led_level -= LED_STEP; // Decrementa o nível do LED
            if (led_level <= LED_STEP)
                up_down = 1; // Muda direção para aumentar quando atingir o mínimo
        }
    }
}

/**
 * Exemplo de acionamento do buzzer utilizando sinal PWM no GPIO 21 da Raspberry Pico / BitDogLab
 * 06/12/2024
 */

 #include <stdio.h>
 #include "pico/stdlib.h"
 #include "hardware/pwm.h"
 #include "hardware/clocks.h"
 
 // Configuração do pino do buzzer
 #define BUZZER_PIN 21
 
 // Notas musicais para a música tema de Star Wars
 const uint star_wars_notes[] = {
     330, 330, 330, 262, 392, 523, 330, 262,
     392, 523, 330, 659, 659, 659, 698, 523,
     415, 349, 330, 262, 392, 523, 330, 262,
     392, 523, 330, 659, 659, 659, 698, 523,
     415, 349, 330, 523, 494, 440, 392, 330,
     659, 784, 659, 523, 494, 440, 392, 330,
     659, 659, 330, 784, 880, 698, 784, 659,
     523, 494, 440, 392, 659, 784, 659, 523,
     494, 440, 392, 330, 659, 523, 659, 262,
     330, 294, 247, 262, 220, 262, 330, 262,
     330, 294, 247, 262, 330, 392, 523, 440,
     349, 330, 659, 784, 659, 523, 494, 440,
     392, 659, 784, 659, 523, 494, 440, 392
 };
 
 // Duração das notas em milissegundos
 const uint note_duration[] = {
     500, 500, 500, 350, 150, 300, 500, 350,
     150, 300, 500, 500, 500, 500, 350, 150,
     300, 500, 500, 350, 150, 300, 500, 350,
     150, 300, 650, 500, 150, 300, 500, 350,
     150, 300, 500, 150, 300, 500, 350, 150,
     300, 650, 500, 350, 150, 300, 500, 350,
     150, 300, 500, 500, 500, 500, 350, 150,
     300, 500, 500, 350, 150, 300, 500, 350,
     150, 300, 500, 350, 150, 300, 500, 500,
     350, 150, 300, 500, 500, 350, 150, 300,
 };
 
 // Inicializa o PWM no pino do buzzer
 void pwm_init_buzzer(uint pin) {
     gpio_set_function(pin, GPIO_FUNC_PWM);
     uint slice_num = pwm_gpio_to_slice_num(pin);
     pwm_config config = pwm_get_default_config();
     pwm_config_set_clkdiv(&config, 4.0f); // Ajusta divisor de clock
     pwm_init(slice_num, &config, true);
     pwm_set_gpio_level(pin, 0); // Desliga o PWM inicialmente
 }
 
 // Toca uma nota com a frequência e duração especificadas
 void play_tone(uint pin, uint frequency, uint duration_ms) {
     uint slice_num = pwm_gpio_to_slice_num(pin);
     uint32_t clock_freq = clock_get_hz(clk_sys);
     uint32_t top = clock_freq / frequency - 1;
 
     pwm_set_wrap(slice_num, top);
     pwm_set_gpio_level(pin, top / 2); // 50% de duty cycle
 
     sleep_ms(duration_ms);
 
     pwm_set_gpio_level(pin, 0); // Desliga o som após a duração
     sleep_ms(50); // Pausa entre notas
 }
 
 // Função principal para tocar a música
 void play_star_wars(uint pin) {
     for (int i = 0; i < sizeof(star_wars_notes) / sizeof(star_wars_notes[0]); i++) {
         if (star_wars_notes[i] == 0) {
             sleep_ms(note_duration[i]);
         } else {
             play_tone(pin, star_wars_notes[i], note_duration[i]);
         }
     }
 }

 void buzzer() {
  pwm_init_buzzer(BUZZER_PIN);
  while(1){
    if (button_pressed) break;
    play_star_wars(BUZZER_PIN);
   
  }
 
  return 0;
}