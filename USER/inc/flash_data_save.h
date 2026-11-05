#ifndef __FLASH_DATA_SAVE_H
#define __FLASH_DATA_SAVE_H
#include <stdio.h>
#include "mm32_device.h"
#include "hal_conf.h"



//void write_half_data(uint32_t Address,uint16_t write_data);
uint16_t read_half_data(uint32_t Address);
/* 中文：两页事务日志保存；返回1仅表示完整提交并读回验证成功。须由前台停机门控调用。 */
uint8_t save_user_data_checked(void);
void save_user_data(void);
void read_user_data(void);
#endif
