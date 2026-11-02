#pragma once
#define GPIO_IS_VALID_OUTPUT_GPIO(pin) ((pin)>=0 && (pin)<49)
#define GPIO_IS_VALID_GPIO(pin) GPIO_IS_VALID_OUTPUT_GPIO(pin)
