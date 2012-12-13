/*
 * linux/arch/arm/mach-omap2/board-justin-camera.c
 *
 * Copyright (C) 2010 LG Electronics, Inc.
 * Copyright (C) 2007 Texas Instruments
 *
 * Modified from linux/arch/arm/mach-omap2/board-zoom-camera.c
 *
 * Initial code: Syed Mohammed Khasim
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include "mux.h"

#if defined(CONFIG_TWL4030_CORE) && defined(CONFIG_VIDEO_OMAP3)

#include <linux/i2c/twl.h>

#include <asm/io.h>

#include <mach/gpio.h>
#include <plat/omap-pm.h>

static int cam_inited;
#include <media/v4l2-int-device.h>
#include <../drivers/media/video/omap34xxcam.h>
#include <../drivers/media/video/isp/ispreg.h>

#if defined(CONFIG_REGULATOR_LP8720)
#include <linux/regulator/lp8720.h>

static struct lp8720_platform_data lp8720_pdata = {
	.en_gpio_num         = 37,
};
#endif

#define DEBUG_BASE		0x08000000

#define REG_SDP3430_FPGA_GPIO_2 (0x50)
#define FPGA_SPR_GPIO1_3v3	(0x1 << 14)
#define FPGA_GPIO6_DIR_CTRL	(0x1 << 6)

#define VAUX_2_8_V		0x09
#define VAUX_1_8_V		0x05

#define VAUX3_1_8_V		0x01
#define VAUX_DEV_GRP_P1		0x20
#define VAUX_DEV_GRP_NONE	0x00


#define VMMC1_DEV_GRP          0x27
#define VMMC1_DEV_GRP_P1       0x20
#define VMMC1_DEDICATED                0x2A
#define SET_VMMC1_V2_8 0x01


#define USE_XCLKA  	0
#define USE_XCLKB  	1

#define ISP_IMX072_MCLK		216000000

//--[[ LGE_UBIQUIX_MODIFIED_START : ymjun@mnbt.co.kr [2011.07.26] - CAM
#define ISP_MT9V113_MCLK    216000000
//--]] LGE_UBIQUIX_MODIFIED_END : ymjun@mnbt.co.kr [2011.07.26] - CAM

/* Sensor specific GPIO signals */
#define IMX072_RESET_GPIO  	98
#define IMX072_STANDBY_GPIO	58
#define SUBPM_ENABLE	37

//#define YACD5B1S_RESET_GPIO  	61
//#define YACD5B1S_STANDBY_GPIO    36

//--[[ LGE_UBIQUIX_MODIFIED_START : ymjun@mnbt.co.kr [2011.07.26] - CAM
#define MT9V113_RESET_GPIO    60
#define MT9V113_STANDBY_GPIO  36
//--]] LGE_UBIQUIX_MODIFIED_END : ymjun@mnbt.co.kr [2011.07.26] - CAM

#define DW9716_VCM_ENABLE 167
/* Flash enable/disable GPIO */
#define RT8515_FLINH_GPIO 		39
//--[[ LGE_UBIQUIX_MODIFIED_START : ymjun@mnbt.co.kr [2011.07.26] - CAM
#define RT8515_FLEN_GPIO  		102
#define RT8515_ENSET_S2C_GPIO  	157
//--]] LGE_UBIQUIX_MODIFIED_END : ymjun@mnbt.co.kr [2011.07.26] - CAM

#if defined(CONFIG_VIDEO_IMX072) || defined(CONFIG_VIDEO_IMX072_MODULE)
#include <media/imx072.h>
#include <../drivers/media/video/isp/ispcsi2.h>
#include <../drivers/media/video/dw9716.h>
#include <../drivers/media/video/rt8515.h>
#define IMX072_CSI2_CLOCK_POLARITY	0	/* +/- pin order */
#define IMX072_CSI2_DATA0_POLARITY	0	/* +/- pin order */
#define IMX072_CSI2_DATA1_POLARITY	0	/* +/- pin order */
#define IMX072_CSI2_CLOCK_LANE		1	 /* Clock lane position: 1 */
#define IMX072_CSI2_DATA0_LANE		2	 /* Data0 lane position: 2 */
#define IMX072_CSI2_DATA1_LANE		3	 /* Data1 lane position: 3 */
#define IMX072_CSI2_PHY_THS_TERM	2
#define IMX072_CSI2_PHY_THS_SETTLE	23
#define IMX072_CSI2_PHY_TCLK_TERM	0
#define IMX072_CSI2_PHY_TCLK_MISS	1
#define IMX072_CSI2_PHY_TCLK_SETTLE	14
#define IMX072_BIGGEST_FRAME_BYTE_SIZE	PAGE_ALIGN(2608 * 1960 * 2)
#endif

//--[[ LGE_UBIQUIX_MODIFIED_START : ymjun@mnbt.co.kr [2011.07.26] - CAM
#if defined(CONFIG_VIDEO_MT9V113) || defined(CONFIG_VIDEO_MT9V113_MODULE)
#include <../drivers/media/video/mt9v113.h>
#endif
//--]] LGE_UBIQUIX_MODIFIED_END : ymjun@mnbt.co.kr [2011.07.26] - CAM

#if defined(CONFIG_REGULATOR_LP8720)
extern void subpm_set_output(subpm_output_enum outnum, int onoff);
extern void subpm_output_enable(void);
#endif

//--[[ LGE_UBIQUIX_MODIFIED_START : ymjun@mnbt.co.kr [2012.05.22] - CAM
static struct pm_qos_request_list pm_qos_handler;
#define SET_MPU_CONSTRAINT         12
#define CLEAR_MPU_CONSTRAINT       -1
//--]] LGE_UBIQUIX_MODIFIED_END : ymjun@mnbt.co.kr [2012.05.22] - CAM

/* S[, 20120922, mannsik.chung@lge.com, PM from froyo. */
extern u32 doing_wakeup;
/* E], 20120922, mannsik.chung@lge.com, PM from froyo. */

#if defined(CONFIG_VIDEO_IMX072) || defined(CONFIG_VIDEO_IMX072_MODULE)

static struct omap34xxcam_sensor_config imx072_hwc = {
	.sensor_isp  = 0,
	.capture_mem = IMX072_BIGGEST_FRAME_BYTE_SIZE * 2,
	.ival_default	= { 1, 10 },
	.isp_if = ISP_CSIA,
};

static int imx072_sensor_set_prv_data(struct v4l2_int_device *s, void *priv)
{
	struct omap34xxcam_hw_config *hwc = priv;

	hwc->u.sensor		= imx072_hwc;
	hwc->dev_index		= 2;
	hwc->dev_minor		= 5;
	hwc->dev_type		= OMAP34XXCAM_SLAVE_SENSOR;

	return 0;
}

static struct isp_interface_config imx072_if_config = {
	.ccdc_par_ser 		= ISP_CSIA,
	.dataline_shift 	= 0x0,
	.hsvs_syncdetect 	= ISPCTRL_SYNC_DETECT_VSRISE,
	.strobe 		= 0x0,
	.prestrobe 		= 0x0,
	.shutter 		= 0x0,
	.wenlog 		= ISPCCDC_CFG_WENLOG_AND,
	.wait_hs_vs		= 0,
	.cam_mclk		= ISP_IMX072_MCLK,
	.raw_fmt_in		= ISPCCDC_INPUT_FMT_RG_GB,
	.u.csi.crc 		= 0x0,
	.u.csi.mode 		= 0x0,
	.u.csi.edge 		= 0x0,
	.u.csi.signalling 	= 0x0,
	.u.csi.strobe_clock_inv = 0x0,
	.u.csi.vs_edge 		= 0x0,
	.u.csi.channel 		= 0x0,
	.u.csi.vpclk 		= 0x2,
	.u.csi.data_start 	= 0x0,
	.u.csi.data_size 	= 0x0,
	.u.csi.format 		= V4L2_PIX_FMT_SGRBG10,
};


static int imx072_sensor_power_set(struct v4l2_int_device *s, enum v4l2_power power)
{
	struct omap34xxcam_videodev *vdev = s->u.slave->master->priv;
	struct isp_device *isp = dev_get_drvdata(vdev->cam->isp);
	struct isp_csi2_lanes_cfg lanecfg;
	struct isp_csi2_phy_cfg phyconfig;
	static enum v4l2_power previous_power = V4L2_POWER_OFF;
	int err = 0;

	switch (power) {
	case V4L2_POWER_ON:		
		/* Power Up Sequence */
		printk(KERN_DEBUG "imx072_sensor_power_set(ON)\n");

		/*
		 * Through-put requirement:
		 * Set max OCP freq for 3630 is 200 MHz through-put
		 * is in KByte/s so 200000 KHz * 4 = 800000 KByte/s
		 */
		omap_pm_set_min_bus_tput(vdev->cam->isp,
					 OCP_INITIATOR_AGENT, 800000);

/* S[, 20120922, mannsik.chung@lge.com, PM from froyo. */
		doing_wakeup = 1;
/* E], 20120922, mannsik.chung@lge.com, PM from froyo. */

		/* Hold a constraint to keep MPU in C1 */
		//--[[ LGE_UBIQUIX_MODIFIED_START : ymjun@mnbt.co.kr [2012.05.22] - CAM
		pm_qos_update_request(&pm_qos_handler,
							SET_MPU_CONSTRAINT);
		//--]] LGE_UBIQUIX_MODIFIED_END : ymjun@mnbt.co.kr [2012.05.22] - CAM

		isp_csi2_reset(&isp->isp_csi2);

		lanecfg.clk.pol = IMX072_CSI2_CLOCK_POLARITY;
		lanecfg.clk.pos = IMX072_CSI2_CLOCK_LANE;
		lanecfg.data[0].pol = IMX072_CSI2_DATA0_POLARITY;
		lanecfg.data[0].pos = IMX072_CSI2_DATA0_LANE;
		lanecfg.data[1].pol = IMX072_CSI2_DATA1_POLARITY;
		lanecfg.data[1].pos = IMX072_CSI2_DATA1_LANE;
		lanecfg.data[2].pol = 0;
		lanecfg.data[2].pos = 0;
		lanecfg.data[3].pol = 0;
		lanecfg.data[3].pos = 0;
		isp_csi2_complexio_lanes_config(&isp->isp_csi2, &lanecfg);
		isp_csi2_complexio_lanes_update(&isp->isp_csi2, true);

		isp_csi2_ctrl_config_ecc_enable(&isp->isp_csi2, true);

		phyconfig.ths_term = IMX072_CSI2_PHY_THS_TERM;
		phyconfig.ths_settle = IMX072_CSI2_PHY_THS_SETTLE;
		phyconfig.tclk_term = IMX072_CSI2_PHY_TCLK_TERM;
		phyconfig.tclk_miss = IMX072_CSI2_PHY_TCLK_MISS;
		phyconfig.tclk_settle = IMX072_CSI2_PHY_TCLK_SETTLE;
		isp_csi2_phy_config(&isp->isp_csi2, &phyconfig);
		isp_csi2_phy_update(&isp->isp_csi2, true);

		isp_configure_interface(vdev->cam->isp, &imx072_if_config);

		/* Request and configure gpio pins */
		if (gpio_request(IMX072_RESET_GPIO, "imx072_rst") != 0)
			return -EIO;

//--[[ LGE_UBIQUIX_MODIFIED_START : ymjun@mnbt.co.kr [2011.07.26] - CAM
		//KJK for HDMI Sub PMIC usage 2011.03.10
		//extern void lp8720_reinit();
		//lp8720_reinit();
    
		//gpio_direction_output(lp8720_pdata.en_gpio_num, 1);
		//subpm_lp8720_power_on();
//--]] LGE_UBIQUIX_MODIFIED_END : ymjun@mnbt.co.kr [2011.07.26] - CAM

		subpm_set_output(SWREG,1);
		subpm_output_enable();
		subpm_set_output(LDO4,1);
		subpm_output_enable();
		subpm_set_output(LDO3,1);
		subpm_output_enable();

		twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER,
					VAUX_1_8_V, TWL4030_VAUX4_DEDICATED);
		twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER,
					VAUX_DEV_GRP_P1, TWL4030_VAUX4_DEV_GRP);
	
		udelay(100);
		
		/* have to put sensor to reset to guarantee detection */
		gpio_direction_output(IMX072_RESET_GPIO, 1);
//		gpio_set_value(IMX072_RESET_GPIO, 1);
		udelay(100);
			
//		gpio_direction_output(IMX072_RESET_GPIO, 0);
		gpio_set_value(IMX072_RESET_GPIO, 0);

		udelay(1500);

		/* nRESET is active LOW. set HIGH to release reset */
//		gpio_direction_output(IMX072_RESET_GPIO, 1);
		gpio_set_value(IMX072_RESET_GPIO, 1);
		udelay(300);

		break;
	case V4L2_POWER_OFF:
//--[[ LGE_UBIQUIX_MODIFIED_START : ymjun@mnbt.co.kr [2011.07.26] - CAM
		printk(KERN_DEBUG "imx072_sensor_power_set(OFF)\n");
//--]] LGE_UBIQUIX_MODIFIED_END : ymjun@mnbt.co.kr [2011.07.26] - CAM
		subpm_set_output(LDO5,0);
		subpm_output_enable();

//--[[ LGE_UBIQUIX_MODIFIED_START : ymjun@mnbt.co.kr [2011.07.26] - CAM
		//KJK for HDMI Sub PMIC usage 2011.03.10
		//gpio_direction_output(lp8720_pdata.en_gpio_num, 0);
		//subpm_lp8720_power_off();
//--]] LGE_UBIQUIX_MODIFIED_END : ymjun@mnbt.co.kr [2011.07.26] - CAM

        gpio_set_value(DW9716_VCM_ENABLE, 0);

//--[[ LGE_UBIQUIX_MODIFIED_START : ymjun@mnbt.co.kr [2011.07.26] - CAM
		/* This is not required cuz it is disabled in isp_csi_memvs.c*/
		/* isp_disable_mclk(isp); */
//--]] LGE_UBIQUIX_MODIFIED_END : ymjun@mnbt.co.kr [2011.07.26] - CAM

        isp_csi2_complexio_power(&isp->isp_csi2, ISP_CSI2_POWER_OFF);
        isp_csi2_reset(&isp->isp_csi2);
        isp_csi2_ctrl_config_ecc_enable(&isp->isp_csi2, true);
        isp_csi2_complexio_power(&isp->isp_csi2, ISP_CSI2_POWER_OFF);
		break;	
		
	case V4L2_POWER_STANDBY:
//--[[ LGE_UBIQUIX_MODIFIED_START : ymjun@mnbt.co.kr [2011.07.26] - CAM
  #ifdef CONFIG_OMAP2_DSS_HDMI    
  {
    extern int hdmi_power_initialize;
    int hdmi_wait_cnt = 0;
    for(hdmi_wait_cnt = 0; hdmi_wait_cnt<20; hdmi_wait_cnt++)
    {
      if (hdmi_power_initialize== 0)
      {
        printk("==========================================================\n");
        printk("waiting DONE for HDMI : %d looping \n", hdmi_wait_cnt);
        printk("==========================================================\n");
        break;
      }
      printk("==========================================================\n");
      printk("HDMI is not done yet...waiting for HDMI DONE : %d looping \n", hdmi_wait_cnt);
      printk("==========================================================\n");
      msleep(100);
    }
  }
  #endif
//--]] LGE_UBIQUIX_MODIFIED_END : ymjun@mnbt.co.kr [2011.07.26] - CAM

		printk(KERN_DEBUG "imx072_sensor_power_set(%s)\n",
			(power == V4L2_POWER_OFF) ? "OFF" : "STANDBY");
		/* Power Down Sequence */
		isp_csi2_complexio_power(&isp->isp_csi2, ISP_CSI2_POWER_OFF);

		twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER,
				VAUX_DEV_GRP_NONE, TWL4030_VAUX4_DEV_GRP);
		gpio_set_value(IMX072_RESET_GPIO, 0);
		udelay(5);
		subpm_set_output(LDO3,0);
		subpm_output_enable();
		subpm_set_output(LDO4,0);
		subpm_output_enable();
		subpm_set_output(SWREG,0);
		subpm_output_enable();
		gpio_free(IMX072_RESET_GPIO);

/* S[, 20120922, mannsik.chung@lge.com, PM from froyo. */
		doing_wakeup = 0;
/* E], 20120922, mannsik.chung@lge.com, PM from froyo. */

		/* Remove pm constraints */
		//--[[ LGE_UBIQUIX_MODIFIED_START : ymjun@mnbt.co.kr [2012.05.22] - CAM		
		omap_pm_set_min_bus_tput(vdev->cam->isp, OCP_INITIATOR_AGENT, -1);
		pm_qos_update_request(&pm_qos_handler,
							CLEAR_MPU_CONSTRAINT);
		//--]] LGE_UBIQUIX_MODIFIED_END : ymjun@mnbt.co.kr [2012.05.22] - CAM	

		break;
	}

	/* Save powerstate to know what was before calling POWER_ON. */
	previous_power = power;
	return err;
}

static u32 imx072_sensor_set_xclk(struct v4l2_int_device *s, u32 xclkfreq)
{
	struct omap34xxcam_videodev *vdev = s->u.slave->master->priv;
	struct isp_device *isp = dev_get_drvdata(vdev->cam->isp);

	/* Enable/Disable sensor xclk */
	if (xclkfreq)
		isp_enable_mclk(isp->dev);
	else
		isp_disable_mclk(isp);


	return isp_set_xclk(vdev->cam->isp, xclkfreq, USE_XCLKA);
}

static int imx072_csi2_lane_count(struct v4l2_int_device *s, int count)
{
	struct omap34xxcam_videodev *vdev = s->u.slave->master->priv;
	struct isp_device *isp = dev_get_drvdata(vdev->cam->isp);

	return isp_csi2_complexio_lanes_count(&isp->isp_csi2, count);
}

static int imx072_csi2_cfg_vp_out_ctrl(struct v4l2_int_device *s,
				       u8 vp_out_ctrl)
{
	struct omap34xxcam_videodev *vdev = s->u.slave->master->priv;
	struct isp_device *isp = dev_get_drvdata(vdev->cam->isp);

	return isp_csi2_ctrl_config_vp_out_ctrl(&isp->isp_csi2, vp_out_ctrl);
}

static int imx072_csi2_ctrl_update(struct v4l2_int_device *s, bool force_update)
{
	struct omap34xxcam_videodev *vdev = s->u.slave->master->priv;
	struct isp_device *isp = dev_get_drvdata(vdev->cam->isp);

	return isp_csi2_ctrl_update(&isp->isp_csi2, force_update);
}

static int imx072_csi2_cfg_virtual_id(struct v4l2_int_device *s, u8 ctx, u8 id)
{
	struct omap34xxcam_videodev *vdev = s->u.slave->master->priv;
	struct isp_device *isp = dev_get_drvdata(vdev->cam->isp);

	return isp_csi2_ctx_config_virtual_id(&isp->isp_csi2, ctx, id);
}

static int imx072_csi2_ctx_update(struct v4l2_int_device *s, u8 ctx,
				  bool force_update)
{
	struct omap34xxcam_videodev *vdev = s->u.slave->master->priv;
	struct isp_device *isp = dev_get_drvdata(vdev->cam->isp);

	return isp_csi2_ctx_update(&isp->isp_csi2, ctx, force_update);
}

static int imx072_csi2_calc_phy_cfg0(struct v4l2_int_device *s,
				     u32 mipiclk, u32 lbound_hs_settle,
				     u32 ubound_hs_settle)
{
	struct omap34xxcam_videodev *vdev = s->u.slave->master->priv;
	struct isp_device *isp = dev_get_drvdata(vdev->cam->isp);

	return isp_csi2_calc_phy_cfg0(&isp->isp_csi2, mipiclk,
				      lbound_hs_settle, ubound_hs_settle);
}

struct imx072_platform_data justin_imx072_platform_data = {
	.power_set            = imx072_sensor_power_set,
	.priv_data_set        = imx072_sensor_set_prv_data,
	.set_xclk             = imx072_sensor_set_xclk,
	.csi2_lane_count      = imx072_csi2_lane_count,
	.csi2_cfg_vp_out_ctrl = imx072_csi2_cfg_vp_out_ctrl,
	.csi2_ctrl_update     = imx072_csi2_ctrl_update,
	.csi2_cfg_virtual_id  = imx072_csi2_cfg_virtual_id,
	.csi2_ctx_update      = imx072_csi2_ctx_update,
	.csi2_calc_phy_cfg0   = imx072_csi2_calc_phy_cfg0,
};


static int dw9716_lens_power_set(enum v4l2_power power)
{

	static enum v4l2_power previous_pwr = V4L2_POWER_OFF;

	switch (power) {
	case V4L2_POWER_ON:

		printk("dw9716_lens_power_set(ON)\n");

		#if 1
			subpm_set_output(LDO5,1);
			subpm_output_enable();
		#endif


			/* nStandBy is active HIGH. set HIGH to release reset */
			gpio_direction_output(DW9716_VCM_ENABLE, true);

//			gpio_set_value(DW9716_VCM_ENABLE, 0);

			gpio_set_value(DW9716_VCM_ENABLE, 1);


		
		gpio_direction_output(DW9716_VCM_ENABLE, 1);
		break;
	case V4L2_POWER_OFF:
		
		//subpm_set_output(LDO5,0);
		//subpm_output_enable();
		printk( "dw9716_lens_power_set(OFF)\n");
		//gpio_free(DW9716_VCM_ENABLE);

		break;
	case V4L2_POWER_STANDBY:

		printk ("dw9716_lens_power_set(STANDBY)");
		//gpio_set_value(DW9716_VCM_ENABLE, 0);
		//subpm_set_output(LDO5,0);
		//subpm_output_enable();
		break;

	}

	previous_pwr = power;
	return 0;
}

static int dw9716_lens_set_prv_data(void *priv)
{
	struct omap34xxcam_hw_config *hwc = priv;

	hwc->dev_index = 2;
	hwc->dev_minor = 5;
	hwc->dev_type = OMAP34XXCAM_SLAVE_LENS;

	return 0;
}

struct dw9716_platform_data justin_dw9716_platform_data = {
	.power_set = dw9716_lens_power_set,
	.priv_data_set = dw9716_lens_set_prv_data,
};

static u8 s2c_data, count;

static u8 rt8515_hw_status;

static int rt8515_init(void)
{
	if (gpio_request(RT8515_FLEN_GPIO, "RT8515 flash enable gpio") != 0){
		printk(KERN_ERR "Failed to request GPIO %d for ",RT8515_FLEN_GPIO);
		return -EIO;
	}
	if (gpio_request(RT8515_ENSET_S2C_GPIO, "RT8515 s2c communication gpio") != 0){
		printk(KERN_ERR "Failed to request GPIO %d for ",RT8515_ENSET_S2C_GPIO);
		return -EIO;
	}
			
	gpio_set_value(RT8515_FLEN_GPIO, 0);
	gpio_set_value(RT8515_ENSET_S2C_GPIO, 0);	
			
	gpio_direction_output(RT8515_FLEN_GPIO, 0);
	gpio_direction_output(RT8515_ENSET_S2C_GPIO, 0);	

	rt8515_hw_status = 0;


	return 0;
}

static int rt8515_exit(void)
{
//--[[ LGE_UBIQUIX_MODIFIED_START : ymjun@mnbt.co.kr [2011.09.20] - CAM
	gpio_set_value(RT8515_FLEN_GPIO, 0);
	gpio_set_value(RT8515_ENSET_S2C_GPIO, 0);
//--[[ LGE_UBIQUIX_MODIFIED_START : ymjun@mnbt.co.kr [2011.10.20] - CAM : remove
//	gpio_set_value(RT8515_FLINH_GPIO, 0);
//--]] LGE_UBIQUIX_MODIFIED_END : ymjun@mnbt.co.kr [2011.10.20] - CAM : remove	
//--]] LGE_UBIQUIX_MODIFIED_END : ymjun@mnbt.co.kr [2011.09.20] - CAM	
	
 	gpio_free(RT8515_FLEN_GPIO);
	gpio_free(RT8515_ENSET_S2C_GPIO);

	return 0;
}

static void rt8515_flash_on(void)
{
 	gpio_set_value(RT8515_FLEN_GPIO, 1);
	gpio_set_value(RT8515_ENSET_S2C_GPIO, 0);
	printk(KERN_DEBUG "RT8515_FLEN_GPIO (%d) is set to 1 \n",RT8515_FLEN_GPIO);
	rt8515_hw_status = (RT8515_HW_FLASH_MODE | RT8515_HW_FLASH_IS_ON);
}

static void rt8515_torch_on(u8 data)
{
	// sent data over s2c 
	// start with low state and leave high when done sending
	gpio_set_value(RT8515_FLEN_GPIO, 0);
	gpio_set_value(RT8515_ENSET_S2C_GPIO, 0);
	udelay(5); 
	

	printk(KERN_DEBUG "Settings RT8515_ENSET_S2C_GPIO (%d) is set from 0 to 1 %d times for s2c communication \n",RT8515_ENSET_S2C_GPIO,data);
	while(data--){
		// one rising edge is considered 1 value
		
		gpio_set_value(RT8515_ENSET_S2C_GPIO, 0);
		udelay(1);
		gpio_set_value(RT8515_ENSET_S2C_GPIO, 1);
		udelay(1);
		
	}
    	udelay(5);
	
	rt8515_hw_status = (RT8515_HW_FLASH_IS_ON);
}


static void rt8515_flash_off(unsigned long dummy)
{
	gpio_set_value(RT8515_FLEN_GPIO, 0);
	gpio_set_value(RT8515_ENSET_S2C_GPIO, 0);
	printk(KERN_DEBUG "RT8515_FLEN_GPIO (%d) is set to 0 \n",RT8515_FLEN_GPIO);
	rt8515_hw_status = 0;
}

static int rt8515_update_hw(struct v4l2_int_device *s)
{
	return rt8515_hw_status;
}

static int rt8515_set_prv_data(void *priv)
{
	struct omap34xxcam_hw_config *hwc = priv;

	hwc->dev_index = 2;
	hwc->dev_minor = 5;
	hwc->dev_type = OMAP34XXCAM_SLAVE_FLASH;

	return 0;
}

struct rt8515_platform_data justin_rt8515_data = {
	.init		= rt8515_init,
	.exit		= rt8515_exit,
	.flash_on	= rt8515_flash_on,
	.torch_on   = rt8515_torch_on,
	.flash_off	= rt8515_flash_off,
	.update_hw	= rt8515_update_hw,
	.priv_data_set  = rt8515_set_prv_data,
};

#endif

//--[[ LGE_UBIQUIX_MODIFIED_START : ymjun@mnbt.co.kr [2011.07.26] - CAM
// for the secondary sensor
#if defined(CONFIG_VIDEO_MT9V113) || defined(CONFIG_VIDEO_MT9V113_MODULE)

static struct omap34xxcam_sensor_config mt9v113_hwc = {
  .sensor_isp = 1,
  //.capture_mem = PAGE_ALIGN(640 * 480 * 2) * 6  ,
  .capture_mem = PAGE_ALIGN(640 * 480 * 2) * 6,
  .ival_default = { 1, 30 },
};

static int mt9v113_sensor_set_prv_data(struct v4l2_int_device *s, void *priv)
{
  struct omap34xxcam_hw_config *hwc = priv;

  hwc->u.sensor = mt9v113_hwc;
  hwc->dev_index = 0;
  hwc->dev_minor = 0;
  hwc->dev_type = OMAP34XXCAM_SLAVE_SENSOR;

  return 0;
}

static struct isp_interface_config mt9v113_if_config = {
  .ccdc_par_ser = ISP_PARLL,
  .dataline_shift = 0x2,
  .hsvs_syncdetect = ISPCTRL_SYNC_DETECT_VSRISE,
  .strobe = 0x0,
  .prestrobe = 0x0,
  .shutter = 0x0,
  .cam_mclk =  ISP_MT9V113_MCLK,
  .wenlog = ISPCCDC_CFG_WENLOG_AND,
  .wait_hs_vs = 0,
//  .raw_fmt_in = ISPCCDC_INPUT_FMT_RG_GB,
  .u.par.par_bridge = 0x2,
  .u.par.par_clk_pol = 0x0,

};

static int mt9v113_sensor_power_set(struct v4l2_int_device *dev, enum v4l2_power power)
{
  struct omap34xxcam_videodev *vdev = dev->u.slave->master->priv;
  struct isp_device *isp = dev_get_drvdata(vdev->cam->isp);
  static enum v4l2_power previous_power = V4L2_POWER_OFF;
  int err = 0;

  struct mt9v113_sensor *sensor = dev->priv;

  switch (power) 
  {
  case V4L2_POWER_ON:

    /* Power Up Sequence */
    printk(KERN_DEBUG "mt9v113_sensor_power_set(ON)\n");

	/*
	 * Through-put requirement:
	 * Set max OCP freq for 3630 is 200 MHz through-put
	 * is in KByte/s so 200000 KHz * 4 = 800000 KByte/s
	 */
    omap_pm_set_min_bus_tput(vdev->cam->isp, OCP_INITIATOR_AGENT, 800000);
/* S[, 20120922, mannsik.chung@lge.com, PM from froyo. */
	doing_wakeup = 1;
/* E], 20120922, mannsik.chung@lge.com, PM from froyo. */

    /* Hold a constraint to keep MPU in C1 */
    //--[[ LGE_UBIQUIX_MODIFIED_START : ymjun@mnbt.co.kr [2012.05.22] - CAM
    pm_qos_update_request(&pm_qos_handler,
	    				SET_MPU_CONSTRAINT);
    //--]] LGE_UBIQUIX_MODIFIED_END : ymjun@mnbt.co.kr [2012.05.22] - CAM

    isp_configure_interface(vdev->cam->isp,&mt9v113_if_config);


    if (gpio_request(MT9V113_RESET_GPIO, "mt9v113_rst") != 0)
      printk("\n\n\n>>>>>>>>>>>>MT9V113_RESET_GPIO_gpio_request_error\n\n\n");//return -EIO;
  
    if (gpio_request(MT9V113_STANDBY_GPIO, "mt9v113_PWD") != 0)
      printk("\n\n\n>>>>>>>>>>>>MT9V113_STANDBY_GPIO_gpio_request_error\n\n\n");//return -EIO;return -EIO;

    //RESET & PWDN HI
    gpio_direction_output(MT9V113_RESET_GPIO, true);
    gpio_set_value(MT9V113_RESET_GPIO, 1);

    gpio_direction_output(MT9V113_STANDBY_GPIO, true);
    gpio_set_value(MT9V113_STANDBY_GPIO, 1);

    mdelay(5);

    //<1.POWER ON>

    twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, 
          SET_VMMC1_V2_8,VMMC1_DEDICATED );
    twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, 
          VMMC1_DEV_GRP_P1,VMMC1_DEV_GRP );

    //udelay(20);
    udelay(1);
		
    twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER,
          VAUX3_1_8_V, TWL4030_VAUX3_DEDICATED);
    twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER,
          VAUX_DEV_GRP_P1, TWL4030_VAUX3_DEV_GRP);


    twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER,
          VAUX_1_8_V, TWL4030_VAUX4_DEDICATED);
    twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER,
          VAUX_DEV_GRP_P1, TWL4030_VAUX4_DEV_GRP);

    mdelay(10);

    //<2.VGA MCLK ON>
    //mt9v113_sensor_set_xclk(dev,24000000); //KJK 1211
    //sensor->pdata->set_xclk(dev, xclk_current);
    //sensor->pdata->set_xclk((struct v4l2_int_device *)dev, 24000000);
    //mdelay(5);

    //<3.POWER DOWN LOW>
    //gpio_direction_output(MT9V113_STANDBY_GPIO, true);
    gpio_set_value(MT9V113_STANDBY_GPIO, 0);
    mdelay(5);
    //udelay(2);

    //<4.RESET>
  
    //gpio_set_value(MT9V113_RESET_GPIO, 0);
    
    /* set to output mode */
    //gpio_direction_output(MT9V113_RESET_GPIO, true);
    //reset is active low
    //gpio_set_value(MT9V113_RESET_GPIO, 1);
    //mdelay(5);
    gpio_set_value(MT9V113_RESET_GPIO, 0);
    mdelay(10);
    gpio_set_value(MT9V113_RESET_GPIO, 1);
    mdelay(10);

    break;
    
  case V4L2_POWER_OFF:
  case V4L2_POWER_STANDBY:
    printk(KERN_DEBUG "mt9v113_sensor_power_set(STANDBY OR OFF) POWER %d\n",power);
    /* Power Down Sequence */

    //<1.POWER DOWN LOW && RESET LOW>
    gpio_set_value(MT9V113_STANDBY_GPIO, 0);
    gpio_set_value(MT9V113_RESET_GPIO, 0);
    mdelay(5);
    

    twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER,
          VAUX_DEV_GRP_NONE, TWL4030_VAUX4_DEV_GRP);


    twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, 
          VAUX_DEV_GRP_NONE,VMMC1_DEV_GRP );

    twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER,
          VAUX_DEV_GRP_NONE, TWL4030_VAUX3_DEV_GRP);

    /*
    twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, 
          VAUX_DEV_GRP_NONE,VMMC1_DEV_GRP );
    twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER,
          VAUX_DEV_GRP_NONE, TWL4030_VAUX3_DEV_GRP);
    */
    mdelay(10);

    
    gpio_free(MT9V113_STANDBY_GPIO);
    gpio_free(MT9V113_RESET_GPIO);
   
/* S[, 20120922, mannsik.chung@lge.com, PM from froyo. */
	doing_wakeup = 0; 
/* E], 20120922, mannsik.chung@lge.com, PM from froyo. */
    
	/* Remove pm constraints */
	//--[[ LGE_UBIQUIX_MODIFIED_START : ymjun@mnbt.co.kr [2012.05.22] - CAM		
	omap_pm_set_min_bus_tput(vdev->cam->isp, OCP_INITIATOR_AGENT, -1);
	pm_qos_update_request(&pm_qos_handler,
						CLEAR_MPU_CONSTRAINT);
	//--]] LGE_UBIQUIX_MODIFIED_END : ymjun@mnbt.co.kr [2012.05.22] - CAM  
    break;
  /*
  case V4L2_POWER_STANDBY:
    printk(KERN_DEBUG "mt9v113_sensor_power_set(STANDBY)\n");

    gpio_free(MT9V113_STANDBY_GPIO);
    gpio_free(MT9V113_RESET_GPIO);

    gpio_set_value(MT9V113_STANDBY_GPIO, 0);

    break;
    */

  }

  /* Save powerstate to know what was before calling POWER_ON. */
  previous_power = power;

  return err;
}

static u32 mt9v113_sensor_set_xclk(struct v4l2_int_device *s, u32 xclkfreq)
{
  struct omap34xxcam_videodev *vdev = s->u.slave->master->priv;
  struct isp_device *isp = dev_get_drvdata(vdev->cam->isp);

  /* Enable/Disable sensor xclk */
  if (xclkfreq)
      isp_enable_mclk(isp->dev);
  else
      isp_disable_mclk(isp);

  return isp_set_xclk(vdev->cam->isp, xclkfreq, USE_XCLKB); // XCLK B 
}
struct mt9v113_platform_data justin_mt9v113_platform_data = {
  .power_set  = mt9v113_sensor_power_set,
  .priv_data_set  = mt9v113_sensor_set_prv_data,
  .set_xclk = mt9v113_sensor_set_xclk,
};  
#endif //endif for the secondary sensor
//--]] LGE_UBIQUIX_MODIFIED_END : ymjun@mnbt.co.kr [2011.07.26] - CAM

void __init justin_cam_init(void)
{
	printk("Camera_inited\n");
	cam_inited = 0;
		
//--[[ LGE_UBIQUIX_MODIFIED_START : ymjun@mnbt.co.kr [2012.05.22] - CAM
	pm_qos_add_request(&pm_qos_handler, PM_QOS_CPU_DMA_LATENCY,
						CLEAR_MPU_CONSTRAINT);
//--]] LGE_UBIQUIX_MODIFIED_END : ymjun@mnbt.co.kr [2012.05.22] - CAM						
		
//	omap_mux_init_gpio(IMX072_RESET_GPIO, OMAP_PIN_OUTPUT);
		
	omap_mux_init_signal("gpio_37", OMAP_PIN_OUTPUT);
        	
	#if 0
	 omap_mux_init_signal("gpio_98", OMAP_PIN_INPUT_PULLDOWN);
        if (gpio_request(IMX072_RESET_GPIO, "camreset") < 0) {
                printk(KERN_ERR "can't get subpm enable GPIO\n");
                return;
        }
        gpio_direction_output(IMX072_RESET_GPIO, 0);

	#endif
		omap_mux_init_signal("gpio_98", OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_gpio(DW9716_VCM_ENABLE, OMAP_PIN_INPUT_PULLDOWN);
//--[[ LGE_UBIQUIX_MODIFIED_START : ymjun@mnbt.co.kr [2011.07.26] - CAM
#if defined(CONFIG_VIDEO_MT9V113) || defined(CONFIG_VIDEO_MT9V113_MODULE)
		omap_mux_init_gpio(MT9V113_RESET_GPIO, OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_gpio(MT9V113_STANDBY_GPIO, OMAP_PIN_INPUT_PULLDOWN);
#endif
//--]] LGE_UBIQUIX_MODIFIED_END : ymjun@mnbt.co.kr [2011.07.26] - CAM

//--[[ LGE_UBIQUIX_MODIFIED_START : ymjun@mnbt.co.kr [2011.10.20] - CAM : remove
//		omap_mux_init_gpio(RT8515_FLINH_GPIO, OMAP_PIN_OUTPUT);  
//--]] LGE_UBIQUIX_MODIFIED_END : ymjun@mnbt.co.kr [2011.10.20] - CAM : remove
		omap_mux_init_gpio(RT8515_FLEN_GPIO, OMAP_PIN_OUTPUT);  
		omap_mux_init_gpio(RT8515_ENSET_S2C_GPIO, OMAP_PIN_OUTPUT); 

	cam_inited = 1;
}
#endif
