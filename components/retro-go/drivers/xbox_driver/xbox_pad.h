// xbox_pad.h
#pragma once

#include <stdint.h>
#include <stdbool.h>

// 初始化 Xbox 蓝牙扫描和连接任务
void xbox_pad_init(void);

// 获取当前 Xbox 手柄的按键状态（映射到 Retro-Go 的位掩码）
uint32_t xbox_pad_get_state(void);