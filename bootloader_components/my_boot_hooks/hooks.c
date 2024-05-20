#include "esp_log.h"
#include "hal/gpio_hal.h"
#include "hal/spi_hal.h"
#include "../../main/pins.h"

/* Function used to tell the linker to include this file
 * with all its symbols.
 */
void bootloader_hooks_include(void){
}

void init_pin(uint32_t pin_num) {
    // gpio_ll
//    gpio_hal_iomux_func_sel(pin_num, PIN_FUNC_GPIO);
    gpio_ll_output_enable(&GPIO, pin_num);
    gpio_ll_input_disable(&GPIO, pin_num);
    gpio_ll_pullup_dis(&GPIO, pin_num);
    gpio_ll_pulldown_dis(&GPIO, pin_num);
    gpio_ll_set_level(&GPIO, pin_num, 0);
}

void bootloader_before_init(void) {
    init_pin(M1_FWD);
    init_pin(M1_REV);
    init_pin(M2_FWD);
    init_pin(M2_REV);
    init_pin(M3_FWD);
    init_pin(M3_REV);
}

void bootloader_after_init(void) {

}