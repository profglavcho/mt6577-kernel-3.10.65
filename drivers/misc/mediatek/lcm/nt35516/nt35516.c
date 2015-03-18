//#define LCM_DEBUG

#ifdef BUILD_LK
	//#include <platform/disp_drv_platform.h>
	#include <platform/mt_gpio.h>
    #ifdef LCD_DEBUG
        #define LCM_DEBUG(format, ...)   printf("uboot nt35516" format "\n", ## __VA_ARGS__)
    #else
        #define LCM_DEBUG(format, ...)
    #endif
#elif defined(BUILD_UBOOT)
	#include <asm/arch/disp_drv_platform.h>
	#include <asm/arch/mt6577_gpio.h>
    #ifdef LCD_DEBUG
        #define LCM_DEBUG(format, ...)   printf("uboot nt35516" format "\n", ## __VA_ARGS__)
    #else
        #define LCM_DEBUG(format, ...)
    #endif
#else
    #include <linux/string.h>
    //#include <linux/delay.h>
	#include <mach/mt6577_gpio.h>
    #ifdef LCD_DEBUG
        #define LCM_DEBUG(format, ...)   printk("kernel nt35516" format "\n", ## __VA_ARGS__)
    #else
        #define LCM_DEBUG(format, ...)
    #endif
#endif

#include "lcm_drv.h"

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  										(540)
#define FRAME_HEIGHT 										(960)

#define REGFLAG_DELAY             							0XFE
#define REGFLAG_END_OF_TABLE      							0xFF   // END OF REGISTERS MARKER

#define LCM_DSI_CMD_MODE									1

#define LCD_LDO2V8_GPIO_PIN          						(12)

#define LCM_ID       										(0x00)

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------
static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))
#define SET_GPIO_OUT(n, v)	        						(lcm_util.set_gpio_out((n), (v)))

#define UDELAY(n)	                						(lcm_util.udelay(n))
#define MDELAY(n)	                						(lcm_util.mdelay(n))



// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)									lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)				lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg											lcm_util.dsi_read_reg()
// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type   = LCM_TYPE_DSI;

	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	// enable tearing-free
	params->dbi.te_mode 				= LCM_DBI_TE_MODE_VSYNC_ONLY;
	params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode   = CMD_MODE;
#else
	params->dsi.mode   = SYNC_PULSE_VDO_MODE;
#endif

	// DSI
	/* Command mode setting */
	params->dsi.LANE_NUM				= LCM_TWO_LANE;
	//The following defined the fomat for data coming from LCD engine.
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

	// Highly depends on LCD driver capability.
	// Not support in MT6573
	params->dsi.packet_size=256;

	// Video mode setting
	params->dsi.intermediat_buffer_num = 2;

	params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.word_count=480*3;

	params->dsi.vertical_sync_active=3;
	params->dsi.vertical_backporch=12;
	params->dsi.vertical_frontporch=2;
	params->dsi.vertical_active_line=800;

	params->dsi.line_byte=2048;		// 2256 = 752*3
	params->dsi.horizontal_sync_active_byte=26;
	params->dsi.horizontal_backporch_byte=146;
	params->dsi.horizontal_frontporch_byte=146;
	params->dsi.rgb_byte=(480*3+6);

	params->dsi.horizontal_sync_active_word_count=20;
	params->dsi.horizontal_backporch_word_count=140;
	params->dsi.horizontal_frontporch_word_count=140;

	// Bit rate calculation
	params->dsi.pll_div1=38;		// fref=26MHz, fvco=fref*(div1+1)	(div1=0~63, fvco=500MHZ~1GHz)
	params->dsi.pll_div2=1;			// div2=0~15: fout=fvo/(2*div2)
}

static void lcm_init(void)
{
	LCM_DEBUG("nt35516 lcm_init begin-----------\n");

	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(50);

	unsigned int data_array[16];

	data_array[0]=0x00053902;
	data_array[1]=0x2555aaff;
	data_array[2]=0x00000001;
	dsi_set_cmdq(&data_array,3,1);

	data_array[0]=0x00053902;
	data_array[1]=0x2555aaff;
	data_array[2]=0x00000001;
	dsi_set_cmdq(&data_array,3,1);

	data_array[0]=0x00073902;
	data_array[1]=0x070302f3;
	data_array[2]=0x00d48844;
	dsi_set_cmdq(&data_array,3,1);

	data_array[0]=0x00063902;
	data_array[1]=0x52aa55f0;
	data_array[2]=0x00000008;
	dsi_set_cmdq(&data_array,3,1);

	//data_array[0]=0x00023902;
	//data_array[1]=0x0000ecB1;
	//dsi_set_cmdq(&data_array,2,1);

	data_array[0]=0x00043902;
	data_array[1]=0x020202BC;
	dsi_set_cmdq(&data_array,2,1);

	data_array[0]=0x00063902;
	data_array[1]=0x52aa55f0;
	data_array[2]=0x00000108;
	dsi_set_cmdq(&data_array,3,1);

	data_array[0]=0x00023902;
	data_array[1]=0x000051Be;
	dsi_set_cmdq(&data_array,2,1);

	data_array[0]=0x00053902;
	data_array[1]=0x100f0fD0;
	data_array[2]=0x00000010;
	dsi_set_cmdq(&data_array,3,1);

	data_array[0]=0x00043902;
	data_array[1]=0x006c00bc;
	dsi_set_cmdq(&data_array,2,1);

	data_array[0]=0x00043902;
	data_array[1]=0x006c00bd;
	dsi_set_cmdq(&data_array,2,1);

	data_array[0]=0x00113902;
	data_array[1]=0x003700d1;
	data_array[2]=0x0072004f;
	data_array[3]=0x00a30089;
	data_array[4]=0x01e400c9;
	data_array[5]=0x00000014;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00113902;
	data_array[1]=0x013d01d2;
	data_array[2]=0x01af017d;
	data_array[3]=0x023c02fd;
	data_array[4]=0x0277023d;
	data_array[5]=0x000000b3;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00113902;
	data_array[1]=0x03db02d3;
	data_array[2]=0x0325030f;
	data_array[3]=0x0377035c;
	data_array[4]=0x039f0394;
	data_array[5]=0x000000ac;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00053902;
	data_array[1]=0x03ba03D4;
	data_array[2]=0x000000c1;
	dsi_set_cmdq(&data_array,3,1);

	data_array[0]=0x00113902;
	data_array[1]=0x003700d5;
	data_array[2]=0x0072004f;
	data_array[3]=0x00a30089;
	data_array[4]=0x01e400c9;
	data_array[5]=0x00000014;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00113902;
	data_array[1]=0x013d01d6;
	data_array[2]=0x01af017d;
	data_array[3]=0x023c02fd;
	data_array[4]=0x0277023d;
	data_array[5]=0x000000b3;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00113902;
	data_array[1]=0x03db02d7;
	data_array[2]=0x0325030f;
	data_array[3]=0x0377035c;
	data_array[4]=0x039f0394;
	data_array[5]=0x000000ac;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00053902;
	data_array[1]=0x03ba03D8;
	data_array[2]=0x000000c1;
	dsi_set_cmdq(&data_array,3,1);

	data_array[0]=0x00113902;
	data_array[1]=0x003700d9;
	data_array[2]=0x0072004f;
	data_array[3]=0x00a30089;
	data_array[4]=0x01e400c9;
	data_array[5]=0x00000014;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00113902;
	data_array[1]=0x013d01dd;
	data_array[2]=0x01af017d;
	data_array[3]=0x023c02fd;
	data_array[4]=0x0277023d;
	data_array[5]=0x000000b3;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00113902;
	data_array[1]=0x03db02de;
	data_array[2]=0x0325030f;
	data_array[3]=0x0377035c;
	data_array[4]=0x039f0394;
	data_array[5]=0x000000ac;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00053902;
	data_array[1]=0x03ba03Df;
	data_array[2]=0x000000c1;
	dsi_set_cmdq(&data_array,3,1);

	data_array[0]=0x00113902;
	data_array[1]=0x003700e0;
	data_array[2]=0x0072004f;
	data_array[3]=0x00a30089;
	data_array[4]=0x01e400c9;
	data_array[5]=0x00000014;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00113902;
	data_array[1]=0x013d01e1;
	data_array[2]=0x01af017d;
	data_array[3]=0x023c02fd;
	data_array[4]=0x0277023d;
	data_array[5]=0x000000b3;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00113902;
	data_array[1]=0x03db02e2;
	data_array[2]=0x0325030f;
	data_array[3]=0x0377035c;
	data_array[4]=0x039f0394;
	data_array[5]=0x000000ac;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00053902;
	data_array[1]=0x03ba03e3;
	data_array[2]=0x000000c1;
	dsi_set_cmdq(&data_array,3,1);

	data_array[0]=0x00113902;
	data_array[1]=0x003700e4;
	data_array[2]=0x0072004f;
	data_array[3]=0x00a30089;
	data_array[4]=0x01e400c9;
	data_array[5]=0x00000014;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00113902;
	data_array[1]=0x013d01e5;
	data_array[2]=0x01af017d;
	data_array[3]=0x023c02fd;
	data_array[4]=0x0277023d;
	data_array[5]=0x000000b3;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00113902;
	data_array[1]=0x03db02e6;
	data_array[2]=0x0325030f;
	data_array[3]=0x0377035c;
	data_array[4]=0x039f0394;
	data_array[5]=0x000000ac;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00053902;
	data_array[1]=0x03ba03e7;
	data_array[2]=0x000000c1;
	dsi_set_cmdq(&data_array,3,1);

	data_array[0]=0x00113902;
	data_array[1]=0x003700e8;
	data_array[2]=0x0072004f;
	data_array[3]=0x00a30089;
	data_array[4]=0x01e400c9;
	data_array[5]=0x00000014;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00113902;
	data_array[1]=0x013d01e9;
	data_array[2]=0x01af017d;
	data_array[3]=0x023c02fd;
	data_array[4]=0x0277023d;
	data_array[5]=0x000000b3;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00113902;
	data_array[1]=0x03db02ea;
	data_array[2]=0x0325030f;
	data_array[3]=0x0377035c;
	data_array[4]=0x039f0394;
	data_array[5]=0x000000ac;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00053902;
	data_array[1]=0x03ba03eb;
	data_array[2]=0x000000c1;
	dsi_set_cmdq(&data_array,3,1);

	data_array[0]=0x00043902;
	data_array[1]=0x141414B3;
	dsi_set_cmdq(&data_array,2,1);

	data_array[0]=0x00043902;
	//data_array[1]=0x060606B4;
	data_array[1]=0x000000B4;
	dsi_set_cmdq(&data_array,2,1);

	data_array[0]=0x00043902;
	data_array[1]=0x030303B2;
	dsi_set_cmdq(&data_array,2,1);

	data_array[0]=0x00351500;
	dsi_set_cmdq(&data_array,1,1);

	data_array[0]=0x00110500;
	dsi_set_cmdq(&data_array,1,1);

	MDELAY(120);

	data_array[0]=0x00291500;
	dsi_set_cmdq(&data_array,1,1);

	data_array[0]=0x00043902;
	data_array[1]=0x060606B4;
	dsi_set_cmdq(&data_array,2,1);

	data_array[0]=0x00023902;
	data_array[1]=0x00002453;
	dsi_set_cmdq(&data_array,2,1);
	LCM_DEBUG("nt35516 lcm_init end-------------------\n");
}

void B4_set(void)
{
	unsigned int data_array[16];
	data_array[0]=0x00043902;
	data_array[1]=0x060606B4;
	dsi_set_cmdq(&data_array,2,1);
}

static void lcm_suspend(void)
{
	LCM_DEBUG("nt35516 lcm_suspend begin-------\n");

	unsigned int data_array[16];
	data_array[0]=0x00280500;
	dsi_set_cmdq(&data_array,1,1);

	data_array[0]=0x00100500;
	dsi_set_cmdq(&data_array,1,1);
	MDELAY(120);

	SET_RESET_PIN(0);
	MDELAY(100);
	SET_GPIO_OUT(LCD_LDO2V8_GPIO_PIN , 0);

	LCM_DEBUG("nt35516 lcm_suspend end----------\n");
}

static void lcm_resume(void)
{
	LCM_DEBUG("lcm_resume begin----------------\n");

	SET_GPIO_OUT(LCD_LDO2V8_GPIO_PIN , 1);
	MDELAY(80);

	lcm_init();
	B4_set();

	LCM_DEBUG("lcm_resume end------------------\n");
}

static void lcm_update(unsigned int x, unsigned int y,
                       unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0>>8)&0xFF);
	unsigned char x0_LSB = (x0&0xFF);
	unsigned char x1_MSB = ((x1>>8)&0xFF);
	unsigned char x1_LSB = (x1&0xFF);
	unsigned char y0_MSB = ((y0>>8)&0xFF);
	unsigned char y0_LSB = (y0&0xFF);
	unsigned char y1_MSB = ((y1>>8)&0xFF);
	unsigned char y1_LSB = (y1&0xFF);

	unsigned int data_array[16];

	data_array[0]= 0x00053902;
	data_array[1]= (x1_MSB<<24)|(x0_LSB<<16)|(x0_MSB<<8)|0x2a;
	data_array[2]= (x1_LSB);
	data_array[3]= 0x00053902;
	data_array[4]= (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
	data_array[5]= (y1_LSB);
	data_array[6]= 0x002c3909;

	dsi_set_cmdq(&data_array, 7, 0);
}

static void lcm_setbacklight(unsigned int level)
{
	//for LGE backlight IC mapping table
	if(level > 255)
			level = 255;
	// Refresh value of backlight level.
	unsigned int data_array[16];
	data_array[0]= 0x00023902;
	data_array[1] =(0x51|(level<<8));
	dsi_set_cmdq(data_array, 2, 1);
}

static unsigned int lcm_compare_id()
{
	unsigned int id=0;

   	mt_set_gpio_mode(50,0);  // gpio mode   high
	mt_set_gpio_pull_enable(50,0);
	mt_set_gpio_dir(50,0);  //input

	mt_set_gpio_mode(47,0);  // gpio mode   low
	mt_set_gpio_pull_enable(47,0);
	mt_set_gpio_dir(47,0);

	id = (mt_get_gpio_in(50) <<4) | mt_get_gpio_in(47);

#if defined(BUILD_LK) || defined(BUILD_UBOOT)
	printf("%s,first source id1 = 0x%08x\n", __func__, id);
#endif	

    return (LCM_ID == id) ? 1 : 0;
}

LCM_DRIVER nt35516_lcm_drv =
{
    .name			= "nt35516",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
    .compare_id     = lcm_compare_id,
#if (LCM_DSI_CMD_MODE)
    .set_backlight	= lcm_setbacklight,
    .update         = lcm_update
#endif
};
