
#include "syscfg/syscfg.h"

#if MYNEWT_VAL(BLE_MESH_SHELL_MODELS)

#include "mesh/mesh.h"
#include "bsp.h"
#include "pwm/pwm.h"
#include "light_model.h"
#include "ws2812.h"

#if (!MYNEWT_VAL(USE_NEOPIXEL))
#if MYNEWT_VAL(PWM_0)
struct pwm_dev *pwm0;
#endif
#if MYNEWT_VAL(PWM_1)
struct pwm_dev *pwm1;
#endif
#if MYNEWT_VAL(PWM_2)
struct pwm_dev *pwm2;
#endif
#if MYNEWT_VAL(PWM_3)
struct pwm_dev *pwm3;
#endif

static uint16_t top_val;
#endif

#if (MYNEWT_VAL(USE_NEOPIXEL))
static uint32_t neopixel[WS2812_NUM_LED];
#endif

static u8_t gen_onoff_state;
static s16_t gen_level_state;

static void light_set_lightness(u8_t percentage)
{
#if (!MYNEWT_VAL(USE_NEOPIXEL))
	int rc;

	uint16_t pwm_val = (uint16_t) (percentage * top_val / 100);

#if MYNEWT_VAL(PWM_0)
	rc = pwm_enable_duty_cycle(pwm0, 0, pwm_val);
	assert(rc == 0);
#endif
#if MYNEWT_VAL(PWM_1)
	rc = pwm_enable_duty_cycle(pwm1, 0, pwm_val);
	assert(rc == 0);
#endif
#if MYNEWT_VAL(PWM_2)
	rc = pwm_enable_duty_cycle(pwm2, 0, pwm_val);
	assert(rc == 0);
#endif
#if MYNEWT_VAL(PWM_3)
	rc = pwm_enable_duty_cycle(pwm3, 0, pwm_val);
	assert(rc == 0);
#endif
#else
	int i;
	u32_t lightness;
	u8_t max_lightness = 0x1f;

	lightness = (u8_t) (percentage * max_lightness / 100);

	for (i = 0; i < WS2812_NUM_LED; i++) {
		neopixel[i] = (lightness | lightness << 8 | lightness << 16);
	}
	ws2812_write(neopixel);
#endif
}

static void update_light_state(void)
{
	u16_t level = (u16_t)gen_level_state;
	int percent = 100 * level / 0xffff;

	if (gen_onoff_state == 0) {
		percent = 0;
	}
	light_set_lightness((uint8_t) percent);
}

int light_model_gen_onoff_get(struct bt_mesh_model *model, u8_t *state)
{
	*state = gen_onoff_state;
	return 0;
}

int light_model_gen_onoff_set(struct bt_mesh_model *model, u8_t state)
{
	gen_onoff_state = state;
	update_light_state();
	return 0;
}

int light_model_gen_level_get(struct bt_mesh_model *model, s16_t *level)
{
	*level = gen_level_state;
	return 0;
}

int light_model_gen_level_set(struct bt_mesh_model *model, s16_t level)
{
	gen_level_state = level;
	if ((u16_t)gen_level_state > 0x0000) {
		gen_onoff_state = 1;
	}
	if ((u16_t)gen_level_state == 0x0000) {
		gen_onoff_state = 0;
	}
	update_light_state();
	return 0;
}

int light_model_light_lightness_get(struct bt_mesh_model *model, s16_t *lightness)
{
	return light_model_gen_level_get(model, lightness);
}

int light_model_light_lightness_set(struct bt_mesh_model *model, s16_t lightness)
{
	return light_model_gen_level_set(model, lightness);
}

#if (!MYNEWT_VAL(USE_NEOPIXEL))
#if MYNEWT_VAL(PWM_0)
static struct pwm_dev_interrupt_cfg led1_conf = {
	.cfg = {
		.pin = LED_1,
		.inverted = true,
		.n_cycles = 0,
		.interrupts_cfg = true,
	},
	.int_prio = 3,
};
#endif

#if MYNEWT_VAL(PWM_1)
static struct pwm_dev_interrupt_cfg led2_conf = {
	.cfg = {
		.pin = LED_2,
		.inverted = true,
		.n_cycles = 0,
		.interrupts_cfg = true,
	},
	.int_prio = 3,
};
#endif

#if MYNEWT_VAL(PWM_2)
static struct pwm_dev_interrupt_cfg led3_conf = {
	.cfg = {
		.pin = LED_3,
		.inverted = true,
		.n_cycles = 0,
		.interrupts_cfg = true,
	},
	.int_prio = 3,
};
#endif
#endif

#if MYNEWT_VAL(PWM_3)
static struct pwm_dev_interrupt_cfg led4_conf = {
	.cfg = {
		.pin = LED_4,
		.inverted = true,
		.n_cycles = 0,
		.interrupts_cfg = true,
	},
	.int_prio = 3,
};
#endif

#if (!MYNEWT_VAL(USE_NEOPIXEL))
int pwm_init(void)
{
	int rc = 0;

#if MYNEWT_VAL(PWM_0)
	led1_conf.seq_end_data = &led1_conf;
	pwm0 = (struct pwm_dev *) os_dev_open("pwm0", 0, NULL);
	assert(pwm0);
	pwm_set_frequency(pwm0, 1000);
	rc = pwm_chan_config(pwm0, 0, (struct pwm_chan_cfg*) &led1_conf);
	assert(rc == 0);
#endif

#if MYNEWT_VAL(PWM_1)
	led2_conf.seq_end_data = &led2_conf;
	pwm1 = (struct pwm_dev *) os_dev_open("pwm1", 0, NULL);
	assert(pwm1);
	pwm_set_frequency(pwm1, 1000);
	rc = pwm_chan_config(pwm1, 0, (struct pwm_chan_cfg*) &led2_conf);
	assert(rc == 0);
#endif

#if MYNEWT_VAL(PWM_2)
	led3_conf.seq_end_data = &led3_conf;
	pwm2 = (struct pwm_dev *) os_dev_open("pwm2", 0, NULL);
	assert(pwm2);
	pwm_set_frequency(pwm2, 1000);
	rc = pwm_chan_config(pwm2, 0, (struct pwm_chan_cfg*) &led3_conf);
	assert(rc == 0);
#endif

#if MYNEWT_VAL(PWM_3)
	led4_conf.seq_end_data = &led4_conf;
	pwm3 = (struct pwm_dev *) os_dev_open("pwm3", 0, NULL);
	assert(pwm3);
	pwm_set_frequency(pwm3, 1000);
	rc = pwm_chan_config(pwm3, 0, (struct pwm_chan_cfg*) &led4_conf);
	assert(rc == 0);
#endif

	if (!pwm0) {
		return 0;
	}

	top_val = (uint16_t) pwm_get_top_value(pwm0);
	update_light_state();

	return rc;
}
#endif
#endif

int light_model_init(void)
{
#if MYNEWT_VAL(BLE_MESH_SHELL_MODELS)
	int rc;
#if (!MYNEWT_VAL(USE_NEOPIXEL))
	rc = pwm_init();
	assert(rc == 0);
#else
	rc = ws2812_init();
	assert(rc == 0);
	update_light_state();
#endif
	return rc;
#else
	return 0;
#endif
}

