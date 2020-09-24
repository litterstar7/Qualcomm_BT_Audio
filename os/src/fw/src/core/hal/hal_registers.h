/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file
 * Register access macros used by HAL macros
 *
 * \section hal_registers_h_usage USAGE
 * Accessor macros used by the macros in the autogenerated file
 * hal_macros.h.  They are collected into one file here for
 * convenience; they can be redefined for simulation and the like
 * more easily.
 */

#ifndef HAL_REGISTERS_H
#define HAL_REGISTERS_H

/**
 * Read a value from a memory-mapped register
 *
 * Read the memory location corresponding to the register
 *
 * `width` is the width of the register in words (i.e. 1 for 16-bit
 * registers), which may be needed by other veneers.
 */
#define hal_get_register(reg, width) (reg)


/**
 * Read a value from a memory-mapped array register
 *
 * Read the memory location corresponding to the given index into a
 * array register.
 */
#define hal_get_register_indexed(reg, i) ((reg)[i])


/**
 * Write a value to a memory-mapped register
 *
 * Write to the memory location corresponding to the register
 *
 * `width` is the width of the register in words (i.e. 1 for 16-bit
 * registers), which may be needed by other veneers.
 */
#define hal_set_register(reg, x, width) ((void) ((reg) = (x)))


/**
 * Write a value to a memory-mapped array register
 *
 * Write to the memory location corresponding to the given index into
 * a array register.
 */
#define hal_set_register_indexed(reg, i, x) ((void) ((reg)[i] = (x)))

#endif /* HAL_REGISTERS_H */
