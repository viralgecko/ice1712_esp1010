/*
 *   ALSA driver for ICEnsemble ICE1712 (Envy24)
 *
 *   Lowlevel functions for ESI ESP1010
 *
 *	Copyright (c) 2017 Andreas Merl <merl.andreas@outlook.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef __SOUND_ESP1010_H
#define __SOUND_ESP1010_H

//#define ESP_DEVICE_DESC "{ESI Audio,ESP 1010}," "{Audiotrack, Maya 1010},"
#define ESP_DEVICE_DESC "{ESI Audio,ESP 1010},"

#define ICE1712_SUBDEVICE_ESP1010 0x31315441
//#define ICE1712_SUBDEVICE_MAYA1010 0x30

extern struct snd_ice1712_card_info snd_ice1712_esp_cards[];

#define ESP_AK4114_ADDR 0x20
#define ESP_AK4358_ADDR 0x22
#define ESP_HP_ADDR 0xF0
#define ESP_MIC_EN_ADDR 0xF1
#define ESP_MIC_ADDR0 0xF3
#define ESP_MIC_ADDR1 0xEB
#define ESP_HP_ON_F 0x20
#define ESP_HP_ON_L 0x30
#define ESP_HP_OFF_F 0x30
#define ESP_HP_OFF_L 0x00
#define ESP_CPLD_MASK 0x3B

#endif
