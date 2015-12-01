#ifndef _OTAUPGRADE_H_
#define _OTAUPGRADE_H_

/**
 * Note the booted slot, so the overridden Cache_Read_Enable can map
 * the correct flash area.
 *
 * The slot information is passed by the
 * nodemcu-bootloader to the user_start_trampoline.
 * @param which Booted slot ID (0 or 1).
 */
void otaupgrade_save_booted_slot (uint8_t which);

#endif
