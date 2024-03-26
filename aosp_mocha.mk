# Inherit device configuration for mocha.
$(call inherit-product, device/xiaomi/mocha/full_mocha.mk)

# Inherit some common aosp stuff.
$(call inherit-product, vendor/aosp/config/common.mk)


TARGET_GAPPS_ARCH := arm
TARGET_BOOT_ANIMATION_RES := 1080

PRODUCT_NAME := aosp_mocha
PRODUCT_DEVICE := mocha
PRODUCT_BAND := xiaomi
PRODUCT_MANUFACTURER := Xiaomi
BOARD_VENDOR := Xiaomi

PRODUCT_BUILD_PROP_OVERRIDES += BUILD_FINGERPRINT=Xiaomi/carbon_mocha/mocha:5.1.1/LMY49J/7fd38a3d2b:user/release-keys

PRODUCT_GMS_CLIENTID_BASE := android-xiaomi


  
