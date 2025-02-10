#include <stdio.h>        // Biblioteca padrão de entrada e saída
#include <string.h>
#include <stdlib.h>
#include "hardware/adc.h" // Biblioteca para manipulação do ADC no RP2040
#include "hardware/pwm.h" // Biblioteca para controle de PWM no RP2040
#include "pico/stdlib.h"  // Biblioteca padrão do Raspberry Pi Pico
#include "inc/ssd1306.h" // Biblioteca para o display OLED

// Definição dos pinos usados para o joystick e LEDs
const int VRX = 26;          // Pino de leitura do eixo X do joystick (conectado ao ADC)
const int VRY = 27;          // Pino de leitura do eixo Y do joystick (conectado ao ADC)
const int ADC_CHANNEL_0 = 0; // Canal ADC para o eixo X do joystick
const int ADC_CHANNEL_1 = 1; // Canal ADC para o eixo Y do joystick
const int SW = 22;           // Pino de leitura do botão do joystick

const int I2C_SDA = 14;
const int I2C_SCL = 15;
const int MAX_CHARS_PER_LINE 21;
const int LINE_HEIGHT 8;

const int LED_B = 13;                    // Pino para controle do LED azul via PWM
const int LED_R = 11;                    // Pino para controle do LED vermelho via PWM
const float DIVIDER_PWM = 16.0;          // Divisor fracional do clock para o PWM
const uint16_t PERIOD = 4096;            // Período do PWM (valor máximo do contador)
uint16_t led_b_level, led_r_level = 100; // Inicialização dos níveis de PWM para os LEDs
uint slice_led_b, slice_led_r;           // Variáveis para armazenar os slices de PWM correspondentes aos LEDs

uint8_t ssd[ssd1306_buffer_length];

struct render_area frame_area = {
    start_column : 0,
    end_column : ssd1306_width - 1,
    start_page : 0,
    end_page : ssd1306_n_pages - 1
};

void ClearDisplay() {
  memset(ssd, 0, ssd1306_buffer_length);
  render_on_display(ssd, &frame_area);
}

void OledRenderString(uint8_t *ssd, int16_t x, int16_t y, char *string) {
    ClearDisplay();
  
    int current_line = 0;
    int str_len = strlen(string);
    int pos = 0;
  
    while (pos < str_len) {
      int chars_to_copy = str_len - pos;
      if (chars_to_copy > MAX_CHARS_PER_LINE) {
        chars_to_copy = MAX_CHARS_PER_LINE;
        for (int i = chars_to_copy - 1; i >= 0; i--) {
          if (string[pos + i] == ' ') {
            chars_to_copy = i;
            break;
          }
        }
        // Se não encontrar espaço, desenha a linha completa mesmo assim
        if (chars_to_copy == 0) chars_to_copy = MAX_CHARS_PER_LINE;
      }
  
      // Cria um buffer local para a linha atual
      char line_buffer[MAX_CHARS_PER_LINE + 1];
      strncpy(line_buffer, string + pos, chars_to_copy);
      line_buffer[chars_to_copy] = '\0'; // Null terminate é crucial
  
      ssd1306_draw_string(ssd, x, y + (current_line * LINE_HEIGHT), line_buffer);
  
      pos += chars_to_copy;
      while (pos < str_len && string[pos] == ' ') {
        pos++;
      }
      current_line++;
    }
  
    render_on_display(ssd, &frame_area);
  }

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

void setup_oledscreen(){
        // Inicialização do i2c
        i2c_init(i2c1, ssd1306_i2c_clock * 1000);
        gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
        gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
        gpio_pull_up(I2C_SDA);
        gpio_pull_up(I2C_SCL);
    
        // Processo de inicialização completo do OLED SSD1306
        ssd1306_init();
        calculate_render_area_buffer_length(&frame_area);
        ClearDisplay();
}

// Função de configuração geral
void setup()
{
  stdio_init_all();                                // Inicializa a porta serial para saída de dados
  setup_joystick();                                // Chama a função de configuração do joystick
  setup_pwm_led(LED_B, &slice_led_b, led_b_level); // Configura o PWM para o LED azul
  setup_pwm_led(LED_R, &slice_led_r, led_r_level); // Configura o PWM para o LED vermelho
  setup_oledscreen()                               // Configuração do i2c e tela
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

// ##NÃO ESQUEÇA DE TIRAR## OledRenderString(ssd, 0, 0, "SINAL ABERTO - ATRAVESSAR COM CUIDADO");
void menu_op1() {
  while (!gpio_get(SW)) // Enquanto o Joystick estiver solto
  {
    joystick_read_axis(&vrx_value, &vry_value); // Lê os valores dos eixos do joystick
    // Ajusta os níveis PWM dos LEDs de acordo com os valores do joystick
    pwm_set_gpio_level(LED_B, vrx_value); // Ajusta o brilho do LED azul com o valor do eixo X
    pwm_set_gpio_level(LED_R, vry_value); // Ajusta o brilho do LED vermelho com o valor do eixo Y

    // Pequeno delay antes da próxima leitura
    sleep_ms(100); // Espera 100 ms antes de repetir o ciclo
  }
  return;
}

int main(){
    uint16_t vrx_value, vry_value, sw_value; // Variáveis para armazenar os valores do joystick (eixos X e Y) e botão
    setup();                                 // Chama a função de configuração
    printf("Joystick-PWM\n");                // Exibe uma mensagem inicial via porta serial

    unsigned int menu_number = 1;
    // Loop principal
    while(1){
        // Leitura do valor do eixo Y do joystick
        adc_select_input(ADC_CHANNEL_1); // Seleciona o canal ADC para o eixo Y
        sleep_us(2);                     // Pequeno delay para estabilidade

        if(adc_read() > 2000){           // Lê o valor do eixo Y (0-4095)
            if(menu_number == 3){
                menu_number = 1;
            } else{
                menu_number += 1;
            }

        } else{
            if(menu_number == 1) menu_number = 3;
            else menu_number -= 1;
        }

          sleep_ms(200) // para evitar debounce
    }
}
