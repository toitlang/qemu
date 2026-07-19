/*
 * ESP32-S3 eFuse emulation
 *
 * Copyright (c) 2024 Espressif Systems (Shanghai) Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 or
 * (at your option) any later version.
 */

#include "qemu/osdep.h"
#include "hw/nvram/esp32s3_efuse.h"


static void esp32s3_efuse_realize(DeviceState *dev, Error **errp)
{
    ESP32S3EfuseClass* esp32s3_class = ESP32S3_EFUSE_GET_CLASS(dev);
    ESPEfuseState *s = ESP_EFUSE(dev);

    esp32s3_class->parent_realize(dev, errp);

    if (s->blk == NULL) {
        assert(s->mirror != NULL);

        /* Default octal flash and PSRAM pad configuration. */
        s->efuses.blocks.rd_repeat_data3 = 0x80000100;
        s->efuses.blocks.rd_mac_spi_sys_0 = 0x00c40a24;
        s->efuses.blocks.rd_mac_spi_sys_1 = 0x07de1001;
        s->efuses.blocks.rd_mac_spi_sys_2 = 0x86571b76;
        s->efuses.blocks.rd_mac_spi_sys_3 = 0x020648e2;
        s->efuses.blocks.rd_mac_spi_sys_4 = 0x00008260;
        s->efuses.blocks.rd_sys_part1_data4 = 0x00000001;

        memcpy(s->mirror, &s->efuses.blocks, sizeof(ESPEfuseBlocks));
    }
}


static void esp32s3_efuse_init(Object *obj)
{
}

static void esp32s3_efuse_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ESP32S3EfuseClass* esp32s3_efuse = ESP32S3_EFUSE_CLASS(klass);

    device_class_set_parent_realize(dc, esp32s3_efuse_realize, &esp32s3_efuse->parent_realize);
}

static const TypeInfo esp32s3_efuse_info = {
    .name = TYPE_ESP32S3_EFUSE,
    .parent = TYPE_ESP_EFUSE,
    .instance_size = sizeof(ESP32S3EfuseState),
    .instance_init = esp32s3_efuse_init,
    .class_init = esp32s3_efuse_class_init,
    .class_size = sizeof(ESP32S3EfuseClass)
};

static void esp32s3_efuse_register_types(void)
{
    type_register_static(&esp32s3_efuse_info);
}

type_init(esp32s3_efuse_register_types)
