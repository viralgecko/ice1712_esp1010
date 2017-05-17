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


#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <sound/tlv.h>

#include <sound/asoundef.h>
#include <sound/core.h>
#include <sound/ak4114.h>

#include "./ice1712.h"
#include "./esp1010.h"

struct esp_spec{
  struct ak4114 *ak4114;
  unsigned char hp;
  unsigned char mic;
  unsigned char micv0;
  unsigned char micv1;
};

static int esp_cpld_write_byte(struct snd_ice1712 *ice, unsigned char data, unsigned char addr)
{
  char mask;
  char gpio_val;
  char gpio_bit;
  char dir;
  int i;
  mask = (char)ice->gpio.write_mask;
  if(mask & ESP_CPLD_MASK)
    return -1;
  dir = (char)ice->gpio.direction;
  if(!(dir & ESP_CPLD_MASK))
    return -2;
  gpio_val = snd_ice1712_gpio_read(ice);
  gpio_val = (gpio_val & ~ESP_CPLD_MASK) | 0x03;
  snd_ice1712_gpio_write(ice, gpio_val);
  udelay(1);
  for(i = 0; i < 8; i++)
    {
      gpio_bit = (addr >> (7 - i)) & 0x01;
      snd_ice1712_gpio_write(ice, (gpio_val & 0xFC) | gpio_bit);
      udelay(1);
      snd_ice1712_gpio_write(ice, (gpio_val & 0xFC) | (gpio_bit | 0x2));
      udelay(1);
    }
  for(i = 0; i < 8; i++)
    {
      gpio_bit = (data >> (7 - i)) & 0x01;
      snd_ice1712_gpio_write(ice, (gpio_val & 0xFC) | gpio_bit);
      udelay(1);
      snd_ice1712_gpio_write(ice, (gpio_val & 0xFC) | (gpio_bit | 0x2));
      udelay(1);
    }
  snd_ice1712_gpio_write(ice, gpio_val | ESP_CPLD_MASK);
  return 0;
}

static int esp_cpld_write_vbyte(struct snd_ice1712 *ice, unsigned char data, unsigned char addr)
{
  char mask;
  char gpio_val;
  char gpio_bit;
  char dir;
  uint16_t addr1, data1;
  int i;
  mask = (char)ice->gpio.write_mask;
  if(mask & ESP_CPLD_MASK)
    return -1;
  dir = (char)ice->gpio.direction;
  if(!(dir & ESP_CPLD_MASK))
    return -2;
  gpio_val = snd_ice1712_gpio_read(ice);
  addr1 = addr + 0x100;
  data1 = (data << 1) + 1;
  for(i = 0; i < 9; i++)
    {
      gpio_bit = ( addr1 >> (8 - i)) & 0x01;
      snd_ice1712_gpio_write(ice, (gpio_val & 0xFC) | gpio_bit);
      udelay(1);
      snd_ice1712_gpio_write(ice, (gpio_val & 0xFC) | (gpio_bit | 0x2));
      udelay(1);
      if(!i)
	gpio_val ^= 0x20;
    }
  for(i = 0; i < 9; i++)
    {
      gpio_bit = ( data1 >> (8 - i)) & 0x01;
      snd_ice1712_gpio_write(ice, (gpio_val & 0xFC) | gpio_bit);
      udelay(1);
      snd_ice1712_gpio_write(ice, (gpio_val & 0xFC) | (gpio_bit | 0x2));
      udelay(1);
      if(i == 7)
	gpio_val ^= 0x20;
    }
  return 0;
}

/* 
 * AK4358 section
 */

static void esp_akm_lock(struct snd_akm4xxx *ak, int chip)
{
}

static void esp_akm_unlock(struct snd_akm4xxx *ak, int chip)
{
}

static void esp_akm_write(struct snd_akm4xxx *ak, int chip, unsigned char addr, unsigned char data)
{
  struct snd_ice1712 *ice = ak->private_data[0];
  snd_ice1712_write_i2c(ice, ESP_AK4358_ADDR, addr, data);
}

/* 
 * AK4114 section
 */

static void esp_ak4114_write(void *privdata, unsigned char reg, unsigned char val)
{
  struct snd_ice1712 *ice = (struct snd_ice1712 *)privdata;
  snd_ice1712_write_i2c(ice, ESP_AK4114_ADDR, reg, val);
}

static unsigned char esp_ak4114_read(void *privdata, unsigned char reg)
{
  struct snd_ice1712 *ice = (struct snd_ice1712 *)privdata;
  return snd_ice1712_read_i2c(ice, ESP_AK4114_ADDR, reg);
}

static int snd_ice1712_esp_hp_en_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *info)
{
  info->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
  info->count = 1;
  info->value.integer.min = 0;
  info->value.integer.max = 1;
  return 0;
}

#ifdef CONFIG_PM_SLEEP
static int esp_resume(struct snd_ice1712 *ice)
{
  struct snd_akm4xxx *ak = ice->akm;
  struct esp_spec *spec = ice->spec;
  snd_akm4xxx_reset(ak, 0);
  snd_ak4114_resume(spec->ak4114);
  return 0;
}

static int esp_suspend(struct snd_ice1712 *ice)
{
  struct snd_akm4xxx *ak = ice->akm;
  struct esp_spec *spec = ice->spec;
  snd_akm4xxx_reset(ak, 1);
  snd_ak4114_suspend(spec->ak4114);
  return 0;
}
#endif

static int snd_ice1712_esp_hp_en_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *value)
{
  struct snd_ice1712 *ice = snd_kcontrol_chip(kcontrol);
  struct esp_spec *spec = (struct esp_spec *)ice->spec;
  unsigned char new_hp = value->value.integer.value[0];
  unsigned char old_hp = spec->hp;
  int change = new_hp != old_hp;
  if(change)
    {
      if(new_hp)
	{
	  esp_cpld_write_byte(ice, ESP_HP_ON_F, ESP_HP_ADDR);
	  udelay(6);
	  esp_cpld_write_byte(ice, ESP_HP_ON_L, ESP_HP_ADDR);
	}
      else
	{
	  esp_cpld_write_byte(ice, ESP_HP_OFF_F, ESP_HP_ADDR);
	  udelay(6);
	  esp_cpld_write_byte(ice, ESP_HP_OFF_L, ESP_HP_ADDR);
        }
      spec->hp = new_hp;
    }
  return change;
}

static int snd_ice1712_esp_hp_en_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *value)
{
  struct snd_ice1712 *ice = snd_kcontrol_chip(kcontrol);
  struct esp_spec *spec = (struct esp_spec *)ice->spec;
  value->value.integer.value[0] = spec->hp;
  return 0;
}

static int snd_ice1712_esp_mic_en_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *value)
{
  struct snd_ice1712 *ice = snd_kcontrol_chip(kcontrol);
  struct esp_spec *spec = (struct esp_spec *)ice->spec;
  unsigned char mic0 = value->value.integer.value[0];
  unsigned char mic1 = value->value.integer.value[1];
  unsigned char new_mic = 0;
  unsigned char old_mic = spec->mic;
  int change;
  if(mic0)
    new_mic |= 0x40;
  else
    new_mic &= ~0x40;
  if(mic1)
    new_mic |= 0x80;
  else
    new_mic &= ~0x80;
  change = new_mic != old_mic;
  if(change)
    {
      esp_cpld_write_byte(ice, new_mic, ESP_MIC_EN_ADDR);
      spec->mic = new_mic;
    }
  return change;
}

static int snd_ice1712_esp_mic_en_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *value)
{
  struct snd_ice1712 *ice = snd_kcontrol_chip(kcontrol);
  struct esp_spec *spec = (struct esp_spec *)ice->spec;
  value->value.integer.value[0] = (spec->mic & 0x40) ? 1 : 0;
  value->value.integer.value[1] = (spec->mic & 0x80) ? 1 : 0;
  return 0;
}
static int snd_ice1712_esp_mic_vol_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *value)
{
  struct snd_ice1712 *ice = snd_kcontrol_chip(kcontrol);
  struct esp_spec *spec = (struct esp_spec *)ice->spec;
  char new_vol0 = value->value.integer.value[0];
  char new_vol1 = value->value.integer.value[1];
  char old_vol0 = spec->micv0;
  char old_vol1 = spec->micv1;
  int change0 = new_vol0 != old_vol0;
  int change1 = new_vol1 != old_vol1;
  if(change0 || change1)
    {
      esp_cpld_write_vbyte(ice, new_vol1, ESP_MIC_ADDR1);
      udelay(6);
      esp_cpld_write_vbyte(ice, new_vol0, ESP_MIC_ADDR0);
    }
  else
    return 0;
  spec->micv0 = new_vol0;
  spec->micv1 = new_vol1;
  return 1;
  
}

static int snd_ice1712_esp_mic_vol_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *value)
{
  struct snd_ice1712 *ice = snd_kcontrol_chip(kcontrol);
  struct esp_spec *spec = (struct esp_spec *)ice->spec;
  value->value.integer.value[0] = spec->micv0;
  value->value.integer.value[1] = spec->micv1;
  return 0;
}

static int snd_ice1712_esp_mic_en_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *info)
{
  info->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
  info->count = 2;
  info->value.integer.min = 0;
  info->value.integer.max = 1;
  return 0;
}

static int snd_ice1712_esp_mic_vol_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *info)
{
  info->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
  info->count = 2;
  info->value.integer.min = 0;
  info->value.integer.max = 255;
  return 0;
}

static
DECLARE_TLV_DB_SCALE(db_scale_esp_mic_vol, -11200, 50, 1);

static struct snd_kcontrol_new esp_enable_hp = {
  .iface = SNDRV_CTL_ELEM_IFACE_HWDEP,
  .name = "PCM Playback Switch",
  .access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
  .info = snd_ice1712_esp_hp_en_info,
  .put = snd_ice1712_esp_hp_en_put,
  .get = snd_ice1712_esp_hp_en_get
};

static struct snd_kcontrol_new esp_enable_mic = {
  .iface = SNDRV_CTL_ELEM_IFACE_HWDEP,
  .name = "Mic Boost",
  .access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
  .info = snd_ice1712_esp_mic_en_info,
  .put = snd_ice1712_esp_mic_en_put,
  .get = snd_ice1712_esp_mic_en_get
};

static struct snd_kcontrol_new esp_mic_vol = {
  .iface = SNDRV_CTL_ELEM_IFACE_MIXER,
  .name = "Mic Capture Volume",
  .access = SNDRV_CTL_ELEM_ACCESS_READWRITE | SNDRV_CTL_ELEM_ACCESS_TLV_READ,
  .info = snd_ice1712_esp_mic_vol_info,
  .put = snd_ice1712_esp_mic_vol_put,
  .get = snd_ice1712_esp_mic_vol_get,
  .tlv.p = db_scale_esp_mic_vol
};

static const struct snd_akm4xxx akm_esp_dac = {
  .type = SND_AK4358,
  .num_dacs = 8,
  .ops = {
    .lock = esp_akm_lock,
    .unlock = esp_akm_unlock,
    .write = esp_akm_write
  },
};

static int esp_init(struct snd_ice1712 *ice)
{
    static const unsigned char ak4114_val[] = { AK4114_RST | AK4114_PWN |
      			AK4114_OCKS0 | AK4114_OCKS1,
			AK4114_DIF_I24I2S,
			AK4114_TX1E,
			AK4114_EFH_1024 | AK4114_DIT |
			AK4114_IPS(1),
			0,
			0
  };
  static const unsigned char ak4114_txcsb[] = { 0x41, 0x02, 0x2c, 0x00, 0x00};
  int err;
  struct snd_akm4xxx *ak;
  struct esp_spec *spec;
  /* needed here:
  create array unsigned char ak4114_init_vals + magic vals
  create array unsigned char aka4114_init_txcsb + magic vals
  alloc mem for ak4114 (ice->spec)
  struct ak4114 *ak4114
  call snd_ak4114_create
  
  set ice->num_total_adcs dacs
  alloc mem for akm (ice->akm)
  set ice->akm_codec = 1;
  call snd_ice1712_akm4xxx_init
  */
  ice->num_total_adcs = 8;
  ice->num_total_dacs = 8;
  spec = kmalloc(sizeof(struct esp_spec), GFP_KERNEL);
  if(!spec)
    return -ENOMEM;
  spec->mic = 0;
  spec->hp = 0;
  spec->micv0 = 0;
  spec->micv1 = 0;
  ice->spec = spec;
#if 0
  printk(KERN_DEBUG "esp_init(struct snd_ice1712 *)\n");
#endif
  err = snd_ak4114_create(ice->card,
		    esp_ak4114_read,
		    esp_ak4114_write,
		    ak4114_val,
		    ak4114_txcsb,
		    ice, &spec->ak4114);
  if(err < 0)
    return err;
#ifdef CONFIG_PM_SLEEP
  ice->pm_resume = esp_resume;
  ice->pm_suspend = esp_suspend;
  ice->pm_suspend_enabled = 1;
#endif
  ice->akm = ak = kmalloc(sizeof(struct snd_akm4xxx),GFP_KERNEL);
  if(!ak)
    return -ENOMEM;
  ice->akm_codecs = 1;
  err = snd_ice1712_akm4xxx_init(ak, &akm_esp_dac, NULL, ice);
  return err;
  
}

static int esp_add_controls(struct snd_ice1712 *ice)
{
  int err;
  struct esp_spec *spec = ice->spec;
  if(!ice)
    return -1;
#if 0
  printk(KERN_DEBUG "esp_add_controls(struct snd_ice1712 *)\n");
#endif
  err = snd_ak4114_build(spec->ak4114, ice->pcm_pro->streams[SNDRV_PCM_STREAM_PLAYBACK].substream, ice->pcm_pro->streams[SNDRV_PCM_STREAM_CAPTURE].substream);
  if (err < 0)
    return err;
  err = snd_ice1712_akm4xxx_build_controls(ice);
  if(err < 0)
     return err;
  err = snd_ctl_add(ice->card, snd_ctl_new1(&esp_enable_hp, ice));
  if (err < 0)
    return err;
  err = snd_ctl_add(ice->card, snd_ctl_new1(&esp_enable_mic, ice));
  if (err < 0)
    return err;
  err = snd_ctl_add(ice->card, snd_ctl_new1(&esp_mic_vol, ice));
  return err;
}

struct snd_ice1712_card_info snd_ice1712_esp_cards[] = {
  {
    .subvendor = ICE1712_SUBDEVICE_ESP1010,
    .name = "ESI ESP1010",
    .model = "esp1010",
    .chip_init = esp_init,
    .build_controls = esp_add_controls,
  },
  /*  {
  .subvendor = ICE1712_SUBDEVICE_MAYA1010,
    .name = "Audiotrack MAYA1010",
    .model = "maya1010",
    .chip_init = esp_init,
    .build_controls = esp_add_controls,
    },*/
  {}
};
