#include "espmissingincludes.h"
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "config.h"
#include "user_interface.h"
#include "relay.h"

relay_info relay_data[4];

uint32_t pin_mux[GPIO_PIN_COUNT] = {
  PAD_XPD_DCDC_CONF,  
  PERIPHS_IO_MUX_GPIO5_U,  
  PERIPHS_IO_MUX_GPIO4_U, 	 
  PERIPHS_IO_MUX_GPIO0_U,
	PERIPHS_IO_MUX_GPIO2_U, 
	PERIPHS_IO_MUX_MTMS_U, 
	PERIPHS_IO_MUX_MTDI_U, 
	PERIPHS_IO_MUX_MTCK_U,
	PERIPHS_IO_MUX_MTDO_U, 
	PERIPHS_IO_MUX_U0RXD_U, 
	PERIPHS_IO_MUX_U0TXD_U, 
	PERIPHS_IO_MUX_SD_DATA2_U,
	PERIPHS_IO_MUX_SD_DATA3_U 
	};

uint8_t pin_num[GPIO_PIN_COUNT] = {
  16, 
  5, 
  4, 
  0,
	2,  
	14,  
	12, 
	13,
	15,  
	3,  
	1, 
	9,
	10
	};

uint8_t pin_func[GPIO_PIN_COUNT] = {
  0,
  FUNC_GPIO5, 
  FUNC_GPIO4, 
  FUNC_GPIO0,
	FUNC_GPIO2,  
	FUNC_GPIO14,  
	FUNC_GPIO12,  
	FUNC_GPIO13,
	FUNC_GPIO15,  
	FUNC_GPIO3,  
	FUNC_GPIO1, 
	FUNC_GPIO9,
	FUNC_GPIO10
	}; 

void ICACHE_FLASH_ATTR gpio16_output_conf(void)
{
    WRITE_PERI_REG(PAD_XPD_DCDC_CONF,
                   (READ_PERI_REG(PAD_XPD_DCDC_CONF) & 0xffffffbc) | (uint32)0x1); 	// mux configuration for XPD_DCDC to output rtc_gpio0

    WRITE_PERI_REG(RTC_GPIO_CONF,
                   (READ_PERI_REG(RTC_GPIO_CONF) & (uint32)0xfffffffe) | (uint32)0x0);	//mux configuration for out enable

    WRITE_PERI_REG(RTC_GPIO_ENABLE,
                   (READ_PERI_REG(RTC_GPIO_ENABLE) & (uint32)0xfffffffe) | (uint32)0x1);	//out enable
}

void ICACHE_FLASH_ATTR gpio16_output_set(uint8 value)
{
    WRITE_PERI_REG(RTC_GPIO_OUT,
                   (READ_PERI_REG(RTC_GPIO_OUT) & (uint32)0xfffffffe) | (uint32)(value & 1));
}

void ICACHE_FLASH_ATTR gpio16_input_conf(void)
{
    WRITE_PERI_REG(PAD_XPD_DCDC_CONF,
                   (READ_PERI_REG(PAD_XPD_DCDC_CONF) & 0xffffffbc) | (uint32)0x1); 	// mux configuration for XPD_DCDC and rtc_gpio0 connection

    WRITE_PERI_REG(RTC_GPIO_CONF,
                   (READ_PERI_REG(RTC_GPIO_CONF) & (uint32)0xfffffffe) | (uint32)0x0);	//mux configuration for out enable

    WRITE_PERI_REG(RTC_GPIO_ENABLE,
                   READ_PERI_REG(RTC_GPIO_ENABLE) & (uint32)0xfffffffe);	//out disable
}
 
// GPIO functions
int ICACHE_FLASH_ATTR platform_gpio_mode( unsigned pin, unsigned mode, unsigned pull )
{
  // NODE_DBG("Function platform_gpio_mode() is called. pin_mux:%d, func:%d",pin_mux[pin],pin_func[pin]);
  if (pin >= GPIO_PIN_COUNT)
    return -1;
  if(pin == 0){
    if(mode==PLATFORM_GPIO_INPUT)
      gpio16_input_conf();
    else
      gpio16_output_conf();
    return 1;
  }

  switch(pull){
    case PLATFORM_GPIO_PULLUP:
      PIN_PULLDWN_DIS(pin_mux[pin]);
      PIN_PULLUP_EN(pin_mux[pin]);
      break;
    case PLATFORM_GPIO_PULLDOWN:
      PIN_PULLUP_DIS(pin_mux[pin]);
      PIN_PULLDWN_EN(pin_mux[pin]);
      break;
    case PLATFORM_GPIO_FLOAT:
      PIN_PULLUP_DIS(pin_mux[pin]);
      PIN_PULLDWN_DIS(pin_mux[pin]);
      break;
    default:
      PIN_PULLUP_DIS(pin_mux[pin]);
      PIN_PULLDWN_DIS(pin_mux[pin]);
      break;
  }

  switch(mode){
    case PLATFORM_GPIO_INPUT:
      GPIO_DIS_OUTPUT(pin_num[pin]);
    case PLATFORM_GPIO_OUTPUT:
      ETS_GPIO_INTR_DISABLE();
      PIN_FUNC_SELECT(pin_mux[pin], pin_func[pin]);
      //disable interrupt
      gpio_pin_intr_state_set(GPIO_ID_PIN(pin_num[pin]), GPIO_PIN_INTR_DISABLE);
      //clear interrupt status
      GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(pin_num[pin]));
      GPIO_REG_WRITE(GPIO_PIN_ADDR(GPIO_ID_PIN(pin_num[pin])), 
        GPIO_REG_READ(GPIO_PIN_ADDR(GPIO_ID_PIN(pin_num[pin]))) & (~ GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_ENABLE))); //disable open drain; 
      ETS_GPIO_INTR_ENABLE();
      break;
    default:
      break;
  }
  return 1;
}

int ICACHE_FLASH_ATTR platform_gpio_write( unsigned pin, unsigned level )
{
  NODE_DBG("platform_gpio_write() - pin:%d, level:%d\n",GPIO_ID_PIN(pin_num[pin]),level);
  if (pin >= GPIO_PIN_COUNT)
    return -1;
  if(pin == 0)
  {
    gpio16_output_conf();
    gpio16_output_set(level);
    return 1;
  }
  GPIO_OUTPUT_SET(GPIO_ID_PIN(pin_num[pin]), level);
  return 0;
} 

int ICACHE_FLASH_ATTR relay_get_state(int relayNumber){

  if(relayNumber>=0 && relayNumber<4)
  {
  	return relay_data[relayNumber].state;
  }
  else return -1;
}

int ICACHE_FLASH_ATTR relay_set_state(int relayNumber,unsigned state){

  if(relayNumber>=0 && relayNumber<4)
  {
  	relay_data[relayNumber].state = state;
  	NODE_DBG("Relay %d, new state %d\n",flashConfig.rele_pin[relayNumber],state);
    platform_gpio_mode(flashConfig.rele_pin[relayNumber],PLATFORM_GPIO_OUTPUT,PLATFORM_GPIO_FLOAT);     
  	platform_gpio_write(flashConfig.rele_pin[relayNumber],state); 
  	return state;
	} 
	else return -1;
}

int ICACHE_FLASH_ATTR relay_toggle_state(int relayNumber){

  if(relayNumber>=0 && relayNumber<4)
  {
  	relay_data[relayNumber].state = (relay_data[relayNumber].state ^ 1);
  	NODE_DBG("Relay %d, new state %d\n",flashConfig.rele_pin[relayNumber],relay_data[relayNumber].state);
    platform_gpio_mode(flashConfig.rele_pin[relayNumber],PLATFORM_GPIO_OUTPUT,PLATFORM_GPIO_FLOAT);     
  	platform_gpio_write(flashConfig.rele_pin[relayNumber],relay_data[relayNumber].state); 
  	return relay_data[relayNumber].state;
	} 
	else return -1;
}

void ICACHE_FLASH_ATTR relay_init(){
  int i;
  
	NODE_DBG("Relay init\n");
  for(i=0;i<4;i++) relay_set_state(i,relay_data[i].state);
}
