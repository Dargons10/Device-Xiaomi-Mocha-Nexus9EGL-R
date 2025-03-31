# Inherit device configuration for mocha.
$(call inherit-product, device/xiaomi/mocha/full_mocha.mk)

# Boot Animtion
TARGET_BOOTANIMATION_HALF_RES := true
# Inherit some common lineage stuff.
$(call inherit-product, vendor/lineage/config/common_full_tablet_wifionly.mk)


TARGET_GAPPS_ARCH := arm
TARGET_BOOT_ANIMATION_RES := 1080

PRODUCT_NAME := aosp_mocha
PRODUCT_DEVICE := mocha
PRODUCT_BRAND := xiaomi
PRODUCT_MANUFACTURER := Xiaomi
BOARD_VENDOR := Xiaomi

PRODUCT_GMS_CLIENTID_BASE := android-xiaomi


  
