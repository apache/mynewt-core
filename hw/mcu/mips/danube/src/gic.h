#ifndef H_GIC_H_
#define H_GIC_H_

#include <stdint.h>

void gic_interrupt_set(uint32_t n);
void gic_interrupt_reset(uint32_t n);
void gic_interrupt_active_high(uint32_t n);
void gic_interrupt_active_low(uint32_t n);
int gic_interrupt_is_enabled(uint32_t n);
int gic_interrupt_poll(uint32_t n);
void gic_map(int int_no, uint8_t vpe, uint8_t pin);
void gic_unmap(int int_no, uint8_t pin);
int gic_init(void);

#endif
