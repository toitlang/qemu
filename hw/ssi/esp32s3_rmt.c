/*
 * ESP32 RMT controller
 *
 * Copyright (c) 2019 Espressif Systems (Shanghai) Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 or
 * (at your option) any later version.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "sysemu/sysemu.h"
#include "hw/hw.h"
#include "hw/sysbus.h"
#include "hw/registerfields.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/ssi/ssi.h"
#include "hw/ssi/esp32s3_rmt.h"

#define ESP32S3_RMT_REG_SIZE    0x1000

#define DEBUG(x) 

// send txlim data values, stop if a value is 0
// set the correct raw int for tx_end or tx_thr_event
static void send_data(Esp32S3RmtState *s, int channel) {
    DEBUG(printf("send %d %d %d\n",channel, s->txlim[channel]&0x1ff, s->sent);)
    BusState *b = BUS(s->rmt);
    BusChild *ch = QTAILQ_FIRST(&b->children);
    SSIPeripheral *slave = SSI_PERIPHERAL(ch->child);
    SSIPeripheralClass *ssc = SSI_PERIPHERAL_GET_CLASS(slave);
    if(s->sent==0) {
        s->start_time=qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    }
  //  while(1) {
        /*
        // don't send data when interrupt hasn't been handled
        // a real device can't do this but it's necessary because 
        // qemu can't always keep up.
        if( s->int_raw & s->int_en & (1<<(channel+8) | (1<<channel))) {
            // send data when the interrupt is cleared
            s->unsent_data=true;
            return;
        } else 
            s->unsent_data=false;
*/
        int memsize=FIELD_EX32(s->conf0[channel],RMT_CONF0,MEM_SIZE)*48;
        int divcnt=FIELD_EX32(s->conf0[channel],RMT_CONF0,DIV_CNT);
        int tx_lim=FIELD_EX32(s->txlim[channel],RMT_TX_LIM,TX_LIM);

        DEBUG(printf("divcnt %d\n",divcnt);)
        for (int i = 0; i < tx_lim+1 ; i++) {    
            int v=s->data[((i+s->sent)%memsize+channel*48)%512];
            // adjust periods based on the divider
            int d0=v&0x7fff;
            int d1=(v>>16)&0x7fff;
            d0=(d0*divcnt)/8;
            d1=(d1*divcnt)/8;
            v=(v&0x80008000) | d0 | (d1<<16);
            if((v&0x7fff7fff)==0) { // stop sending when we see a zero period
                DEBUG(printf("end send\n");)
                timer_mod_anticipate_ns(&s->rmt_timer,s->start_time+1300*s->sent);
                
                return;            
            }
            if(i<tx_lim)
                ssc->transfer(slave,v);
        }
        s->sent+=tx_lim;
        s->int_raw|=(1<<(channel+8));
        if(s->int_en & (1<<(channel+8)))
            qemu_irq_raise(s->irq);
    //}
}

static void esp32_rmt_timer_cb(void *opaque) {
    Esp32S3RmtState *s = ESP32S3_RMT(opaque);
    // set complete for any enabled channels 
    for(int channel=0;channel<4;channel++) {
        if(FIELD_EX32(s->conf0[channel],RMT_CONF0,TX_START)) {
            s->int_raw|=(1<<(channel)) | (1<<(channel+8));
//            s->int_raw&=~(1<<(channel+8));
            s->sent=0;
            s->conf0[channel] = FIELD_DP32(s->conf0[channel],RMT_CONF0,TX_START,0);
            if(s->int_en & (1<<channel)) {
                 qemu_irq_raise(s->irq);
            }
        }
    }
}

static void send_unsent_data(Esp32S3RmtState *s) {
     // send data for any enabled channels 
     for(int i=0;i<4;i++) {
        if(FIELD_EX32(s->conf0[i],RMT_CONF0,TX_START)) {
             send_data(s,i);
         }
     }
}

static uint64_t esp32_rmt_read(void *opaque, hwaddr addr, unsigned int size)
{
    Esp32S3RmtState *s = ESP32S3_RMT(opaque);
    uint64_t r = 0;
    int channel;
    switch (addr) {
    case A_RMT_CH0CONF0 ... A_RMT_CH3CONF0:
        channel=(addr-A_RMT_CH0CONF0)/4; 
        r=s->conf0[channel];
        break;
    case A_RMT_INT_RAW:
        r = s->int_raw ;
        break;
    case A_RMT_INT_ST:
        r = s->int_raw & s->int_en;
        break;
    case A_RMT_INT_ENA:
        r = s->int_en;
        break;
    case A_RMT_CH0_TX_LIM ... A_RMT_CH3_TX_LIM:
        channel=(addr-A_RMT_CH0_TX_LIM)/4;
        r = s->txlim[channel];
        break;
    case A_RMT_DATA ... A_RMT_DATA+(ESP32S3_RMT_BUF_WORDS-1)* sizeof(uint32_t):
        r = s->data[(addr-A_RMT_DATA)/sizeof(uint32_t)];
        break;
    case A_RMT_SYS_CONF:
        r = s->apb_conf;
        break;
    }
    DEBUG(printf("rmt read %lx %lx\n",addr,r);)
    return r;
}


static void esp32_rmt_write(void *opaque, hwaddr addr,
                       uint64_t value, unsigned int size)
{
    Esp32S3RmtState *s = ESP32S3_RMT(opaque);
    DEBUG(if(addr<A_RMT_DATA) printf("rmt write %lx %lx\n",addr,value);)
    int channel;
    switch (addr) {
    case A_RMT_CH0CONF0 ... A_RMT_CH3CONF0:
        channel=(addr-A_RMT_CH0CONF0)/4; 
        s->conf0[channel]=value;
        if(FIELD_EX32(value,RMT_CONF0,MEM_RD_RESET)) {
            s->sent=0;
        }
        if(FIELD_EX32(value,RMT_CONF0,TX_START)) {
            // send data
            send_data(s,channel);
        }
        break;
    case A_RMT_CH0_TX_LIM ... A_RMT_CH3_TX_LIM:
        channel=(addr-A_RMT_CH0_TX_LIM)/4;
        s->txlim[channel]=value;
        break;
    case A_RMT_INT_ENA:
        s->int_en=value;
        DEBUG (printf("int ena %x %x\n",s->int_en,s->int_raw);)
        if(s->int_en & s->int_raw)
            qemu_irq_raise(s->irq);
        else
            qemu_irq_lower(s->irq);
        //if(s->unsent_data) 
        //    send_unsent_data(s);
        if(value&1) {
            send_data(s,0);
        //    send_data(s,0);
        }

        break;
    case A_RMT_INT_CLR:
        s->int_raw&=(~value);
     //   if((s->int_raw & s->int_en)==0) {
            qemu_irq_lower(s->irq);
     //   }
      //  if(s->unsent_data) 
            send_unsent_data(s);
        break;
    case A_RMT_DATA ... A_RMT_DATA+(ESP32S3_RMT_BUF_WORDS-1)* sizeof(uint32_t):
        s->data[(addr-A_RMT_DATA)/sizeof(uint32_t)]=value;
        break;
    case A_RMT_SYS_CONF:
        s->apb_conf=value;
        break;
    }
}


static const MemoryRegionOps esp32_rmt_ops = {
    .read =  esp32_rmt_read,
    .write = esp32_rmt_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void esp32_rmt_reset(DeviceState *dev)
{
    Esp32S3RmtState *s = ESP32S3_RMT(dev);
    s->int_raw=0;
    s->sent=0;
    s->int_en=0;
    qemu_irq_lower(s->irq);
    timer_del(&s->rmt_timer);
    for(int i=0;i<4;i++) {
        s->conf0[i]=0;
        s->txlim[i]=0x80;
    }
}

static void esp32_rmt_realize(DeviceState *dev, Error **errp)
{
}

static void esp32_rmt_init(Object *obj)
{
    Esp32S3RmtState *s = ESP32S3_RMT(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->iomem, obj, &esp32_rmt_ops, s,
                          TYPE_ESP32S3_RMT, ESP32S3_RMT_REG_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);
    timer_init_ns(&s->rmt_timer, QEMU_CLOCK_VIRTUAL, esp32_rmt_timer_cb, s);
    s->rmt = ssi_create_bus(DEVICE(s), "rmt");
    esp32_rmt_reset((DeviceState *)s);
}

static Property esp32_rmt_properties[] = {
    DEFINE_PROP_END_OF_LIST(),
};

static void esp32_rmt_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->legacy_reset = esp32_rmt_reset;
    dc->realize = esp32_rmt_realize;
    device_class_set_props(dc, esp32_rmt_properties);
}

static const TypeInfo esp32_rmt_info = {
    .name = TYPE_ESP32S3_RMT,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Esp32S3RmtState),
    .instance_init = esp32_rmt_init,
    .class_init = esp32_rmt_class_init
};

static void esp32_rmt_register_types(void)
{
    type_register_static(&esp32_rmt_info);
}

type_init(esp32_rmt_register_types)
