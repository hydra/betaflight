FEATURES       += VCP ONBOARDFLASH SDCARD_SDIO

EXST = yes

TARGET_SRC += \
            drivers/accgyro/accgyro_fake.c \
            drivers/barometer/barometer_fake.c \
            drivers/compass/compass_fake.c \
