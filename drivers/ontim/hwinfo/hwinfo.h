#ifndef KEYWORD

#define KEYWORD_ENUM
#define KEYWORD(symbol) symbol,
/* BEGIN Ontim, jiawentao, 28/09/2020, 10015326, St-result:PASS, Add project version drive device note. */
enum HWINFO_E{
#endif

KEYWORD(cpu_type)
KEYWORD(NFC_MFR)
KEYWORD(pon_reason)
KEYWORD(secboot_version)
KEYWORD(board_id)
KEYWORD(board_nfc_flag)
KEYWORD(prj_ver_flag)
KEYWORD(hw_version)
KEYWORD(band_id)
//KEYWORD(qcn_type)
KEYWORD(serialno)

// Nodes for ufs
KEYWORD(ufs_capacity)
KEYWORD(ufs_info)
KEYWORD(ufs_cid)
KEYWORD(ufs_life)
KEYWORD(ufs_manufacturing_date)
KEYWORD(ufs_mfr)
KEYWORD(ufs_oem_id)
KEYWORD(ufs_product_name)
KEYWORD(ufs_product_revision)
KEYWORD(ufs_sn)
KEYWORD(ufs_spec_ver)
KEYWORD(ufs_wwid)

// Nodes for emmc
KEYWORD(emmc_mfr)
KEYWORD(emmc_life)
KEYWORD(emmc_sn)
KEYWORD(emmc_cid)
KEYWORD(emmc_capacity)
KEYWORD(lpddr_mfr)
KEYWORD(dual_sim)
KEYWORD(lpddr_capacity)
KEYWORD(SPEAKER_MFR)    //Speaker box mfr
KEYWORD(FP_MFR)         //Fingerprint mfr
KEYWORD(GSENSOR_MFR)         //Fingerprint mfr
KEYWORD(ALSPS_MFR)         //Fingerprint mfr
KEYWORD(LCD_MFR)        //LCD mfr
KEYWORD(TP_MFR)         //Touch mfr
KEYWORD(POWER_USB_TYPE)    //battary  mfr
KEYWORD(BATTARY_MFR)    //battary  mfr
KEYWORD(BATTARY_VOL)    //battary  voltage_now
KEYWORD(BATTARY_CAP)    //battary  capacity
KEYWORD(battery_charging_enabled)    //battary  capacity
KEYWORD(battery_input_suspend)    //battary  capacity
KEYWORD(ADB_SN)         //adb sn
KEYWORD(TYPEC_MFR)      //Typec  mfr
KEYWORD(TYPEC_CC_STATUS)      //Typec  mfr
KEYWORD(BACK_CAM_MFR)         //camera  mfr
KEYWORD(BACKAUX_CAM_MFR)      //camera  mfr
KEYWORD(BACKAUX2_CAM_MFR)	  //camera	mfr

KEYWORD(BACKAUX2_CAM_OTP_STATUS)	  //camera	mfr
KEYWORD(BACKAUX_CAM_OTP_STATUS)	  //camera	mfr
KEYWORD(BACK_CAM_OTP_STATUS)	  //camera	mfr
KEYWORD(FRONT_CAM_OTP_STATUS)	  //camera	mfr

KEYWORD(FRONT_CAM_MFR)        //camera  mfr
KEYWORD(FRONTAUX_CAM_MFR)        //camera  mfr
KEYWORD(BACK_CAM_EFUSE)         //camera  efuse
KEYWORD(BACKAUX_CAM_EFUSE)      //camera  efuse
KEYWORD(BACKAUX2_CAM_EFUSE)		//camera  efuse
KEYWORD(FRONT_CAM_EFUSE)        //camera  efuse
KEYWORD(FRONTAUX_CAM_EFUSE)        //camera  efuse
KEYWORD(CARD_HOLDER_PRESENT)  //card hold detect
KEYWORD(CHARGER_IC_MFR) //houzn add

#ifdef CONFIG_HBM_SUPPORT
KEYWORD(hbm)
#endif

#ifdef CONFIG_USB_CABLE
KEYWORD(RF_GPIO)
#endif
//KEYWORD(current_cpuid)// current cpuid

#ifdef KEYWORD_ENUM
KEYWORD(HWINFO_MAX)
};
/* END 10015326 */
int smartisan_hwinfo_register(enum HWINFO_E e_hwinfo,char *hwinfo_name);
#undef KEYWORD_ENUM
#undef KEYWORD

#endif
