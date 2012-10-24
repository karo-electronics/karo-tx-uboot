#include <common.h>
#include <asm-generic/gpio.h>

int gpio_request_one(unsigned int gpio, enum gpio_flags flags,
		const char *label)
{
	int ret;

	ret = gpio_request(gpio, label);
	if (ret)
		return ret;

	if (flags == GPIOF_INPUT)
		gpio_direction_input(gpio);
	else if (flags == GPIOF_OUTPUT_INIT_LOW)
		gpio_direction_output(gpio, 0);
	else if (flags == GPIOF_OUTPUT_INIT_HIGH)
		gpio_direction_output(gpio, 1);

	return ret;
}

int gpio_request_array(const struct gpio *gpios, int count)
{
	int ret;
	int i;

	for (i = 0; i < count; i++) {
		ret = gpio_request_one(gpios[i].gpio, gpios[i].flags,
				gpios[i].label);
		if (ret) {
			printf("Failed to request GPIO%d (%u of %u): %d\n",
				gpios[i].gpio, i, count, ret);
			goto error;
		}
	}
	return 0;

error:
	while (--i >= 0)
		gpio_free(gpios[i].gpio);

	return ret;
}

int gpio_free_array(const struct gpio *gpios, int count)
{
	int ret = 0;
	int i;

	for (i = 0; i < count; i++)
		ret |= gpio_free(gpios[i].gpio);

	return ret;
}
