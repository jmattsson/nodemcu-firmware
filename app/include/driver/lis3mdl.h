#ifndef LIS3MDL_H
#define LIS3MDL_H

/* register addresses: A: accel, M: mag, T: temp */
#define ADDR_WHO_AM_I			0x0F
#define WHO_I_AM				0x3D


#define ADDR_CTRL_REG0			0x1F
#define ADDR_CTRL_REG1			0x20
#define ADDR_CTRL_REG2			0x21
#define ADDR_CTRL_REG3			0x22
#define ADDR_CTRL_REG4			0x23
#define ADDR_CTRL_REG5			0x24

#define ADDR_STATUS			0x27
#define ADDR_OUT_X_L			0x28
#define ADDR_OUT_X_H			0x29
#define ADDR_OUT_Y_L			0x2A
#define ADDR_OUT_Y_H			0x2B
#define ADDR_OUT_Z_L			0x2C
#define ADDR_OUT_Z_H			0x2D

#define ADDR_OUT_TEMP_L			0x2E
#define ADDR_OUT_TEMP_H			0x2F

#define ADDR_FIFO_CTRL			0x2e
#define ADDR_FIFO_SRC			0x2f

#define ADDR_INT_CFG			0x30
#define ADDR_INT_SRC			0x31
#define ADDR_INT_THS_L			0x32
#define ADDR_INT_THS_H			0x33

#define REG1_FAST_ODR_SELECT            (1<<1)
#define REG1_OM_LP                      ((0<<6) | (0<<5))
#define REG1_OM_MP                      ((0<<6) | (1<<5))
#define REG1_OM_HP                      ((1<<6) | (0<<5))
#define REG1_OM_UHP                     ((1<<6) | (1<<5))
#define REG1_FAST_ODR_1000              REG1_OM_LP
#define REG1_FAST_ODR_560               REG1_OM_MP
#define REG1_FAST_ODR_300               REG1_OM_HP
#define REG1_FAST_ODR_155               REG1_OM_UHP

#define REG1_RATE_0_625                 ((0<<4) | (0<<3) | (1<<2))
#define REG1_RATE_1_25                  ((0<<4) | (0<<3) | (0<<2))
#define REG1_RATE_2_5                   ((0<<4) | (1<<3) | (1<<2))
#define REG1_RATE_5                     ((0<<4) | (1<<3) | (0<<2))
#define REG1_RATE_10                    ((1<<4) | (0<<3) | (1<<2))
#define REG1_RATE_20                    ((1<<4) | (0<<3) | (0<<2))
#define REG1_RATE_40                    ((1<<4) | (1<<3) | (1<<2))
#define REG1_RATE_80                    ((1<<4) | (1<<3) | (0<<2))

#define REG1_TEMP_ENABLE                (1<<7)

#define REG2_FULL_SCALE_BITS	((1<<6) | (1<<5))
#define REG2_FULL_SCALE_4GA	((0<<6) | (0<<5))
#define REG2_FULL_SCALE_8GA	((0<<6) | (1<<5))
#define REG2_FULL_SCALE_12GA	((1<<6) | (0<<5))
#define REG2_FULL_SCALE_16GA	((1<<6) | (1<<5))

#define REG3_CONT_MODE		((0<<1) | (0<<0))
#define REG3_SINGLE_MODE	((0<<1) | (1<<0))
#define REG3_OFF1_MODE   	((1<<1) | (0<<0))
#define REG3_OFF2_MODE		((1<<1) | (1<<0))
#define REG3_MODE_BITS		((1<<1) | (1<<0))

#define REG4_OMZ_LP                     ((0<<3) | (0<<2))
#define REG4_OMZ_MP                     ((0<<3) | (1<<2))
#define REG4_OMZ_HP                     ((1<<3) | (0<<2))
#define REG4_OMZ_UHP                    ((1<<3) | (1<<2))

#endif
