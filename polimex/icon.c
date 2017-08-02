// Copyright 2016 by TMCDOS

#include "esp8266.h"
#include "espconn.h"
#include "config.h"
#include "cgiservices.h" // for flash_maps
#include "uart.h"
#include "serbridge.h"
#include "polimex.h"
#include "icon.h"
#include "http_sdk.h"

ics_type icon_state; // state machine position
bool cmd_wait; // if a command from SDK is waiting to be sent
uint8_t icon_adr; // which controller is currently being asked for data
uint8_t saved_adr; // updated from /sdk/cmd.json
uint8_t icon_cmd; // what command is currently executing the ICON_ADR controller
uint8_t saved_cmd; // updated from /sdk/cmd.json

uint8_t icon_start_adr; // starting address for iCON-bus scan
uint8_t icon_stop_adr; // ending address for iCON-bus scan
uint8_t icon_scan_adr; // currently being scanned device ID

os_timer_t icon_timer;

uint8_t icon_count; // numer of valid elements in ICON_DEV
uint8_t icon_current; // currently polled element in ICON_DEV
icon_node icon_dev[MAX_ICON];

int icon_data_len; // how many bytes in ICON_DATA are to be send with next ICON_CMD
char icon_data[MAX_DATA];

uint8_t icon_len; // current amount of symbols in ICON_BUF
icon_input icon_buf;


uint8_t ICACHE_FLASH_ATTR calc_crc(const char *buf, int len)
{
  uint8_t crc,i,c;
  int n;
  crc = 0;
  for(n=0; n<len; n++)
  {
    i = (buf[n] ^ crc);
    c = 0;
    if(i & 1)   c^= 0x5e;
    if(i & 2)   c^= 0xbc;
    if(i & 4)   c^= 0x61;
    if(i & 8)   c^= 0xc2;
    if(i & 16)  c^= 0x9d;
    if(i & 32)  c^= 0x23;
    if(i & 64)  c^= 0x46;
    if(i & 128) c^= 0x8c;
    crc = c;
  }
  return crc;
}

// this is reply for 0xF0 command
void ICACHE_FLASH_ATTR icon_scan_reply(void) 
{
  if(icon_dev[icon_current].err == 0xE0)
  {
    if(icon_count < MAX_ICON)
    {
      NODE_DBG("Add device at position %d\n",icon_count);
      // add newly discovered iCON device to the list
      memset(&icon_dev[icon_count], 0, sizeof(icon_node)); 
      icon_dev[icon_count].adr = icon_adr;
      memcpy(&icon_dev[icon_current].ctrl_info, &icon_buf.ans_scan.hw_type, sizeof(scan_result));
      icon_count++;
    }
    else 
    {
      icon_start_poll();
      return;
    }
  }
  icon_scan_next();
}

// this is short reply for 0xFA command
void ICACHE_FLASH_ATTR icon_event_short(void) 
{
  int i;
  
  NODE_DBG("Empty event received for #%d\n",icon_adr);
  //icon_dev[icon_current].modified = 0;
  // no new events - only update some info variables about controller
  //if(icon_buf.ans_empty.in != icon_dev[icon_current].inp) 
  {
    icon_dev[icon_current].inp = icon_buf.ans_empty.in;
    //icon_dev[icon_current].modified = 1;
  }
  //if(icon_buf.ans_empty.out != icon_dev[icon_current].outp) 
  {
    icon_dev[icon_current].outp = icon_buf.ans_empty.out;
    //icon_dev[icon_current].modified = 1;
  }
  //if(icon_buf.ans_empty.UL != 0 && icon_buf.ans_empty.UH != 0 
  //  && abs(
  //    ((icon_buf.ans_empty.UL & 15) + 10*(icon_buf.ans_empty.UL >> 4) + 100*(icon_buf.ans_empty.UH & 15) + 1000*(icon_buf.ans_empty.UH >> 4)) -
  //    ((icon_dev[icon_current].UL & 15) + 10*(icon_dev[icon_current].UL >> 4) + 100*(icon_dev[icon_current].UH & 15) + 1000*(icon_dev[icon_current].UH >> 4))
  //  ) > 99)
  {
    icon_dev[icon_current].UL = icon_buf.ans_empty.UL;
    icon_dev[icon_current].UH = icon_buf.ans_empty.UH;
    //icon_dev[icon_current].modified = 1;
  }
  for(i=0; i<sizeof(icon_buf.ans_empty.DT); i++)
    //if(icon_buf.ans_empty.DT[i] != icon_dev[icon_current].DT[i]) 
    {
      icon_dev[icon_current].DT[i] = icon_buf.ans_empty.DT[i];
      //icon_dev[icon_current].modified = 1;
    }
  //if(icon_dev[icon_current].modified != 0)
  {
    //NODE_DBG("No events - but some other change for #%d\n",icon_adr);
    // send info to callback URL
    //send_json_info();
  }
  //else
    icon_next_poll();
}

// this is long reply for 0xFA command
void ICACHE_FLASH_ATTR icon_event_long(void) 
{
  NODE_DBG("New event for #%d\n",icon_adr);
  // new event
  if(flashConfig.flags & F_EVENT_PUSH)
  {
    memcpy(&icon_dev[icon_current].event, &icon_buf.ans_event.event_data, sizeof(icon_event_format));
    //icon_dev[icon_current].modified = 3;
    // send event to callback URL
    prepare_json_event(&icon_dev[icon_current]);
    need_200 = true;
    send_json_info();
  }
}

// callback with a buffer of characters that have arrived on the UART
void ICACHE_FLASH_ATTR icon_recv(char *buf, int length) 
{
  uint32_t icon_crc;
  int i;

  i = 0;
  while(i<length && icon_len < UART_BUF_LEN)
  {
    if(buf[i]==0xCB) icon_len = 0;
    icon_buf.uart[icon_len++] = buf[i++];
  }
  // packet is longer than minimal, has Start and Stop, comes from requested Address + Command,
  if(icon_len>=8 && icon_buf.uart[icon_len-1] == 0xCE && icon_buf.uart[0] == 0xCB 
    && icon_buf.uart[1]==icon_adr && icon_buf.uart[2]==icon_cmd)
  {
    os_timer_disarm(&icon_timer); // Disarm iCON timer
    NODE_DBG("UART got %d bytes\n",length);
    // check CRC
    icon_crc = calc_crc(&icon_buf.uart[1], icon_len-2-3); // exclude Start/Stop and CRC itself
    if(icon_crc == icon_buf.uart[icon_len-2] + icon_buf.uart[icon_len-3]*10 + icon_buf.uart[icon_len-4]*100)
    {
      // good CRC -- decode packet
      NODE_DBG("CRC is good\n");
      icon_dev[icon_current].err = icon_buf.uart[icon_len-5];
      icon_dev[icon_current].num_timeout = 0;
      icon_dev[icon_current].skip = 0;
      switch(icon_state)
      {
        case ICS_IDLE: break;
        case ICS_SCAN: 
          icon_scan_reply();
          break;
        case ICS_EVENT:
          // check for new event
          if(icon_len == sizeof(event_empty)) icon_event_short();
            else icon_event_long();
          break;
        case ICS_DELETE:
          if(icon_buf.uart[icon_len-5] != 0xE0) icon_next_poll();
          else
          {
            NODE_DBG("Deleted event for #%d\n",icon_adr);
            // event deleted - pull another event (from same device)
            icon_dev[icon_current].must_delete = false;
            icon_send_ping();
          }
          break;
        case ICS_CMD:
          NODE_DBG("Reply from #%d for (%02X)\n",icon_adr,icon_cmd);
          json_len = os_sprintf(udp_reply,"{\"convertor\": %02X%02X%02X, \"response\":{\"id\":%d, \"c\":%d, \"e\":%d, \"d\":",
            flashConfig.convertor[0], flashConfig.convertor[1], flashConfig.convertor[2], icon_adr, icon_cmd, 
            icon_buf.uart[icon_len-5] & 15
          );
          for(i=3;i<icon_len-5;i++) json_len += os_sprintf(&udp_reply[json_len],"%02X",icon_buf.uart[i]);
          json_len += os_sprintf(&udp_reply[json_len],"}}");
          need_200 = false;
          send_json_info();
          break;
      }
    }
    else if(icon_state == ICS_SCAN) icon_scan_next();
	}
}

// called on receive timeout
void ICACHE_FLASH_ATTR icon_timerfunc(void)
{
  NODE_DBG("iCON bus timeout\n");
  switch(icon_state)
  {
    case ICS_IDLE:
      break;
    case ICS_SCAN: icon_scan_next();
      {
        // no response - move to next device
      }
      break;
    case ICS_EVENT:
    case ICS_DELETE:
      // no response - move to next device
      if(icon_dev[icon_current].num_timeout < 100) icon_dev[icon_current].num_timeout++;
      icon_dev[icon_current].skip = icon_dev[icon_current].num_timeout;
      icon_next_poll();
      break;
    case ICS_CMD:
      need_200 = false;
      json_len = os_sprintf(udp_reply, "{\"id\": %d, \"c\": \"%02X\", \"e\":20}", icon_adr, icon_cmd); // timeout
      send_json_info();
      break;
  }
}

void ICACHE_FLASH_ATTR icon_must_delete(void)
{
  icon_dev[icon_current].must_delete = true;
}

// send specified command to specified controller + any additional data
void ICACHE_FLASH_ATTR icon_send_cmd(void)
{
  uint8_t crc;
  int len;

  if(bridge_active) espconn_recv_unhold(connData[0].conn);
  else
  {
    // prepare ICON packet
    icon_buf.uart[0] = 0xCB;
    icon_buf.uart[1] = icon_adr;
    icon_buf.uart[2] = icon_cmd;
    if(icon_data_len) os_memcpy(&icon_buf.uart[3], icon_data, icon_data_len);       
    crc = calc_crc(&icon_buf.uart[1], 2+icon_data_len);
    len = icon_data_len + 3;
    icon_buf.uart[len++] = (crc / 100);
    icon_buf.uart[len++] = ((crc % 100) / 10);
    icon_buf.uart[len++] = (crc % 10);
    icon_buf.uart[len] = 0xCE;
    icon_data_len = 0;
    uart0_tx_buffer(icon_buf.uart, len);
    os_timer_arm(&icon_timer, ICON_INTERVAL, 0); // Arm iCON timer, 200msec, once
    NODE_DBG("Sent cmd %02X to %d\n",icon_cmd,icon_adr);
  }
}

// poll the current controller 
void ICACHE_FLASH_ATTR icon_send_ping(void)
{
  if(icon_dev[icon_current].skip)
  {
    icon_dev[icon_current].skip--;
    icon_next_poll();
  }
  else
  {
    // if device has timed-out many times - poll it more seldom
    icon_dev[icon_current].skip = icon_dev[icon_current].num_timeout;
    if(cmd_wait)
    {
      // there is a command from SDK, waiting to be executed
      cmd_wait = false;
      icon_cmd = saved_cmd;
      icon_adr = saved_adr;
      icon_state = ICS_CMD;
      icon_send_cmd();
    }
    else
    {
      // device priority is 2-bit counter, when counted to zero - device is allowed to be polled
      icon_dev[icon_current].priority = (icon_dev[icon_current].priority - 1) & 3;
      if(icon_dev[icon_current].priority) icon_next_poll();
      {
        icon_dev[icon_current].priority = icon_dev[icon_current].def_priority;
        if(icon_dev[icon_current].must_delete)
        {
          icon_cmd = 0xDA;
          icon_state = ICS_DELETE;
        }
        else
        {
          icon_cmd = 0xFA;
          icon_state = ICS_EVENT;
        }
        icon_adr = icon_dev[icon_current].adr;
        icon_send_cmd();
      }
    }
  }
}

// move the polling to next controller
void ICACHE_FLASH_ATTR icon_next_poll(void)
{
  icon_current++;
  if(icon_current >= icon_count) icon_current = 0;
  icon_send_ping();
}

// start polling controllers for new events
void ICACHE_FLASH_ATTR icon_start_poll(void)
{
  icon_current = 0;
  if(icon_count) icon_send_ping();
    else icon_state = ICS_IDLE;
}

void ICACHE_FLASH_ATTR icon_scan_next(void)
{
  icon_scan_adr++;
  icon_send_scan();
}

// continue iCON-bus scanning
void ICACHE_FLASH_ATTR icon_send_scan(void)
{
START:
  if(icon_scan_adr == 255 || icon_scan_adr > icon_stop_adr) 
  {
    NODE_DBG("Bus scan finished\n");
    icon_start_poll();
  }
  else if(icon_scan_adr == 0xCB || icon_scan_adr == 0xCE)
  {
    icon_scan_adr++;
    goto START;
  }
  else
  {
    icon_cmd = 0xF0;
    icon_adr = icon_scan_adr;
    icon_data_len = 0;
    icon_state = ICS_SCAN;
    icon_send_cmd();
  }
}
