/**
 * QEMU WLAN access point emulation
 *
 * Copyright (c) 2008 Clemens Kolbitsch
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Modifications:
 *  2008-February-24  Clemens Kolbitsch :
 *                                  New implementation based on ne2000.c
 *  18/1/22 Martin Johnson : Modified for esp32 wifi emulation
 */

#include "qemu/osdep.h"
#include "net/net.h"
#include "qemu/timer.h"

#include "hw/misc/esp32_wifi.h"
#include "esp32_wlan.h"
#include "esp32_wlan_packet.h"

void Esp32_WLAN_insert_frame(Esp32WifiState *s, struct mac80211_frame *frame);

int beacon_ap=0;

access_point_info access_points[]={
    {"Open Wifi",4,-40},
    {"MasseyWifi",6,-30},
    {"Home Wifi",7,-70},
    {"My Wifi",8,-75},
    {"New Wifi",10,-90},
};

int nb_aps=sizeof(access_points)/sizeof(access_point_info);

static void Esp32_WLAN_beacon_timer(void *opaque)
{
    struct mac80211_frame *frame;
    Esp32WifiState *s = (Esp32WifiState *)opaque;

    frame = Esp32_WLAN_create_beacon_frame(&access_points[beacon_ap]);
    beacon_ap=(beacon_ap+1)%nb_aps;
    if (frame) {
        Esp32_WLAN_init_frame(s, frame);
        Esp32_WLAN_insert_frame(s, frame);
    }
    timer_mod(s->beacon_timer, qemu_clock_get_ns(QEMU_CLOCK_REALTIME) + 50000000);
}

static void Esp32_WLAN_inject_timer(void *opaque)
{
    Esp32WifiState *s = (Esp32WifiState *)opaque;
    struct mac80211_frame *frame;

    frame = s->inject_queue;
    if (frame) {
        // remove from queue
        s->inject_queue_size--;
        s->inject_queue = frame->next_frame;
        Esp32_sendFrame(s, (void *)frame, frame->frame_length,frame->signal_strength);
        free(frame);
    }
    if (s->inject_queue_size > 0) {
        // there are more packets... schedule
        // the timer for sending them as well
        timer_mod(s->inject_timer, qemu_clock_get_ns(QEMU_CLOCK_REALTIME) + 25000000);
    } else {
        // we wait until a new packet schedules
        // us again
        s->inject_timer_running = 0;
    }

}



void Esp32_WLAN_insert_frame(Esp32WifiState *s, struct mac80211_frame *frame)
{
    struct mac80211_frame *i_frame;

    s->inject_queue_size++;
    i_frame = s->inject_queue;
    if (!i_frame) {
        s->inject_queue = frame;
    } else {
        while (i_frame->next_frame) {
            i_frame = i_frame->next_frame;
        }
        i_frame->next_frame = frame;
    }

    if (!s->inject_timer_running) {
        // if the injection timer is not
        // running currently, let's schedule
        // one run...
        s->inject_timer_running = 1;
        timer_mod(s->inject_timer, qemu_clock_get_ns(QEMU_CLOCK_REALTIME) + 25);
    }

}

static _Bool Esp32_WLAN_can_receive(NetClientState *ncs)
{
    Esp32WifiState *s = qemu_get_nic_opaque(ncs);

    if (s->ap_state != Esp32_WLAN__STATE_ASSOCIATED) {
        // we are currently not connected
        // to the access point
        return 0;
    }
    if (s->inject_queue_size > Esp32_WLAN__MAX_INJECT_QUEUE_SIZE) {
        // overload, please give me some time...
        return 0;
    }

    return 1;
}

static ssize_t Esp32_WLAN_receive(NetClientState *ncs,
                                    const uint8_t *buf, size_t size)
{
    Esp32WifiState *s = qemu_get_nic_opaque(ncs);
    struct mac80211_frame *frame;
    if (!Esp32_WLAN_can_receive(ncs)) {
        // this should not happen, but in
        // case it does, let's simply drop
        // the packet
        return -1;
    }

    if (!s) {
        return -1;
    }
    /*
     * A 802.3 packet comes from the qemu network. The
     * access points turns it into a 802.11 frame and
     * forwards it to the wireless device
     */
    frame = Esp32_WLAN_create_data_packet(s, buf, size);
    if (frame) {
        Esp32_WLAN_init_frame(s, frame);
        Esp32_WLAN_insert_frame(s, frame);
    }
    return size;
}

static void Esp32_WLAN_cleanup(NetClientState *ncs)
{

}

static NetClientInfo net_info = {
    .type = NET_CLIENT_DRIVER_NIC,
    .size = sizeof(NICState),
    .can_receive = Esp32_WLAN_can_receive,
    .receive = Esp32_WLAN_receive,
    .cleanup = Esp32_WLAN_cleanup,
};

void Esp32_WLAN_setup_ap(DeviceState *dev,Esp32WifiState *s)
{

    s->ap_state = Esp32_WLAN__STATE_NOT_AUTHENTICATED;
    s->ap_macaddr[0] = 0x01;
    s->ap_macaddr[1] = 0x13;
    s->ap_macaddr[2] = 0x46;
    s->ap_macaddr[3] = 0xbf;
    s->ap_macaddr[4] = 0x31;
    s->ap_macaddr[5] = 0x59;

    s->macaddr[0] = 0x11;
    s->macaddr[1] = 0x01;
    s->macaddr[2] = 0x00;
    s->macaddr[3] = 0xc4;
    s->macaddr[4] = 0x0a;
    s->macaddr[5] = 0x24;

    s->inject_timer_running = 0;
    s->inject_sequence_number = 0;

    s->inject_queue = NULL;
    s->inject_queue_size = 0;


    s->beacon_timer = timer_new_ns(QEMU_CLOCK_REALTIME, Esp32_WLAN_beacon_timer, s);
    timer_mod(s->beacon_timer, qemu_clock_get_ns(QEMU_CLOCK_REALTIME)+100000000);

    // setup the timer but only schedule
    // it when necessary...
    s->inject_timer = timer_new_ns(QEMU_CLOCK_REALTIME, Esp32_WLAN_inject_timer, s);

    s->nic = qemu_new_nic(&net_info, &s->conf, object_get_typename(OBJECT(s)), dev->id, s);
    qemu_format_nic_info_str(qemu_get_queue(s->nic), s->macaddr);
}

void Esp32_WLAN_handle_frame(Esp32WifiState *s, struct mac80211_frame *frame)
{
    struct mac80211_frame *reply = NULL;
    unsigned long ethernet_frame_size;
    unsigned char ethernet_frame[1518];

    //printf("Handle Frame %d %d %d\n",frame->frame_control.type,frame->frame_control.sub_type,wifi_channel);
    access_point_info *ap_info=0;
    for(int i=0;i<nb_aps;i++)
        if(access_points[i].channel==wifi_channel)
            ap_info=&access_points[i];
    if(ap_info!=0) {

        if ((frame->frame_control.type == IEEE80211_TYPE_MGT) &&
                (frame->frame_control.sub_type == IEEE80211_TYPE_MGT_SUBTYPE_PROBE_REQ)) {
            DEBUG_PRINT_AP(("Received probe request!\n"));
            reply = Esp32_WLAN_create_probe_response(ap_info);
        } else if ((frame->frame_control.type == IEEE80211_TYPE_MGT) &&
                (frame->frame_control.sub_type == IEEE80211_TYPE_MGT_SUBTYPE_AUTHENTICATION)) {
            DEBUG_PRINT_AP(("Received authentication!\n"));
            reply = Esp32_WLAN_create_authentication(ap_info);

            if (s->ap_state == Esp32_WLAN__STATE_NOT_AUTHENTICATED) {
                // if everything is going according to
                // the state machine, let's jump into the
                // next state
                s->ap_state = Esp32_WLAN__STATE_AUTHENTICATED;
            }
        } else if ((frame->frame_control.type == IEEE80211_TYPE_MGT) &&
                (frame->frame_control.sub_type == IEEE80211_TYPE_MGT_SUBTYPE_DEAUTHENTICATION)) {
            DEBUG_PRINT_AP(("Received deauthentication!\n"));
        //   reply = Esp32_WLAN_create_deauthentication();

            // some systems (e.g. WinXP) won't send a
            // disassociation. just believe that the
            // deauthentication is ok... nothing bad
            // can happen anyways ;-)
            s->ap_state = Esp32_WLAN__STATE_NOT_AUTHENTICATED;
        } else if ((frame->frame_control.type == IEEE80211_TYPE_MGT) &&
                (frame->frame_control.sub_type == IEEE80211_TYPE_MGT_SUBTYPE_ASSOCIATION_REQ)) {
            DEBUG_PRINT_AP(("Received association request!\n"));
            reply = Esp32_WLAN_create_association_response(ap_info);

            if (s->ap_state == Esp32_WLAN__STATE_AUTHENTICATED) {
                // if everything is going according to
                // the state machine, let's jump into the
                // next state
                s->ap_state = Esp32_WLAN__STATE_ASSOCIATED;
            }
        } else if ((frame->frame_control.type == IEEE80211_TYPE_MGT) &&
                (frame->frame_control.sub_type == IEEE80211_TYPE_MGT_SUBTYPE_DISASSOCIATION)) {
            DEBUG_PRINT_AP(("Received disassociation!\n"));
            reply = Esp32_WLAN_create_disassociation();

            if (s->ap_state == Esp32_WLAN__STATE_ASSOCIATED) {
                // if everything is going according to
                // the state machine, let's jump into the
                // next state
                s->ap_state = Esp32_WLAN__STATE_AUTHENTICATED;
            }
        } else if ((frame->frame_control.type == IEEE80211_TYPE_DATA) &&
                (frame->frame_control.sub_type == IEEE80211_TYPE_DATA_SUBTYPE_DATA) &&
                (s->ap_state == Esp32_WLAN__STATE_ASSOCIATED)) {
            /*
            * The access point uses the 802.11 frame
            * and sends a 802.3 frame into the network...
            * This packet is then understandable by
            * qemu-slirp
            *
            * If we ever want the access point to offer
            * some services, it can be added here!!
            */
            // ethernet header type
            ethernet_frame[12] = frame->data_and_fcs[6];
            ethernet_frame[13] = frame->data_and_fcs[7];

            // the new originator of the packet is
            // the access point
            memcpy(&ethernet_frame[6], s->ap_macaddr, 6);

            if (ethernet_frame[12] == 0x08 && ethernet_frame[13] == 0x06) {
                // for arp request, we use a broadcast
                memset(&ethernet_frame[0], 0xff, 6);
            } else {
                // otherwise we forward the packet to
                // where it really belongs
                memcpy(&ethernet_frame[0], frame->destination_address, 6);
            }

            // add packet content
            ethernet_frame_size = frame->frame_length - 24 - 4 - 8;

            // for some reason, the packet is 22 bytes too small (??)
            ethernet_frame_size += 22;
            if (ethernet_frame_size > sizeof(ethernet_frame)) {
                ethernet_frame_size = sizeof(ethernet_frame);
            }
            memcpy(&ethernet_frame[14], &frame->data_and_fcs[8], ethernet_frame_size);

            // add size of ethernet header
            ethernet_frame_size += 14;

            /*
            * Send 802.3 frame
            */
            qemu_send_packet(qemu_get_queue(s->nic), ethernet_frame, ethernet_frame_size);
            
        }
        if (reply) {
            reply->signal_strength=ap_info->sigstrength;
            memcpy(reply->destination_address, frame->source_address, 6);
            Esp32_WLAN_init_frame(s, reply);
            Esp32_WLAN_insert_frame(s, reply);
        }
    }

    

}

