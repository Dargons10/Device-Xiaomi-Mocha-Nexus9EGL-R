# Inherit device configuration for mocha.
# Boot Animtion
TARGET_BOOTANIMATION_HALF_RES := true

# Inherit some common lineage stuff.
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/languages_full.mk)
$(call inherit-product, vendor/lineage/config/common_full_tablet_wifionly.mk)

# Inherit from xiaomi device
$(call inherit-product, device/xiaomi/mocha/mocha.mk)

# Product Information
PRODUCT_NAME := lineage_mocha
PRODUCT_DEVICE := mocha
PRODUCT_BRAND := Xiaomi
PRODUCT_MANUFACTURER := Xiaomi
BOARD_VENDOR := Xiaomi
PRODUCT_MODEL := MI PAD

# GMS Client ID
PRODUCT_GMS_CLIENTID_BASE := android-xiaomi
