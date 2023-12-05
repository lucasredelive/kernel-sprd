#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/err.h>
#include <linux/kobject.h>
#include <asm-generic/bug.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_fdt.h>

#include <linux/fs.h>
#include <asm/system_misc.h>
//#include <mt-plat/mtk_boot_common.h>
//#include <ontim/ontim_cpu_devinfo.h>

//#include <linux/qpnp/qpnp-adc.h>
//#include "board_id_adc.h"


#define BUF_SIZE 64

#if 0
char front_cam_name[64] = "Unknown";
char frontaux_cam_name[64] = "Unknown";
char back_cam_name[64] = "Unknown";
char backaux_cam_name[64] = "Unknown";
char backaux2_cam_name[64] = "Unknown";

char front_cam_efuse_id[64] = {0};
char frontaux_cam_efuse_id[64] = {0};
char back_cam_efuse_id[64] = {0};
char backaux_cam_efuse_id[64] = {0};
char backaux2_cam_efuse_id[64] = {0};
#endif

char backaux2_cam_otp_status[64] = "Unknown";
char backaux_cam_otp_status[64] = "Unknown";
char back_cam_otp_status[64] = "Unknown";
char front_cam_otp_status[64] = "Unknown";

#if 0
//read borad ID from adc
#define BOARD_ID_MAX 16
struct qpnp_vadc_result chip_adc_result;

typedef struct adc_voltage {
	int min_voltage;
	int max_voltage;
} adc_boardid_match;

static adc_boardid_match boardid_table[] = {
	{.min_voltage = 150, .max_voltage = 250}, //10k
	{.min_voltage = 250, .max_voltage = 350}, //20k
	{.min_voltage = 350, .max_voltage = 480}, //27k
	{.min_voltage = 480, .max_voltage = 550}, //39k
	{.min_voltage = 550, .max_voltage = 630}, //47k
	{.min_voltage = 630, .max_voltage = 710}, //56k
	{.min_voltage = 710, .max_voltage = 790}, //68k
	{.min_voltage = 790, .max_voltage = 870}, //82k
	{.min_voltage = 870, .max_voltage = 970}, //100k
	{.min_voltage = 970, .max_voltage = 1070}, //120k
	{.min_voltage = 1070, .max_voltage = 1170}, //150k
};
#endif

typedef struct board_id {
	int index;
	const char *hw_version;
	const char *qcn_type;
	const char *model;
} boardid_match_t;
/* hawaii
Version gpio131 gpio133
P0      0		0
DVT1    0		1
DVT2    1		0
PVT     1		1

only care hw_version, DONOT care qcn_type and model.
*/
static boardid_match_t board_table[] = {
//	{ .index = 0,  .hw_version = "P0",         .qcn_type = "no-ca", .model = "primary"  },//P0
	{ .index = 0,  .hw_version = "P0",         .qcn_type = "unknown", .model = "primary"  },
	{ .index = 1,  .hw_version = "DVT1",       .qcn_type = "unknown", .model = "primary"    },
	{ .index = 2,  .hw_version = "DVT2",       .qcn_type = "unknown", .model = "primary"    },
	{ .index = 3,  .hw_version = "PVT",        .qcn_type = "unknown", .model = "primary"    },
};

typedef struct mid_match {
	int index;
	const char *name;
} mid_match_t;

static mid_match_t emmc_table[] = {
	{
		.index = 0,
		.name = "Unknown"
	},
	{
		.index = 17,
		.name = "Toshiba"
	},
	{
		.index = 19,
		.name = "Micron"
	},
	{
		.index = 69,
		.name = "Sandisk"
	},
	{
		.index = 21,
		.name = "Samsung"
	},
// BEGIN Ontim, rd.zhigang.he, 9/8/2020, 9888845, St-result :PASS, add FORESEE emmc info and update hwinfo
	{
		.index = 0x88,
		.name = "FORESEE"
	},
// END 9888845
	{
		.index = 0xd6,
		.name = "FORESEE"
	},
	{
		.index = 0x90,
		.name = "Hynix"
	},
	{
		.index = 0x70,
		.name = "KSI"
	},
	/*UFS*/
	{
		.index = 0xCE,
		.name = "Samsung"
	},
	{
		.index = 0xAD,
		.name = "Hynix"
	},
	{
		.index = 0x98,
		.name = "Toshiba"
	},
};


#define MAX_HWINFO_SIZE 64
#include "hwinfo.h"
typedef struct {
	char *hwinfo_name;
	char hwinfo_buf[MAX_HWINFO_SIZE];
} hwinfo_t;

#define KEYWORD(_name) \
	[_name] = {.hwinfo_name = __stringify(_name), \
		   .hwinfo_buf = {0}},

static hwinfo_t hwinfo[HWINFO_MAX] =
{
#include "hwinfo.h"
};
#undef KEYWORD


static const char *foreach_emmc_table(int index)
{
	int i = 0;
	for (; i < sizeof(emmc_table) / sizeof(mid_match_t); i++) {
		if (index == emmc_table[i].index)
			return emmc_table[i].name;
	}

	return emmc_table[0].name;;
}
static int hwinfo_read_file(char *file_name, char buf[], int buf_size)
{
	struct file *fp;
	mm_segment_t fs;
	loff_t pos = 0;
	ssize_t len = 0;

	if (file_name == NULL || buf == NULL)
		return -1;

	fp = filp_open(file_name, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		printk(KERN_CRIT "file not found/n");
		return -1;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);
	memset(buf, 0x00, buf_size);
	len = vfs_read(fp, buf, buf_size, &pos);
	buf[buf_size - 1] = '\n';
	printk(KERN_INFO "buf= %s,size = %ld \n", buf, (long int)len);
	filp_close(fp, NULL);
	set_fs(fs);

	return 0;
}

static int hwinfo_write_file(char *file_name, const char buf[], int buf_size)
{
	struct file *fp;
	mm_segment_t fs;
	loff_t pos = 0;
	ssize_t len = 0;

	if (file_name == NULL || buf == NULL || buf_size < 1)
		return -1;

	fp = filp_open(file_name, O_RDWR | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		printk(KERN_CRIT "file not found/n");
		return -1;

	}

	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = fp->f_pos;
	len = vfs_write(fp, buf, buf_size, &pos);
	fp->f_pos = pos;
	printk(KERN_INFO "buf = %s,size = %ld \n", buf, (long int)len);
	filp_close(fp, NULL);
	set_fs(fs);
	return 0;
}

//extern char *saved_command_line;

int get_cmdline_param_val(const char *param, char *val, int len)
{
	char *p, *q;
	char *s = "unknown";
	int n;
	if ( (len <= 1) || !param || !val)
		return -1;
	p = strstr(saved_command_line, param);
	if (p) {
		p = strstr(p, "=") + 1;
		if (*p == '"') {
			p++;
			q = strstr(p, "\"");
		} else
			q = strstr(p, " ");
		if (q)
			n = q - p;
		else
			n = strlen(p);
		n = min(len - 1, n);
		strncpy(val, p, n);
		val[n] = 0;
	} else {
		n = min_t(int, len - 1, strlen(s));
		strncpy(val, s, n);
		val[n] = 0;
	}
	return n;
}

#if 0
/*Android:Settings->About phone->CPU  register function to distinguish the CPU model*/
static char *msm_read_hardware_id(void)
{
	static char msm_soc_str[256] = "Qualcomm Technologies, Inc ";
	static bool string_generated;
	int ret = 0;

	if (string_generated)
		return msm_soc_str;

	ret = get_cpu_type();
	if (ret != 0)
		goto err_path;

	ret = strlcat(msm_soc_str, hwinfo[CPU_TYPE].hwinfo_buf,
	              sizeof(msm_soc_str));
	if (ret > sizeof(msm_soc_str))
		goto err_path;

	string_generated = true;
	return msm_soc_str;
err_path:
	printk(KERN_CRIT "UNKNOWN SOC TYPE, Using defaults.\n");
	return "Qualcomm Technologies, Inc SDM710";
}
#endif
static int  get_cpu_type(void)
{
	sprintf(hwinfo[cpu_type].hwinfo_buf, "%s", of_flat_dt_get_cpuinfo_hw() ? of_flat_dt_get_cpuinfo_hw() : "unknown");
	return 0;
}
#if 0
char lcd_name[BUF_SIZE] = "unknow";
static int get_lcd_type(void)
{
	sprintf(hwinfo[LCD_MFR].hwinfo_buf, "%s", lcd_name);
	return 1;
}
//__setup("msm_drm.dsi_display0=", set_lcd_name);
#else
/* BEGIN, Ontim,  wzx, 19/04/19, St-result :PASS,LCD and TP Device information */

//#define LCD_INFO_FILE "/sys/class/graphics/fb0/msm_fb_panel_info"
#define LCD_INFO_FILE "/sys/ontim_dev_debug/touch_screen/lcdvendor"

/* END */
static int get_lcd_type(void)
{
	char buf[200] = {0};
	int  ret = 0;

	ret = hwinfo_read_file(LCD_INFO_FILE, buf, sizeof(buf));
	if (ret != 0)
	{
		printk(KERN_CRIT "get lcd_type read file failed.\n");
		return -1;
	}
	printk(KERN_INFO "lcd %s\n", buf);
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';
	memcpy(hwinfo[LCD_MFR].hwinfo_buf, buf, strlen(buf));

	return 0;
}
#endif
#define TP_VENDOR_FILE "/sys/ontim_dev_debug/touch_screen/vendor"
#define TP_VERSION_FILE "/sys/ontim_dev_debug/touch_screen/version"
static int get_tp_info(void)
{
	char buf[BUF_SIZE] = {0};
	char buf2[BUF_SIZE] = {0};
	char str[BUF_SIZE + BUF_SIZE] = {0};
	int  ret = 0;

	ret = hwinfo_read_file(TP_VENDOR_FILE, buf, sizeof(buf));
	if (ret != 0)
	{
		printk(KERN_CRIT "get_tp_info failed.");
		return -1;
	}
	printk(KERN_INFO "tp %s\n", buf);
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';

	ret = hwinfo_read_file(TP_VERSION_FILE, buf2, sizeof(buf2));
	if (ret != 0)
	{
		printk(KERN_CRIT "get_tp_info failed.");
		return -1;
	}
	printk(KERN_INFO "tp version %s\n", buf2);
	if (buf2[strlen(buf2) - 1] == '\n')
		buf2[strlen(buf2) - 1] = '\0';

	sprintf(str, "%s-version:%s", buf, buf2);
	memcpy(hwinfo[TP_MFR].hwinfo_buf, str , strlen(str));
	return 0;
}

/* add for zhanxun platform */
#if 1
static int pars_usb_type_char(char *pcBuf, char *pcRes)
{
	char *pcBegin = NULL;
	char *pcEnd = NULL;

	pcBegin = strstr(pcBuf, "[");
	pcEnd = strstr(pcBuf, "]");

	if(pcBegin == NULL || pcEnd == NULL || pcBegin > pcEnd)
	{
		printk("USB name not found!\n");
	} else {
		pcBegin += strlen("[");
		memcpy(pcRes, pcBegin, pcEnd-pcBegin);
	}

	return 0;

}
#define POWER_USB_ONLINE_FILE "/sys/class/power_supply/charger/usb_type"
static int get_power_usb_type(void)
{
	char buf[64] = {0};
	char usb_buf[64] = {0};
	int ret = 0;

	memset(buf, 0x00, 64);

	ret = hwinfo_read_file(POWER_USB_ONLINE_FILE, buf, sizeof(buf));
	if (ret != 0)
	{
		printk(KERN_CRIT "get_power usb type failed.");
		return -1;
	}
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';
	printk(KERN_INFO "power usb buf %s\n", buf);

	pars_usb_type_char(buf,usb_buf);

	strcpy(hwinfo[POWER_USB_TYPE].hwinfo_buf, usb_buf);

	return 0;
}
#else
#define POWER_USB_ONLINE_FILE "/sys/class/power_supply/usb/online"
#define POWER_AC_ONLINE_FILE "/sys/class/power_supply/ac/online"
static int get_power_usb_type(void)
{
	char buf[64] = {0};
	char online[64] = {0};
	int ret = 0;

	memset(buf, 0x00, 64);
	ret = hwinfo_read_file(POWER_AC_ONLINE_FILE, buf, sizeof(buf));
	if (ret != 0)
	{
		printk(KERN_CRIT "get_power usb type failed.");
		return -1;
	}
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';
	printk(KERN_INFO "power ac %s\n", buf);

	memset(online, 0x00, 64);
	ret = hwinfo_read_file(POWER_USB_ONLINE_FILE, online, sizeof(online));
	if (ret != 0)
	{
		printk(KERN_CRIT "get_power usb online failed.");
		return -1;
	}
	if (online[strlen(online) - 1] == '\n')
		online[strlen(online) - 1] = '\0';
	printk(KERN_INFO "power usb online %s\n", online);

	if (!strcmp(online, "1"))
		strcpy(hwinfo[POWER_USB_TYPE].hwinfo_buf, "USB");
	else if (!strcmp(buf, "1"))
		strcpy(hwinfo[POWER_USB_TYPE].hwinfo_buf, "USB_DCP");
	else
		strcpy(hwinfo[POWER_USB_TYPE].hwinfo_buf, "Unknow");

	return 0;
}
#endif

#define BATTARY_RESISTANCE_FILE "/sys/ontim_dev_debug/battery/vendor"
static int get_battary_mfr(void)
{
	char buf[64] = {0};
	int ret = 0;

	ret = hwinfo_read_file(BATTARY_RESISTANCE_FILE, buf, sizeof(buf));
	if (ret != 0)
	{
		printk(KERN_CRIT "get_battary_mfr failed.");
		return -1;
	}
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';
	printk(KERN_INFO "Battary %s\n", buf);

	strcpy(hwinfo[BATTARY_MFR].hwinfo_buf, buf);

	return 0;
}

#define BATTARY_CAPACITY_FILE "/sys/class/power_supply/battery/capacity"
static int get_battary_cap(void)
{
	char buf[20] = {0};
	int ret = 0;
	int capacity_value = 0;

	ret = hwinfo_read_file(BATTARY_CAPACITY_FILE, buf, sizeof(buf));
	if (ret != 0)
	{
		printk(KERN_CRIT "get_battary_cap failed.");
		return -1;
	}
	printk(KERN_INFO "Battary cap %s\n", buf);
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';
	sscanf(buf, "%d", &capacity_value);

	strcpy(hwinfo[BATTARY_CAP].hwinfo_buf, buf);

	return 0;
}

#define BATTARY_VOL_FILE "/sys/class/power_supply/battery/voltage_now"
static int get_battary_vol(void)
{
	char buf[20] = {0};
	int ret = 0;
	int voltage_now_value = 0;

	ret = hwinfo_read_file(BATTARY_VOL_FILE, buf, sizeof(buf));
	if (ret != 0)
	{
		printk(KERN_CRIT "get_battary_vol failed.");
		return -1;
	}
	printk(KERN_INFO "Battary vol %s\n", buf);
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';
	sscanf(buf, "%d", &voltage_now_value);

	strcpy(hwinfo[BATTARY_VOL].hwinfo_buf, buf);

	return 0;
}

#define BATTARY_IN_SUSPEND_FILE "/sys/class/power_supply/battery/input_suspend"
static int get_battery_input_suspend(void)
{
	char buf[64] = {0};
	int ret = 0;
	int input_suspend_value = 0;

	ret = hwinfo_read_file(BATTARY_IN_SUSPEND_FILE, buf, sizeof(buf));
	if (ret != 0)
	{
		printk(KERN_CRIT "input_suspend_value failed.");
		return -1;
	}
	printk(KERN_INFO "Battary input_suspend_value %s\n", buf);
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';
	sscanf(buf, "%d", &input_suspend_value);

	strcpy(hwinfo[battery_input_suspend].hwinfo_buf, buf);

	return 0;
}

static int set_backaux2_camera_otp_status(const char * buf, int n)
{
	strncpy(backaux2_cam_otp_status, buf, n);
	printk(KERN_INFO  "buf = %s n = %d\n", buf, n);
	backaux2_cam_otp_status[n] = '\0';
	return 0;
}
static int set_backaux_camera_otp_status(const char * buf, int n)
{
	int i = 0;
	strncpy(backaux_cam_otp_status, buf, n);
	printk(KERN_INFO "buf = %s n = %d\n", buf, n);
	backaux_cam_otp_status[n] = '\0';
	for (; i< n+2; i++) 
	{
		printk("buf[%d] = %x\n", i, backaux_cam_otp_status[i]);
	}
	return 0;
}
static int set_back_camera_otp_status(const char * buf, int n)
{
	strncpy(back_cam_otp_status, buf, n);
	printk(KERN_INFO "buf = %s n = %d\n", buf, n);
	back_cam_otp_status[n] = '\0';
	return 0;
}
static int set_front_camera_otp_status(const char * buf, int n)
{
	strncpy(front_cam_otp_status, buf, n);
	printk(KERN_INFO "buf = %s n = %d\n", buf, n);
	front_cam_otp_status[n] = '\0';
	return 0;
}
/*
static int set_front_camera_id(const char * buf, int n)
{
	strncpy(front_cam_name, buf, n);
	printk(KERN_INFO "buf = %s n = %d\n", buf, n);
	front_cam_name[n] = '\0';
	return 0;
}
static int set_frontaux_camera_id(const char * buf, int n)
{
	strncpy(frontaux_cam_name, buf, n);
	printk(KERN_INFO "buf = %s n = %d\n", buf, n);
	frontaux_cam_name[n] = '\0';
	return 0;
}
static int set_back_camera_id(const char * buf, int n)
{
	strncpy(back_cam_name, buf, n);
	printk(KERN_INFO "buf = %s n = %d\n", buf, n);
	back_cam_name[n] = '\0';
	return 0;
}
static int set_backaux_camera_id(const char * buf, int n)
{
	strncpy(back_cam_name, buf, n);
	printk(KERN_INFO "buf = %s n = %d\n", buf, n);
	back_cam_name[n] = '\0';
	return 0;
}
static int set_backaux2_camera_id(const char * buf, int n)
{
	strncpy(backaux2_cam_name, buf, n);
	printk(KERN_INFO "buf = %s n = %d\n", buf, n);
	backaux2_cam_name[n] = '\0';
	return 0;
}
static int set_front_camera_efuse_id(const char * buf, int n)
{
	strncpy(front_cam_efuse_id, buf, n);
	printk(KERN_INFO "buf = %s n = %d\n", buf, n);
	front_cam_efuse_id[n] = '\0';
	return 0;
}
static int set_frontaux_camera_efuse_id(const char * buf, int n)
{
	strncpy(frontaux_cam_efuse_id, buf, n);
	printk(KERN_INFO "buf = %s n = %d\n", buf, n);
	frontaux_cam_efuse_id[n] = '\0';
	return 0;
}
static int set_back_camera_efuse_id(const char * buf, int n)
{
	strncpy(back_cam_efuse_id, buf, n);
	printk(KERN_INFO "buf = %s n = %d\n", buf, n);
	back_cam_efuse_id[n] = '\0';
	return 0;
}
static int set_backaux_camera_efuse_id(const char * buf, int n)
{
	strncpy(backaux_cam_efuse_id, buf, n);
	printk(KERN_INFO "buf = %s n = %d\n", buf, n);
	backaux_cam_efuse_id[n] = '\0';
	return 0;
}

static int set_backaux2_camera_efuse_id(const char * buf, int n)
{
	strncpy(backaux2_cam_efuse_id, buf, n);
	printk(KERN_INFO "buf = %s n = %d\n", buf, n);
	backaux2_cam_efuse_id[n] = '\0';
	return 0;
}
*/
static int put_battery_input_suspend(const char * buf, int n)
{
	int ret = 0;

	ret = hwinfo_write_file(BATTARY_IN_SUSPEND_FILE, buf, 1);
	if (ret != 0)
	{
		printk(KERN_CRIT "input_suspend_value failed.");
		return -1;
	}

	return 0;
}
#define BATTARY_CHARGING_EN_FILE "/sys/bus/platform/devices/charger/input_current"
static int get_battery_charging_enabled(void)
{
	char buf[64] = {0};
	int ret = 0;
	int charging_enabled_value = 0;

	ret = hwinfo_read_file(BATTARY_CHARGING_EN_FILE, buf, sizeof(buf));
	if (ret != 0)
	{
		printk(KERN_CRIT "battery_charging_enabled failed.");
		return -1;
	}
	printk(KERN_INFO "Battary battery_charging_enabled %s\n", buf);
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';
	sscanf(buf, "%d", &charging_enabled_value);

	strcpy(hwinfo[battery_charging_enabled].hwinfo_buf, buf);

	return 0;
}
static int put_battery_charging_enabled(const char * buf, int n)
{
	int ret = 0;

	ret = hwinfo_write_file(BATTARY_CHARGING_EN_FILE, buf, n > 0 ? n : 1);
	if (ret != 0)
	{
		printk(KERN_CRIT "battery_charging_enabled failed.");
		return -1;
	}

	return 0;
}
#define TYPEC_VENDOR_FILE "/sys/class/power_supply/usb/typec_mode"
static ssize_t get_typec_vendor(void)
{
	char buf[64] = {0};
	int ret = 0;

	ret = hwinfo_read_file(TYPEC_VENDOR_FILE, buf, sizeof(buf));
	if (ret != 0) {
		printk(KERN_CRIT "get_typec_vendor failed.");
		return -1;
	}
	buf[63] = '\n';
	printk(KERN_INFO "Typec vendor: %s\n", buf);
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';

	memcpy(hwinfo[TYPEC_MFR].hwinfo_buf, buf, 64);

	return 0;
}

#ifdef CONFIG_USB_SC27XX_TYPEC
extern int g_typec_cc_polarity;
#else
#define TYPEC_CC_STATUS_FILE "/sys/class/power_supply/usb/typec_cc_orientation"
#endif
static ssize_t get_typec_cc_status(void)
{
	char buf[BUF_SIZE] = {0};

	#ifdef CONFIG_USB_SC27XX_TYPEC
        switch(g_typec_cc_polarity){
        case 0:strcpy(buf,"cc_1");
               break;
        case 1:strcpy(buf,"cc_2");
               break;
        default:strcpy(buf,"unknow");
             break;
        }
    #else

	int ret = 0;

	ret = hwinfo_read_file(TYPEC_CC_STATUS_FILE, buf, BUF_SIZE);
	if (ret != 0) {
		printk(KERN_CRIT "get_typec_cc status failed.");
		return -1;
	}
	buf[1] = '\0';
	#endif
	printk(KERN_INFO "Typec cc status: %s\n", buf);

	memcpy(hwinfo[TYPEC_CC_STATUS].hwinfo_buf, buf, BUF_SIZE);

	return 0;
}

#define ADB_SN_FILE "/config/usb_gadget/g1/strings/0x409/serialnumber"
static ssize_t get_adb_sn(void)
{
	char buf[BUF_SIZE] = {};
	int ret = 0;

	ret = hwinfo_read_file(ADB_SN_FILE, buf, sizeof(buf));
	if (ret != 0) {
		printk(KERN_CRIT "get adb sn failed.");
		return -1;
	}
	printk(KERN_INFO "Adb SN: %s\n", buf);
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';

	strcpy(hwinfo[ADB_SN].hwinfo_buf, buf);

	return 0;
}

#define SPEAKER_MFR_FILE "/proc/asound/cards"
static int get_speaker_mfr(void)
{
	char buf[MAX_HWINFO_SIZE] = {0};
	char* p = buf;
	char* q = buf;
	int len = 0;
	int ret = -1;

	ret = hwinfo_read_file(SPEAKER_MFR_FILE, buf, sizeof(buf));
	if (ret != 0)
	{
		printk(KERN_CRIT "get_speaker_mfr failed.");
		return -1;
	}

	while ((*p++ != '[') && (p < (buf + MAX_HWINFO_SIZE)));
	while ((*p != ']') && (p < (buf + MAX_HWINFO_SIZE)))
	{
		*q++ = *p++;
		len++;
	}

	buf[len - 1] = '\0';
	printk(KERN_INFO "speaker %s\n", buf);

	memcpy(hwinfo[SPEAKER_MFR].hwinfo_buf, buf, len);
	return 0;
}

//houzn add
#define CHARGER_IC_VENDOR_FILE "/sys/ontim_dev_debug/charge_ic/vendor"
static void get_charger_ic(void)
{
	int ret = 0;
	char buf[MAX_HWINFO_SIZE] = {0};

	sprintf(buf, "%s", "Unknow");
	ret = hwinfo_read_file(CHARGER_IC_VENDOR_FILE, buf, sizeof(buf));
	if (ret)
		pr_err("get %s finger type failed.\n", CHARGER_IC_VENDOR_FILE);

	if (likely(buf[strlen(buf) - 1] == '\n')) {
		buf[strlen(buf) - 1] = '\0';
	}
	strcpy(hwinfo[CHARGER_IC_MFR].hwinfo_buf, buf);
	pr_err("charge_ic:%s .\n", hwinfo[CHARGER_IC_MFR].hwinfo_buf);
}
//add end

#ifdef CONFIG_USB_CABLE
extern unsigned int get_rf_gpio_value(void);
static void get_rfgpio_state(void)
{
	int ret = 0;
	ret = get_rf_gpio_value();
	pr_err("RF_GPIO=%d\n", ret);
	strcpy(hwinfo[RF_GPIO].hwinfo_buf, ret?"1":"0");
}
#endif

#define FINGERPRINT_VENDOR_FILE "/sys/ontim_dev_debug/fingersensor/vendor"
static ssize_t get_fingerprint_id(void)
{
	char buf[MAX_HWINFO_SIZE] = {};
	int ret = 0;

	ret = hwinfo_read_file(FINGERPRINT_VENDOR_FILE, buf, sizeof(buf));
	if (ret != 0) {
		printk(KERN_CRIT "get_fingerprint_vendor failed.");
		return -1;
	}
	printk(KERN_INFO "Fingerprint vendor: %s\n", buf);
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';

	strcpy(hwinfo[FP_MFR].hwinfo_buf, buf);

	return 0;
}

#define GSENSOR_VENDOR_FILE "/sys/class/sprd_sensorhub/sensor_hub/gyr_info"
static ssize_t get_gsensor_id(void)
{
	char buf[MAX_HWINFO_SIZE] = {};
	int ret = 0;

	ret = hwinfo_read_file(GSENSOR_VENDOR_FILE, buf, sizeof(buf));
	if (ret != 0) {
		printk(KERN_CRIT "gsensor failed.");
		return -1;
	}
	printk(KERN_INFO "gsensor vendor: %s\n", buf);
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';

	strcpy(hwinfo[GSENSOR_MFR].hwinfo_buf, buf);

	return 0;
}

#define ALSPS_VENDOR_FILE "/sys/class/sprd_sensorhub/sensor_hub/light_info"
static ssize_t get_alsps_id(void)
{
	char buf[MAX_HWINFO_SIZE] = {};
	int ret = 0;

	ret = hwinfo_read_file(ALSPS_VENDOR_FILE, buf, sizeof(buf));
	if (ret != 0) {
		printk(KERN_CRIT "als_prox failed.");
		return -1;
	}
	printk(KERN_INFO "als_prox vendor: %s\n", buf);
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';

	strcpy(hwinfo[ALSPS_MFR].hwinfo_buf, buf);

	return 0;
}
#define FRONT_CAMERA_VENDOR_FILE "/sys/ontim_bootinfo/front_cam_info"
#define FRONTAUX_CAMERA_VENDOR_FILE "/sys/ontim_bootinfo/frontaux_cam_info"
#define BACK_CAMERA_VENDOR_FILE "/sys/ontim_bootinfo/back_cam_info"
#define BACKAUX_CAMERA_VENDOR_FILE "/sys/ontim_bootinfo/backaux_cam_info"
#define BACKAUX2_CAMERA_VENDOR_FILE "/sys/ontim_bootinfo/backaux2_cam_info"

#define FRONT_CAMERA_EFUSE_FILE "/sys/ontim_bootinfo/front_cam_efuse"
#define FRONTAUX_CAMERA_EFUSE_FILE "/sys/ontim_bootinfo/frontaux_cam_efuse"
#define BACK_CAMERA_EFUSE_FILE "/sys/ontim_bootinfo/back_cam_efuse"
#define BACKAUX_CAMERA_EFUSE_FILE "/sys/ontim_bootinfo/backaux_cam_efuse"
#define BACKAUX2_CAMERA_EFUSE_FILE "/sys/ontim_bootinfo/backaux2_cam_efuse"

static void get_front_camera_id(void)
{
	char buf[MAX_HWINFO_SIZE] = {};
	int ret = 0;

	ret = hwinfo_read_file(FRONT_CAMERA_VENDOR_FILE, buf, sizeof(buf));
	if (ret != 0) {
		printk(KERN_CRIT "front camera failed.");
		return;
	}
	printk(KERN_INFO "front camera vendor: %s\n", buf);

	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';

	strcpy(hwinfo[FRONT_CAM_MFR].hwinfo_buf, buf);

	return;
}
static void get_frontaux_camera_id(void)
{
	char buf[MAX_HWINFO_SIZE*6] = {};
	int ret = 0;

	ret = hwinfo_read_file(FRONTAUX_CAMERA_VENDOR_FILE, buf, sizeof(buf));
	if (ret != 0) {
		printk(KERN_CRIT "frontaux camera failed.");
		return;
	}
	printk(KERN_INFO "frontaux camera vendor: %s\n", buf);

	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';

	strcpy(hwinfo[FRONTAUX_CAM_MFR].hwinfo_buf, buf);

	return;

}
static void get_back_camera_id(void)
{
	char buf[MAX_HWINFO_SIZE] = {};
	int ret = 0;

	ret = hwinfo_read_file(BACK_CAMERA_VENDOR_FILE, buf, sizeof(buf));
	if (ret != 0) {
		printk(KERN_CRIT "back camera failed.");
		return;
	}
	printk(KERN_INFO "back camera vendor: %s\n", buf);

	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';

	strcpy(hwinfo[BACK_CAM_MFR].hwinfo_buf, buf);

	return;
}
static void get_backaux_camera_id(void)
{
	char buf[MAX_HWINFO_SIZE] = {};
	int ret = 0;

	ret = hwinfo_read_file(BACKAUX_CAMERA_VENDOR_FILE, buf, sizeof(buf));
	if (ret != 0) {
		printk(KERN_CRIT "backaux camera failed.");
		return;
	}
	printk(KERN_INFO "backaux camera vendor: %s\n", buf);

	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';

	strcpy(hwinfo[BACKAUX_CAM_MFR].hwinfo_buf, buf);

	return;
}
static void get_backaux2_camera_id(void)
{
	char buf[MAX_HWINFO_SIZE] = {};
	int ret = 0;

	ret = hwinfo_read_file(BACKAUX2_CAMERA_VENDOR_FILE, buf, sizeof(buf));
	if (ret != 0) {
		printk(KERN_CRIT "backaux2 camera failed.");
		return;
	}
	printk(KERN_INFO "backaux2 camera vendor: %s\n", buf);

	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';

	strcpy(hwinfo[BACKAUX2_CAM_MFR].hwinfo_buf, buf);

	return;
}
static void get_backaux2_camera_otp_status(void)
{
	if (backaux2_cam_otp_status != NULL)
		strncpy(hwinfo[BACKAUX2_CAM_OTP_STATUS].hwinfo_buf, backaux2_cam_otp_status,
		        ((strlen(backaux2_cam_otp_status) >= sizeof(hwinfo[BACKAUX2_CAM_OTP_STATUS].hwinfo_buf) ?
		          sizeof(hwinfo[BACKAUX2_CAM_OTP_STATUS].hwinfo_buf) : strlen(backaux2_cam_otp_status))));
}
static void get_backaux_camera_otp_status(void)
{
         if (backaux_cam_otp_status != NULL)
                 strncpy(hwinfo[BACKAUX_CAM_OTP_STATUS].hwinfo_buf, backaux_cam_otp_status,
                        ((strlen(backaux_cam_otp_status) >= sizeof(hwinfo[BACKAUX_CAM_OTP_STATUS].hwinfo_buf) ?
                           sizeof(hwinfo[BACKAUX_CAM_OTP_STATUS].hwinfo_buf) : strlen(backaux_cam_otp_status))));
 }

static void get_back_camera_otp_status(void)
{
         if (back_cam_otp_status != NULL)
                 strncpy(hwinfo[BACK_CAM_OTP_STATUS].hwinfo_buf, back_cam_otp_status,
                        ((strlen(back_cam_otp_status) >= sizeof(hwinfo[BACK_CAM_OTP_STATUS].hwinfo_buf) ?
                           sizeof(hwinfo[BACK_CAM_OTP_STATUS].hwinfo_buf) : strlen(back_cam_otp_status))));
 }


static void get_front_camera_otp_status(void)
{
         if (front_cam_otp_status != NULL)
                 strncpy(hwinfo[FRONT_CAM_OTP_STATUS].hwinfo_buf, front_cam_otp_status,
                        ((strlen(front_cam_otp_status) >= sizeof(hwinfo[FRONT_CAM_OTP_STATUS].hwinfo_buf) ?
                           sizeof(hwinfo[FRONT_CAM_OTP_STATUS].hwinfo_buf) : strlen(front_cam_otp_status))));
 }
static void get_front_camera_efuse_id(void)
{
	char buf[MAX_HWINFO_SIZE] = {};
	int ret = 0;

	ret = hwinfo_read_file(FRONT_CAMERA_EFUSE_FILE, buf, sizeof(buf));
	if (ret != 0) {
		printk(KERN_CRIT "front camera efuse failed.");
		return;
	}
	printk(KERN_INFO "front camera efuse id: %s\n", buf);

	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';

	strcpy(hwinfo[FRONT_CAM_EFUSE].hwinfo_buf, buf);

	return;
}
static void get_frontaux_camera_efuse_id(void)
{
	char buf[MAX_HWINFO_SIZE] = {};
	int ret = 0;

	ret = hwinfo_read_file(FRONTAUX_CAMERA_EFUSE_FILE, buf, sizeof(buf));
	if (ret != 0) {
		printk(KERN_CRIT "frontaux camera efuse failed.");
		return;
	}
	printk(KERN_INFO "frontaux camera efuse: %s\n", buf);

	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';

	strcpy(hwinfo[FRONTAUX_CAM_EFUSE].hwinfo_buf, buf);

	return;
}
static void get_back_camera_efuse_id(void)
{
	char buf[MAX_HWINFO_SIZE] = {};
	int ret = 0;

	ret = hwinfo_read_file(BACK_CAMERA_EFUSE_FILE, buf, sizeof(buf));
	if (ret != 0) {
		printk(KERN_CRIT "back camera efuse failed.");
		return;
	}
	printk(KERN_INFO "back camera efuse: %s\n", buf);

	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';

	strcpy(hwinfo[BACK_CAM_EFUSE].hwinfo_buf, buf);

	return;
}
static void get_backaux_camera_efuse_id(void)
{
	char buf[MAX_HWINFO_SIZE] = {};
	int ret = 0;

	ret = hwinfo_read_file(BACKAUX_CAMERA_EFUSE_FILE, buf, sizeof(buf));
	if (ret != 0) {
		printk(KERN_CRIT "backaux camera efuse failed.");
		return;
	}
	printk(KERN_INFO "backaux camera efuse: %s\n", buf);

	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';

	strcpy(hwinfo[BACKAUX_CAM_EFUSE].hwinfo_buf, buf);

	return;
}

static void get_backaux2_camera_efuse_id(void)
{
	char buf[MAX_HWINFO_SIZE] = {};
	int ret = 0;

	ret = hwinfo_read_file(BACKAUX2_CAMERA_EFUSE_FILE, buf, sizeof(buf));
	if (ret != 0) {
		printk(KERN_CRIT "backaux2 camera efuse failed.");
		return;
	}
	printk(KERN_INFO "backaux2 camera efuse: %s\n", buf);

	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';

	strcpy(hwinfo[BACKAUX2_CAM_EFUSE].hwinfo_buf, buf);

	return;
}

static void get_card_present(void)
{
	char card_holder_present[BUF_SIZE];
// BEGIN Ontim, rd.zhigang.he, 8/28/2020, 9824711, St-result :PASS, detect card holder present
	int  gpio_cd = 355;

	memset(card_holder_present, '\0', BUF_SIZE);
	printk("%s: gpio(%d) value=%d\n", __func__, gpio_cd, gpio_get_value(gpio_cd));
	if (gpio_get_value(gpio_cd) == 1)
		strncpy(card_holder_present, "truly", 5);
	else
		strncpy(card_holder_present, "false", 5);
// END 9824711

	memset(hwinfo[CARD_HOLDER_PRESENT].hwinfo_buf, 0x00, sizeof(hwinfo[CARD_HOLDER_PRESENT].hwinfo_buf));
	strncpy(hwinfo[CARD_HOLDER_PRESENT].hwinfo_buf, card_holder_present,
	        ((strlen(card_holder_present) >= sizeof(hwinfo[CARD_HOLDER_PRESENT].hwinfo_buf) ?
	          sizeof(hwinfo[CARD_HOLDER_PRESENT].hwinfo_buf) : strlen(card_holder_present))));
}

static int get_pon_reason(void)
{
	char pon_reason_info[32] = "UNKNOWN";

	/*	switch ((get_boot_reason() & 0xFF))
		{
		case 0x20:
			pon_reason_info = "usb charger";
			break;
		case 0x21:
			pon_reason_info = "soft reboot";
			break;
		case 0xa0:
			pon_reason_info = "power key";
			break;
		case 0xa1:
			pon_reason_info = "hard reset";
			break;
		default:
			pon_reason_info = "unknow";
			break;
		}*/
	get_cmdline_param_val("bootcause=", pon_reason_info, sizeof(pon_reason_info));

	return sprintf(hwinfo[pon_reason].hwinfo_buf, "%s", pon_reason_info);
}

static int get_secure_boot_version(void)
{
	char is_secureboot[32] = "Unknown";
	/*	if (get_secure_boot_value())
			is_secureboot = "SE";
		else
			is_secureboot = "NSE";*/

	return sprintf(hwinfo[secboot_version].hwinfo_buf, "%s", is_secureboot);
}

static int get_dual_sim(void)
{
	unsigned int gpio_base = 343;
	unsigned int pin4 = 167;
	int pin_val = 0;

	pin_val |= (gpio_get_value(gpio_base + pin4) & 0x01) << 4;

	printk(KERN_ERR "%s: pin_val is %x ;\n", __func__, pin_val);

	return sprintf(hwinfo[dual_sim].hwinfo_buf, "%s", pin_val ? "sig" : "dual");
}

#if 0
static int get_band_id(void)
{
	unsigned int gpio_base = 343;

	unsigned int pin1 = 163;
	unsigned int pin2 = 164;
	int pin_val = 0;

	pin_val  = (gpio_get_value(gpio_base + pin2) & 0x01) << 1;
	pin_val |= (gpio_get_value(gpio_base + pin1) & 0x01);

	printk(KERN_ERR "%s: hw_ver is %x ;\n", __func__, pin_val);

	if (pin_val == 1)
		strcpy(hwinfo[band_id].hwinfo_buf, "XT2053-2");
	else if (pin_val == 0)
		strcpy(hwinfo[band_id].hwinfo_buf, "XT2053-1");
	else
		strcpy(hwinfo[band_id].hwinfo_buf, "XT2053-1");
	return 0;
}
#endif
static int set_band_id(char *src)
{
	sprintf(hwinfo[band_id].hwinfo_buf, "%s", src);
	return 0;
}

__setup("band_id=", set_band_id);

//Add board nfc flag begin --jwt 20200819
unsigned int platform_nfc_flag = 0;
EXPORT_SYMBOL(platform_nfc_flag);

static int get_nfc_flag(void)
{
	int id = platform_nfc_flag;
	return sprintf(hwinfo[board_nfc_flag].hwinfo_buf, "%04d", id);
}
//Add board nfc flag end --jwt 20200819

/* BEGIN Ontim, jiawentao, 28/09/2020, 10015326, St-result:PASS, Add project version drive device note. */
unsigned int platform_prj_ver_flag = 0;
EXPORT_SYMBOL(platform_prj_ver_flag);

static int get_project_version_flag(void)
{
	int id = platform_prj_ver_flag;
	return sprintf(hwinfo[prj_ver_flag].hwinfo_buf, "%04d", id);
}
/* END 10015326 */

unsigned int platform_board_id = 0;
EXPORT_SYMBOL(platform_board_id);
static int get_version_id(void)
{
#if 1
	int id = platform_board_id;
	return sprintf(hwinfo[board_id].hwinfo_buf, "%04d", id);
#else
	unsigned int gpio_base = 343;
	unsigned int pin0 = 165;
	unsigned int pin1 = 166;
	unsigned int pin2 = 167;
	int pin_val = 0;

	pin_val  = (gpio_get_value(gpio_base + pin2) & 0x01) << 2;
	pin_val |= (gpio_get_value(gpio_base + pin1) & 0x01) << 1;
	pin_val |= (gpio_get_value(gpio_base + pin0) & 0x01) << 0;

	printk(KERN_ERR "%s: hw_ver is %x ;\n", __func__, pin_val);

	return sprintf(hwinfo[board_id].hwinfo_buf, "0x%x", pin_val);
#endif
}

#if 0
static int get_qcn_type(void)
{
	int id = platform_board_id;
	if (id > (sizeof(board_table) / sizeof(boardid_match_t) - 1))
		id = sizeof(board_table) / sizeof(boardid_match_t) - 1;
	return sprintf(hwinfo[qcn_type].hwinfo_buf, "%s", board_table[id].qcn_type);
}
#endif

static int get_hw_version(void)
{
  int id = platform_board_id;
  if (id > (sizeof(board_table) / sizeof(boardid_match_t) - 1))
    id = sizeof(board_table) / sizeof(boardid_match_t) - 1;
  return sprintf(hwinfo[hw_version].hwinfo_buf, "%s", board_table[id].hw_version);
}

char NFC_BUF[MAX_HWINFO_SIZE] = {"Unknow"};
EXPORT_SYMBOL(NFC_BUF);
static void get_nfc_deviceinfo(void)
{
	strcpy(hwinfo[NFC_MFR].hwinfo_buf, NFC_BUF);
}

static int set_serialno(char *src)
{
	if (src == NULL)
		return 0;
	sprintf(hwinfo[serialno].hwinfo_buf, "%s", src);
	return 1;
}
__setup("androidboot.serialno=", set_serialno);

//get emmc info
char pMeminfo[MAX_HWINFO_SIZE] = {'\0'};
static int set_memory_info(char *src)
{
	if (src == NULL)
		return 0;
	sprintf(pMeminfo, "%s", src);
	return 1;
}
__setup("memory_info=", set_memory_info);

int _atoi(char * str)
{
	int value = 0;
	int sign = 1;
	int radix;

	if (*str == '-')
	{
		sign = -1;
		str++;
	}
	if (*str == '0' && (*(str + 1) == 'x' || *(str + 1) == 'X'))
	{
		radix = 16;
		str += 2;
	}
	else if (*str == '0')
	{
		radix = 8;
		str++;
	} else {
		radix = 10;
	}
	while (*str && *str != '\0')
	{
		if (radix == 16)
		{
			if (*str >= '0' && *str <= '9')
				value = value * radix + *str - '0';
			else if (*str >= 'A' && *str <= 'F')
				value = value * radix + *str - 'A' + 10;
			else if (*str >= 'a' && *str <= 'f')
				value = value * radix + *str - 'a' + 10;
		} else {
			value = value * radix + *str - '0';
		}
		str++;
	}
	return sign * value;
}

static char *get_bootdevice(void)
{
	static char bootdevice[BUF_SIZE] = {0};
	if ( ! *bootdevice )
		get_cmdline_param_val("androidboot.boot_devices=", bootdevice, sizeof(bootdevice));
	return bootdevice;
}

static char ufs_file[255];

#define UFS_CAPACITY_FILE "/sys/devices/platform/%s/geometry_descriptor/raw_device_capacity"
static void get_ufs_capacity(void)
{
	char buf[MAX_HWINFO_SIZE] = {'\0'};
	int ret = 0;
	int cap = 0;

	//read ufs capacity
	memset(buf, 0x00, sizeof(buf));
	sprintf(ufs_file, UFS_CAPACITY_FILE, get_bootdevice());
	ret = hwinfo_read_file(ufs_file, buf, sizeof(buf) - 1);
	printk(KERN_INFO "Read %s (ret=%d)\n", ufs_file, ret);
	if (ret != 0)
		return;
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';
	cap = _atoi(buf) / (1024 * 1024 * 2);
	if (cap < 4)
		cap = 4;
	else if (cap > 6 && cap < 8)
		cap = 8;
	else if (cap > 8 && cap < 16)
		cap = 16;
	else if (cap < 32 && cap > 16)
		cap = 32;
	else if (cap < 64 && cap > 32)
		cap = 64;
	else if (cap < 128 && cap > 100)
		cap = 128;
	sprintf(hwinfo[ufs_capacity].hwinfo_buf, "%dGB", cap);
	printk(KERN_INFO "%s: %s (%dGB)\n", __func__, buf, cap);
}

#define UFS_LIFE_A_FILE "/sys/devices/platform/%s/health_descriptor/life_time_estimation_a"
#define UFS_LIFE_B_FILE "/sys/devices/platform/%s/health_descriptor/life_time_estimation_b"
static void get_ufs_life(void)
{
	char buf[MAX_HWINFO_SIZE] = {'\0'};
	int ret = 0;

	//read ufs life A
	memset(buf, 0x00, sizeof(buf));
	sprintf(ufs_file, UFS_LIFE_A_FILE, get_bootdevice());
	ret = hwinfo_read_file(ufs_file, buf, sizeof(buf) - 1);
	printk(KERN_INFO "Read %s (ret=%d)\n", ufs_file, ret);
	if (ret != 0)
		return;
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';
	printk(KERN_INFO "%s: %s\n", __func__, buf);

	strcpy(hwinfo[ufs_life].hwinfo_buf, buf);

	//read ufs life B
	memset(buf, 0x00, sizeof(buf));
	sprintf(ufs_file, UFS_LIFE_B_FILE, get_bootdevice());
	ret = hwinfo_read_file(ufs_file, buf, sizeof(buf) - 1);
	printk(KERN_INFO "Read %s (ret=%d)\n", ufs_file, ret);
	if (ret != 0)
		return;
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';
	printk(KERN_INFO "%s: %s\n", __func__, buf);

	// combine life A and B
	strcat(hwinfo[ufs_life].hwinfo_buf, " ");
	strcat(hwinfo[ufs_life].hwinfo_buf, buf);
}

#define UFS_MF_DATE_FILE "/sys/devices/platform/%s/device_descriptor/manufacturing_date"
static void get_ufs_manufacturing_date(void)
{
	char buf[MAX_HWINFO_SIZE] = {'\0'};
	int ret = 0;

	//read ufs manufacturing_date
	memset(buf, 0x00, sizeof(buf));
	sprintf(ufs_file,UFS_MF_DATE_FILE , get_bootdevice());
	ret = hwinfo_read_file(ufs_file, buf, sizeof(buf) - 1);
	printk(KERN_INFO "Read %s (ret=%d)\n", ufs_file, ret);
	if (ret != 0)
		return;
	printk(KERN_INFO "%s: %s\n", __func__, buf);
	if (strlen(buf) >= 6)
		sprintf(hwinfo[ufs_manufacturing_date].hwinfo_buf, "%c%c/%c%c", buf[2], buf[3], buf[4], buf[5]);
}

#define UFS_MFR_FILE "/sys/devices/platform/%s/string_descriptors/manufacturer_name"
static void get_ufs_mfr(void)
{
	char buf[MAX_HWINFO_SIZE] = {'\0'};
	int ret = 0;

	//read ufs mfr
	memset(buf, 0x00, sizeof(buf));
	sprintf(ufs_file, UFS_MFR_FILE, get_bootdevice());
	ret = hwinfo_read_file(ufs_file, buf, sizeof(buf) - 1);
	printk(KERN_INFO "Read %s (ret=%d)\n", ufs_file, ret);
	if (ret != 0)
		return;
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';
	printk(KERN_INFO "%s: %s\n", __func__, buf);
	strcpy(hwinfo[ufs_mfr].hwinfo_buf, buf);
	strcpy(hwinfo[lpddr_mfr].hwinfo_buf, buf);
}

#define UFS_OEM_ID_FILE "/sys/devices/platform/%s/string_descriptors/oem_id"
static void get_ufs_oem_id(void)
{
	char buf[MAX_HWINFO_SIZE] = {'\0'};
	int ret = 0;

	//read ufs oem_id
	memset(buf, 0x00, sizeof(buf));
	sprintf(ufs_file, UFS_OEM_ID_FILE, get_bootdevice());
	ret = hwinfo_read_file(ufs_file, buf, sizeof(buf) - 1);
	printk(KERN_INFO "Read %s (ret=%d)\n", ufs_file, ret);
	if (ret != 0)
		return;
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';
	printk(KERN_INFO "%s: %s\n", __func__, buf);
	strcpy(hwinfo[ufs_oem_id].hwinfo_buf, buf);
}

#define UFS_PRODUCT_NAME_FILE "/sys/devices/platform/%s/string_descriptors/product_name"
static void get_ufs_product_name(void)
{
	char buf[MAX_HWINFO_SIZE] = {'\0'};
	int ret = 0;

	//read ufs product_name
	memset(buf, 0x00, sizeof(buf));
	sprintf(ufs_file, UFS_PRODUCT_NAME_FILE, get_bootdevice());
	ret = hwinfo_read_file(ufs_file, buf, sizeof(buf) - 1);
	printk(KERN_INFO "Read %s (ret=%d)\n", ufs_file, ret);
	if (ret != 0)
		return;
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';
	printk(KERN_INFO "%s: %s\n", __func__, buf);
	strcpy(hwinfo[ufs_product_name].hwinfo_buf, buf);
}

#define UFS_PRODUCT_REV_FILE "/sys/devices/platform/%s/string_descriptors/product_revision"
static void get_ufs_product_revision(void)
{
	char buf[MAX_HWINFO_SIZE] = {'\0'};
	int ret = 0;

	//read ufs product_revision
	memset(buf, 0x00, sizeof(buf));
	sprintf(ufs_file, UFS_PRODUCT_REV_FILE, get_bootdevice());
	ret = hwinfo_read_file(ufs_file, buf, sizeof(buf) - 1);
	printk(KERN_INFO "Read %s (ret=%d)\n", ufs_file, ret);
	if (ret != 0)
		return;
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';
	printk(KERN_INFO "%s: %s\n", __func__, buf);
	strcpy(hwinfo[ufs_product_revision].hwinfo_buf, buf);
}

#define UFS_SN_FILE "/sys/devices/platform/%s/string_descriptors/serial_number"
static void get_ufs_sn(void)
{
	char buf[MAX_HWINFO_SIZE] = {'\0'};
	int ret = 0;

	//read ufs sn
	memset(buf, 0x00, sizeof(buf));
	sprintf(ufs_file, UFS_SN_FILE, get_bootdevice());
	ret = hwinfo_read_file(ufs_file, buf, sizeof(buf) - 1);
	printk(KERN_INFO "Read %s (ret=%d)\n", ufs_file, ret);
	if (ret != 0)
		return;
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';
	printk(KERN_INFO "%s: %s\n", __func__, buf);
	strcpy(hwinfo[ufs_sn].hwinfo_buf, buf);
}

#define UFS_SPEC_VER_FILE "/sys/devices/platform/%s/device_descriptor/specification_version"
static void get_ufs_spec_ver(void)
{
	char buf[MAX_HWINFO_SIZE] = {'\0'};
	int ret = 0;

	//read ufs wwid
	memset(buf, 0x00, sizeof(buf));
	sprintf(ufs_file, UFS_SPEC_VER_FILE, get_bootdevice());
	ret = hwinfo_read_file(ufs_file, buf, sizeof(buf) - 1);
	printk(KERN_INFO "Read %s (ret=%d)\n", ufs_file, ret);
	if (ret != 0)
		return;
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';
	printk(KERN_INFO "%s: %s\n", __func__, buf);
	strcpy(hwinfo[ufs_spec_ver].hwinfo_buf, buf);
}

#define UFS_WWID_FILE "/sys/devices/platform/%s/host0/target0:0:0/0:0:0:0/wwid"
static void get_ufs_wwid(void)
{
	char buf[MAX_HWINFO_SIZE] = {'\0'};
	int ret = 0;

	//read ufs wwid
	memset(buf, 0x00, sizeof(buf));
	sprintf(ufs_file, UFS_WWID_FILE, get_bootdevice());
	ret = hwinfo_read_file(ufs_file, buf, sizeof(buf) - 1);
	printk(KERN_INFO "Read %s (ret=%d)\n", ufs_file, ret);
	if (ret != 0)
		return;
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';
	printk(KERN_INFO "%s: %s\n", __func__, buf);
	strcpy(hwinfo[ufs_wwid].hwinfo_buf, buf);
}

// cid exists in EMMC, but not in ufs, so use related to fields to generate a similar one
static void get_ufs_info(void)
{
	get_ufs_mfr();
	get_ufs_product_name();
	get_ufs_product_revision();
	get_ufs_manufacturing_date();

	snprintf(hwinfo[ufs_info].hwinfo_buf, sizeof(hwinfo[ufs_info].hwinfo_buf), "%s %s %s %s",
		hwinfo[ufs_mfr].hwinfo_buf,
		hwinfo[ufs_product_name].hwinfo_buf,
		hwinfo[ufs_product_revision].hwinfo_buf,
		hwinfo[ufs_manufacturing_date].hwinfo_buf);

	printk(KERN_INFO "%s: %s\n", __func__, hwinfo[ufs_info].hwinfo_buf);
}

#define UFS_CID_LEN 32
static void get_ufs_cid(void)
{
	int n, i;
	int len;
	char *src;
	char *dst;
	get_ufs_sn();
	src = hwinfo[ufs_sn].hwinfo_buf;
	dst = hwinfo[ufs_cid].hwinfo_buf;
	len = strlen(src);
	if ((src[0] == '0') && (src[1] == '0')) {
		// unicode case
		len = min(len / 4, UFS_CID_LEN / 2);
		for (n = 0, i = UFS_CID_LEN / 2 - len; i > 0; i--)
			n += sprintf(dst + n, "00");
		for (i = 0; i < len; i++) {
			dst[n + 2 * i] = src[4 * i + 2];
			dst[n + 2 * i + 1] = src[4 * i + 3];
		}
		dst[n + 2 * i] = 0;
	} else {
		// asc case
		len = min(len, UFS_CID_LEN);
		for (n = 0, i = UFS_CID_LEN - len; i > 0; i--)
			n += sprintf(dst + n, "0");
		snprintf(dst + n, len + 1, src);
	}
	printk(KERN_INFO "%s: %s\n", __func__, hwinfo[ufs_cid].hwinfo_buf);
}


#define BYTE(_x) (_x<<0x03)
#define EMMC_SN_FILE     "/sys/class/mmc_host/mmc0/mmc0:0001/serial"
static void get_emmc_sn(void)
{
	char buf[MAX_HWINFO_SIZE] = {'\0'};
	int ret = 0;

	//read emmc sn
	memset(buf, 0x00, sizeof(buf));
	ret = hwinfo_read_file(EMMC_SN_FILE, buf, sizeof(buf));
	if (ret != 0) {
		printk(KERN_CRIT "EMMC_SN_FILE failed.");
		return;
	}

	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';
	memset(hwinfo[emmc_sn].hwinfo_buf, 0x00, sizeof(hwinfo[emmc_sn].hwinfo_buf));
	strncpy(hwinfo[emmc_sn].hwinfo_buf, buf, strlen(buf));
	printk("%s: emmc_sn_buf:%s\n", __func__, buf);
}

#define EMMC_CID_FILE     "/sys/class/mmc_host/mmc0/mmc0:0001/cid"
static void get_emmc_cid(void)
{
	char buf[MAX_HWINFO_SIZE] = {'\0'};
	int ret = 0;

	//read emmc cid
	memset(buf, 0x00, sizeof(buf));
	ret = hwinfo_read_file(EMMC_CID_FILE, buf, sizeof(buf));
	if (ret != 0) {
		printk(KERN_CRIT "EMMC_CID_FILE failed.");
		return;
	}
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';
	memset(hwinfo[emmc_cid].hwinfo_buf, 0x00, sizeof(hwinfo[emmc_cid].hwinfo_buf));
	strncpy(hwinfo[emmc_cid].hwinfo_buf, buf, strlen(buf));
	printk("%s: emmc_cid_buf:%s\n", __func__, buf);
}

#define LPDDR_SIZE_FILE   "/proc/meminfo"
static void get_ddr_cap(void)
{
	int lpddr_cap = 0;
	char buf[MAX_HWINFO_SIZE] = {'\0'};
	char* p = buf;
	char* q = buf;
	int ret = 0;

	memset(buf, 0x00, sizeof(buf));
	ret = hwinfo_read_file(LPDDR_SIZE_FILE, buf, sizeof(buf));
	if (ret != 0) {
		printk(KERN_CRIT "LPDDR_SIZE_FILE failed.");
		return;
	}

	while (*p++ != ':' && p < buf + MAX_HWINFO_SIZE);
	while (*p++ == ' ' && p < buf + MAX_HWINFO_SIZE);
	p = p - 1;
	q = p;
	while (*q++ != ' ' && q < buf + MAX_HWINFO_SIZE);
	memcpy(buf, p, q - p - 1);
	buf[q - p - 1] = '\0';
	buf[q - p]   = '\0';
	lpddr_cap = _atoi(buf) / (1024 * 1024);
	printk("%s: buf:%s i = %d\n", __func__, buf, lpddr_cap);
	if (lpddr_cap < 2)
		lpddr_cap = 2;
	else if (lpddr_cap < 3 && lpddr_cap >= 2)
		lpddr_cap = 3;
	else if (lpddr_cap < 4 && lpddr_cap >= 3)
		lpddr_cap = 4;
	else if (lpddr_cap < 6 && lpddr_cap >= 5)
		lpddr_cap = 6;
	else if (lpddr_cap < 8 && lpddr_cap >= 7)
		lpddr_cap = 8;
// BEGIN Ontim, rd.zhigang.he, 9/8/2020, 9888845, St-result :PASS, add FORESEE emmc info and update hwinfo
	sprintf(hwinfo[lpddr_capacity].hwinfo_buf, "%dGB", lpddr_cap);
// END 9888845
}

#define EMMC_LIFE_TIME_FILE "/sys/class/mmc_host/mmc0/mmc0:0001/life_time"
static void get_emmc_lifetime(void)
{
	char buf[MAX_HWINFO_SIZE] = {'\0'};
	int ret = 0;

	//read emmc life_time
	memset(buf, 0x00, sizeof(buf));
	ret = hwinfo_read_file(EMMC_LIFE_TIME_FILE, buf, sizeof(buf));
	if (ret != 0) {
		printk(KERN_CRIT "EMMC_LIFE_TIME_FILE failed.");
		return;
	}
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';
	strncpy(hwinfo[emmc_life].hwinfo_buf, buf, strlen(buf));
	printk("%s:emmc life %s \n", __func__, buf);
}

#define EMMC_SIZE_FILE   "/sys/class/mmc_host/mmc0/mmc0:0001/block/mmcblk0/size"
static void get_emmc_size(void)
{
	int emmc_cap = 0;
	char buf[MAX_HWINFO_SIZE] = {'\0'};
	int ret = 0;

	memset(buf, 0x00, sizeof(buf));
	ret = hwinfo_read_file(EMMC_SIZE_FILE, buf, sizeof(buf));
	if (ret != 0) {
		printk(KERN_CRIT "EMMC_SIZE_FILE failed.");
		return;
	}
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';
	emmc_cap = _atoi(buf) / (1024 * 1024 * 2);

	if (emmc_cap < 4)
		emmc_cap = 4;
	else if (emmc_cap > 6 && emmc_cap < 8)
		emmc_cap = 8;
	else if (emmc_cap > 8 && emmc_cap < 16)
		emmc_cap = 16;
	else if (emmc_cap < 32 && emmc_cap > 16)
		emmc_cap = 32;
	else if (emmc_cap < 64 && emmc_cap > 32)
		emmc_cap = 64;
	else if (emmc_cap < 128 && emmc_cap > 100)
		emmc_cap = 128;

	sprintf(hwinfo[emmc_capacity].hwinfo_buf, "%dGB", emmc_cap);
}

#define EMMC_MANFID_FILE "/sys/class/mmc_host/mmc0/mmc0:0001/manfid"
static void get_emmc_mfr(void)
{
	unsigned char emmc_mid = 0;
	const char *emmc_mid_name;
	char buf[MAX_HWINFO_SIZE] = {'\0'};
	int ret = 0;

	ret = hwinfo_read_file(EMMC_MANFID_FILE, buf, sizeof(buf));
	if (ret != 0) {
		printk(KERN_CRIT "EMMC_MANFID_FILE failed.");
		return;
	}
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';
	emmc_mid = _atoi(buf);

	emmc_mid_name = foreach_emmc_table(emmc_mid);
// BEGIN Ontim, rd.zhigang.he, 9/8/2020, 9888845, St-result :PASS, add FORESEE emmc info and update hwinfo
	if (emmc_mid_name == NULL) {
		printk(KERN_CRIT "cannot recognize emmc mid=0x%x", emmc_mid);
		emmc_mid_name = "Unknown";
	}
// END 9888845
	strncpy(hwinfo[emmc_mfr].hwinfo_buf, emmc_mid_name, strlen(emmc_mid_name));
	strncpy(hwinfo[lpddr_mfr].hwinfo_buf, emmc_mid_name, strlen(emmc_mid_name));
}
//extern int  meta_camera_info(void);
/*
// get_current_cpuid for imie
//extern u32 get_devinfo_with_index(u32 index);
#define CPUID_REG_INDEX 12
#define CPUID_REG_NUM 4

static void get_current_cpuid(void)
{
	u32 ontim_cpuid = 0 ;
	int i =0 ;
	char temp_buffer[MAX_HWINFO_SIZE]={0};
	for(i = CPUID_REG_INDEX;i<(CPUID_REG_INDEX+CPUID_REG_NUM);i++){
		ontim_cpuid = get_devinfo_with_index(i);
		sprintf(temp_buffer+strlen(temp_buffer),"%02x%02x%02x%02x", ontim_cpuid&0xFF, (ontim_cpuid>>8)&0xFF, 
		    (ontim_cpuid>>16)&0xFF, (ontim_cpuid>>24)&0xFF);
	}
	sprintf(hwinfo[current_cpuid].hwinfo_buf,"%s",temp_buffer);
}
*/
#ifdef CONFIG_MTK_BOOT //mtk remove get_boot_mode() function
extern unsigned int get_boot_mode(void);
#endif
/* BEGIN Ontim, jiawentao, 28/09/2020, 10015326, St-result:PASS, Add project version drive device note. */
#ifdef CONFIG_HBM_SUPPORT

bool g_hbm_enable = false;
extern unsigned int g_last_level;
extern int hbm_set_backlight_level(unsigned int level);

static int set_hbm_status(const char * buf, int n)
{
	printk("hbm user buf:%s\n", buf);

#ifdef SMT_VERSION
	printk("SMT version,No hbm");
#else
	switch (buf[0]){
		case '0':
			if (!g_hbm_enable) {
				printk("Have been disabled hbm, exit!\n");
				break;
			}
			g_hbm_enable = false;
			hbm_set_backlight_level(g_last_level);
			break;
		case '3':
			if (g_hbm_enable) {
				printk("Have been enabled hbm, exit!\n");
				break;
			}
			hbm_set_backlight_level(256);
			g_hbm_enable = true;
			break;
		default:
			g_hbm_enable = false;
			break;
	}
#endif
	return 0;
}

static void get_hbm_status(void)
{
	char hbm_str_st[8] = {0};
	if (g_hbm_enable){
	    strcpy(hbm_str_st, "hbm:on");
	}else{
	    strcpy(hbm_str_st, "hbm:off");
	}
	sprintf(hwinfo[hbm].hwinfo_buf,"%s",hbm_str_st);
}
#endif

static ssize_t hwinfo_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	int i = 0;
#ifdef CONFIG_MTK_BOOT
	static int flag = 0;
	int boot_mode ;
	boot_mode = (int)get_boot_mode();
	if (boot_mode == META_BOOT) {
			if(0==flag)
			//meta_camera_info();
		flag=1;
	}
#endif

	printk(KERN_INFO "hwinfo sys node %s \n", attr->attr.name);

	for (; i < HWINFO_MAX && strcmp(hwinfo[i].hwinfo_name, attr->attr.name) && ++i;);

	switch (i)
	{
	case cpu_type:
		get_cpu_type();
		break;
	case NFC_MFR:
		get_nfc_deviceinfo();
		break;
	case SPEAKER_MFR:
		get_speaker_mfr();
		break;
	case POWER_USB_TYPE:
		get_power_usb_type();
		break;
	case BATTARY_MFR:
		get_battary_mfr();
		break;
	case BATTARY_VOL:
		get_battary_vol();
		break;
	case BATTARY_CAP:
		get_battary_cap();
		break;
	case battery_input_suspend:
		get_battery_input_suspend();
		break;
	case battery_charging_enabled:
		get_battery_charging_enabled();
		break;
	case dual_sim:
		get_dual_sim();
		break;
	//case band_id:
	//	get_band_id();
	//	break;
	case board_id:
		get_version_id();
		break;
	case board_nfc_flag:
		get_nfc_flag();
		break;
	case prj_ver_flag:
		get_project_version_flag();
		break;
	case hw_version:
		get_hw_version();
		break;
#if 0
	case qcn_type:
		get_qcn_type();
		break;
#endif
	case LCD_MFR:
		get_lcd_type();
		break;
	case TP_MFR:
		get_tp_info();
		break;
	case TYPEC_MFR:
		get_typec_vendor();
		break;
	case TYPEC_CC_STATUS:
		get_typec_cc_status();
		break;
	case ADB_SN:
		get_adb_sn();
		break;
	case FRONT_CAM_MFR:
		get_front_camera_id();
		break;
	case FRONTAUX_CAM_MFR:
		get_frontaux_camera_id();
		break;
	case BACK_CAM_EFUSE:
		get_back_camera_efuse_id();
		break;
	case BACKAUX_CAM_EFUSE:
		get_backaux_camera_efuse_id();
		break;
	case BACKAUX2_CAM_EFUSE:
		get_backaux2_camera_efuse_id();
		break;
	case FRONT_CAM_EFUSE:
		get_front_camera_efuse_id();
		break;
	case FRONTAUX_CAM_EFUSE:
		get_frontaux_camera_efuse_id();
		break;
	case BACK_CAM_MFR:
		get_back_camera_id();
		break;
	case BACKAUX_CAM_MFR:
		get_backaux_camera_id();
		break;
	case BACKAUX2_CAM_MFR:
		get_backaux2_camera_id();
		break;
	case BACKAUX2_CAM_OTP_STATUS:
		get_backaux2_camera_otp_status();
		break; 
	case BACKAUX_CAM_OTP_STATUS:
		get_backaux_camera_otp_status();
		break;
	case BACK_CAM_OTP_STATUS:
		get_back_camera_otp_status();
		break;
	case FRONT_CAM_OTP_STATUS:
		get_front_camera_otp_status();
		break;
	case FP_MFR:
		get_fingerprint_id();
		break;
	case GSENSOR_MFR:
		get_gsensor_id();
		break;
	case ALSPS_MFR:
		get_alsps_id();
		break;
	case CARD_HOLDER_PRESENT:
		get_card_present();
		break;
	case CHARGER_IC_MFR: //houzn add
		get_charger_ic();
		break;
#ifdef CONFIG_USB_CABLE
	case RF_GPIO:
		get_rfgpio_state();
		break;
#endif
	case pon_reason:
		get_pon_reason();
		break;
	case secboot_version:
		get_secure_boot_version();
		break;
	case ufs_capacity:
		get_ufs_capacity();
		break;
	case ufs_info:
		get_ufs_info();
		break;
	case ufs_cid:
		get_ufs_cid();
		break;
	case ufs_life:
		get_ufs_life();
		break;
	case ufs_manufacturing_date:
		get_ufs_manufacturing_date();
		break;
	case ufs_mfr:
		get_ufs_mfr();
		break;
	case ufs_oem_id:
		get_ufs_oem_id();
		break;
	case ufs_product_name:
		get_ufs_product_name();
		break;
	case ufs_product_revision:
		get_ufs_product_revision();
		break;
	case ufs_sn:
		get_ufs_sn();
		break;
	case ufs_spec_ver:
		get_ufs_spec_ver();
		break;
	case ufs_wwid:
		get_ufs_wwid();
		break;
	case emmc_sn:
		get_emmc_sn();
		break;
	case emmc_cid:
		get_emmc_cid();
		break;
	case emmc_mfr:
		get_emmc_mfr();
		break;
	case emmc_capacity:
		get_emmc_size();
		break;
	case emmc_life:
		get_emmc_lifetime();
		break;
	case lpddr_mfr:
		get_ufs_mfr();
		get_emmc_mfr();
	case lpddr_capacity:
		get_ddr_cap();
		break;
	//case current_cpuid:
	//	get_current_cpuid();
	//	break;
#ifdef CONFIG_HBM_SUPPORT
	case hbm:
		get_hbm_status();
		break;
#endif
	default:
		break;
	}
	return sprintf(buf, "%s=%s \n",  attr->attr.name, ((i >= HWINFO_MAX || hwinfo[i].hwinfo_buf[0] == '\0') ? "unknow" : hwinfo[i].hwinfo_buf));
}
/* END 10015326 */

static ssize_t hwinfo_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	int i = 0;
	printk(KERN_INFO "hwinfo sys node %s \n", attr->attr.name);

	for (; i < HWINFO_MAX && strcmp(hwinfo[i].hwinfo_name, attr->attr.name) && ++i;);

	switch (i)
	{
	case battery_input_suspend:
		put_battery_input_suspend(buf, n);
		break;
	case battery_charging_enabled:
		put_battery_charging_enabled(buf, n);
		break;
	case BACKAUX2_CAM_OTP_STATUS:
		set_backaux2_camera_otp_status(buf, n);
		break; 
	case BACKAUX_CAM_OTP_STATUS:
		set_backaux_camera_otp_status(buf, n);
		break;
	case BACK_CAM_OTP_STATUS:
		set_back_camera_otp_status(buf, n);
		break;
	case FRONT_CAM_OTP_STATUS:
		set_front_camera_otp_status(buf, n);
		break;
/*
	case FRONT_CAM_MFR:
		set_front_camera_id(buf, n);
		break;
	case FRONTAUX_CAM_MFR:
		set_frontaux_camera_id(buf, n);
		break;
	case BACK_CAM_MFR:
		set_back_camera_id(buf, n);
		break;
	case BACKAUX_CAM_MFR:
		set_backaux_camera_id(buf, n);
		break;
	case BACKAUX2_CAM_MFR:
		set_backaux2_camera_id(buf, n);
		break;
	case FRONT_CAM_EFUSE:
		set_front_camera_efuse_id(buf, n);
		break;
	case FRONTAUX_CAM_EFUSE:
		set_frontaux_camera_efuse_id(buf, n);
		break;
	case BACK_CAM_EFUSE:
		set_back_camera_efuse_id(buf, n);
		break;
	case BACKAUX_CAM_EFUSE:
		set_backaux_camera_efuse_id(buf, n);
		break;
	case BACKAUX2_CAM_EFUSE:
		set_backaux2_camera_efuse_id(buf, n);
		break;
*/
#ifdef CONFIG_HBM_SUPPORT
	case hbm:
		set_hbm_status(buf, n);
		break;
#endif
	default:
		break;
	};
	return n;
}
#define KEYWORD(_name) \
    static struct kobj_attribute hwinfo##_name##_attr = {   \
                .attr   = {                             \
                        .name = __stringify(_name),     \
                        .mode = 0644,                   \
                },                                      \
            .show   = hwinfo_show,                 \
            .store  = hwinfo_store,                \
        };

#include "hwinfo.h"
#undef KEYWORD

#define KEYWORD(_name)\
    [_name] = &hwinfo##_name##_attr.attr,

static struct attribute * g[] = {
#include "hwinfo.h"
	NULL
};
#undef KEYWORD

static struct attribute_group attr_group = {
	.attrs = g,
};

int ontim_hwinfo_register(enum HWINFO_E e_hwinfo, char *hwinfo_name)
{
	if ((e_hwinfo >= HWINFO_MAX) || (hwinfo_name == NULL))
		return -1;
	strncpy(hwinfo[e_hwinfo].hwinfo_buf, hwinfo_name, \
	        (strlen(hwinfo_name) >= 20 ? 19 : strlen(hwinfo_name)));
	return 0;
}
EXPORT_SYMBOL(ontim_hwinfo_register);

static int __init hwinfo_init(void)
{
	struct kobject *k_hwinfo = NULL;
	char *bootdevice = get_bootdevice();
	int  isUFS = strstr(bootdevice, "ufs") ? 1 : 0;

	if ( (k_hwinfo = kobject_create_and_add("hwinfo", NULL)) == NULL ) {
		printk(KERN_ERR "%s:hwinfo sys node create error \n", __func__);
	}

	if ( sysfs_create_group(k_hwinfo, &attr_group) ) {
		printk(KERN_ERR "%s: sysfs_create_group failed\n", __func__);
	}
	printk(KERN_INFO "bootdevice=%s\n", bootdevice);
	if (isUFS) {
		sysfs_remove_file_from_group(k_hwinfo, attr_group.attrs[emmc_mfr], NULL);
		sysfs_remove_file_from_group(k_hwinfo, attr_group.attrs[emmc_life], NULL);
		sysfs_remove_file_from_group(k_hwinfo, attr_group.attrs[emmc_sn], NULL);
		sysfs_remove_file_from_group(k_hwinfo, attr_group.attrs[emmc_cid], NULL);
		sysfs_remove_file_from_group(k_hwinfo, attr_group.attrs[emmc_capacity], NULL);
	} else {
		sysfs_remove_file_from_group(k_hwinfo, attr_group.attrs[ufs_capacity], NULL);
		sysfs_remove_file_from_group(k_hwinfo, attr_group.attrs[ufs_cid], NULL);
		sysfs_remove_file_from_group(k_hwinfo, attr_group.attrs[ufs_info], NULL);
		sysfs_remove_file_from_group(k_hwinfo, attr_group.attrs[ufs_life], NULL);
		sysfs_remove_file_from_group(k_hwinfo, attr_group.attrs[ufs_manufacturing_date], NULL);
		sysfs_remove_file_from_group(k_hwinfo, attr_group.attrs[ufs_mfr], NULL);
		sysfs_remove_file_from_group(k_hwinfo, attr_group.attrs[ufs_oem_id], NULL);
		sysfs_remove_file_from_group(k_hwinfo, attr_group.attrs[ufs_product_name], NULL);
		sysfs_remove_file_from_group(k_hwinfo, attr_group.attrs[ufs_product_revision], NULL);
		sysfs_remove_file_from_group(k_hwinfo, attr_group.attrs[ufs_sn], NULL);
		sysfs_remove_file_from_group(k_hwinfo, attr_group.attrs[ufs_spec_ver], NULL);
		sysfs_remove_file_from_group(k_hwinfo, attr_group.attrs[ufs_wwid], NULL);
	}
    //get_current_cpuid();

#ifdef CONFIG_HBM_SUPPORT
	ontim_hwinfo_register(hbm, "hbm");
#endif

	//arch_read_hardware_id = msm_read_hardware_id;
	//printk(KERN_ERR "%s:hwinfo sys node create success \n", __func__);
	return 0;
}

static void __exit hwinfo_exit(void)
{
	return ;
}

late_initcall_sync(hwinfo_init);
module_exit(hwinfo_exit);
MODULE_AUTHOR("eko@ontim.com");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Product Hardward Info Exposure");
