#include "CameraPipeline.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <system/graphics.h>
#include <cutils/log.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <errno.h>
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#ifndef I2C_SLAVE_FORCE
#define I2C_SLAVE_FORCE 0x0707
#endif

/* OV5693 full register table: 1280x720 @ 60fps, 1-lane CSI-E (mocha)
 * Extracted from kernel/tegra/drivers/media/i2c/ov5693_mocha_mode_tbls.h
 * Format: {reg16, val8} */
static const struct { uint16_t reg; uint8_t val; } ov5693_regs_1280x720[] = {
    {0x0100, 0x00}, {0x0103, 0x01}, {0x3001, 0x0a}, {0x3002, 0x80},
    {0x3006, 0x00}, {0x3011, 0x21}, {0x3012, 0x09}, {0x3013, 0x10},
    {0x3014, 0x00}, {0x3015, 0x08}, {0x3016, 0xf0}, {0x3017, 0xf0},
    {0x3018, 0xf0}, {0x301b, 0xb4}, {0x301d, 0x02}, {0x3021, 0x00},
    {0x3022, 0x01}, {0x3028, 0x44}, {0x3098, 0x03}, {0x3099, 0x1e},
    {0x309a, 0x02}, {0x309b, 0x01}, {0x309c, 0x00}, {0x30a0, 0xd2},
    {0x30a2, 0x01}, {0x30b2, 0x00}, {0x30b3, 0x68}, {0x30b4, 0x03},
    {0x30b5, 0x04}, {0x30b6, 0x01}, {0x3104, 0x21}, {0x3106, 0x00},
    {0x3406, 0x01}, {0x3500, 0x00}, {0x3501, 0x2e}, {0x3502, 0x80},
    {0x3503, 0x07}, {0x3504, 0x00}, {0x3505, 0x00}, {0x3506, 0x00},
    {0x3507, 0x02}, {0x3508, 0x00}, {0x3509, 0x10}, {0x350a, 0x00},
    {0x350b, 0x40}, {0x3601, 0x0a}, {0x3602, 0x38}, {0x3612, 0x80},
    {0x3620, 0x54}, {0x3621, 0xc7}, {0x3622, 0x0f}, {0x3625, 0x10},
    {0x3630, 0x55}, {0x3631, 0xf4}, {0x3632, 0x00}, {0x3633, 0x34},
    {0x3634, 0x02}, {0x364d, 0x0d}, {0x364f, 0xdd}, {0x3660, 0x04},
    {0x3662, 0x10}, {0x3663, 0xf1}, {0x3665, 0x00}, {0x3666, 0x20},
    {0x3667, 0x00}, {0x366a, 0x80}, {0x3680, 0xe0}, {0x3681, 0x00},
    {0x3700, 0x42}, {0x3701, 0x14}, {0x3702, 0xa0}, {0x3703, 0xd8},
    {0x3704, 0x78}, {0x3705, 0x02}, {0x3708, 0xe6}, {0x3709, 0xc7},
    {0x370a, 0x00}, {0x370b, 0x20}, {0x370c, 0x0c}, {0x370d, 0x11},
    {0x370e, 0x00}, {0x370f, 0x40}, {0x3710, 0x00}, {0x371a, 0x1c},
    {0x371b, 0x05}, {0x371c, 0x01}, {0x371e, 0xa1}, {0x371f, 0x0c},
    {0x3721, 0x00}, {0x3724, 0x10}, {0x3726, 0x00}, {0x372a, 0x01},
    {0x3730, 0x10}, {0x3738, 0x22}, {0x3739, 0xe5}, {0x373a, 0x50},
    {0x373b, 0x02}, {0x373c, 0x41}, {0x373f, 0x02}, {0x3740, 0x42},
    {0x3741, 0x02}, {0x3742, 0x18}, {0x3743, 0x01}, {0x3744, 0x02},
    {0x3747, 0x10}, {0x374c, 0x04}, {0x3751, 0xf0}, {0x3752, 0x00},
    {0x3753, 0x00}, {0x3754, 0xc0}, {0x3755, 0x00}, {0x3756, 0x1a},
    {0x3758, 0x00}, {0x3759, 0x0f}, {0x376b, 0x44}, {0x375c, 0x04},
    {0x3774, 0x10}, {0x3776, 0x00}, {0x377f, 0x08}, {0x3780, 0x22},
    {0x3781, 0x0c}, {0x3784, 0x2c}, {0x3785, 0x1e}, {0x378f, 0xf5},
    {0x3791, 0xb0}, {0x3795, 0x00}, {0x3796, 0x64}, {0x3797, 0x11},
    {0x3798, 0x30}, {0x3799, 0x41}, {0x379a, 0x07}, {0x379b, 0xb0},
    {0x379c, 0x0c}, {0x37c5, 0x00}, {0x37c6, 0x00}, {0x37c7, 0x00},
    {0x37c9, 0x00}, {0x37ca, 0x00}, {0x37cb, 0x00}, {0x37de, 0x00},
    {0x37df, 0x00}, {0x3800, 0x00}, {0x3801, 0x00}, {0x3802, 0x00},
    {0x3803, 0xf4}, {0x3804, 0x0a}, {0x3805, 0x3f}, {0x3806, 0x06},
    {0x3807, 0xab}, {0x3808, 0x05}, {0x3809, 0x00}, {0x380a, 0x02},
    {0x380b, 0xd0}, {0x380c, 0x06}, {0x380d, 0xd8}, {0x380e, 0x02},
    {0x380f, 0xf8}, {0x3810, 0x00}, {0x3811, 0x02}, {0x3812, 0x00},
    {0x3813, 0x02}, {0x3814, 0x31}, {0x3815, 0x31}, {0x3820, 0x04},
    {0x3821, 0x1f}, {0x3823, 0x00}, {0x3824, 0x00}, {0x3825, 0x00},
    {0x3826, 0x00}, {0x3827, 0x00}, {0x382a, 0x04}, {0x3a04, 0x06},
    {0x3a05, 0x14}, {0x3a06, 0x00}, {0x3a07, 0xfe}, {0x3b00, 0x00},
    {0x3b02, 0x00}, {0x3b03, 0x00}, {0x3b04, 0x00}, {0x3b05, 0x00},
    {0x3e07, 0x20}, {0x4000, 0x08}, {0x4001, 0x04}, {0x4002, 0x45},
    {0x4004, 0x08}, {0x4005, 0x18}, {0x4006, 0x20}, {0x4008, 0x24},
    {0x4009, 0x10}, {0x400c, 0x00}, {0x400d, 0x00}, {0x4058, 0x00},
    {0x404e, 0x37}, {0x404f, 0x8f}, {0x4058, 0x00}, {0x4101, 0xb2},
    {0x4303, 0x00}, {0x4304, 0x08}, {0x4307, 0x30}, {0x4311, 0x04},
    {0x4315, 0x01}, {0x4511, 0x05}, {0x4512, 0x00}, {0x4800, 0x20},
    {0x4806, 0x00}, {0x4816, 0x52}, {0x481f, 0x30}, {0x4826, 0x2c},
    {0x4831, 0x64}, {0x4d00, 0x04}, {0x4d01, 0x71}, {0x4d02, 0xfd},
    {0x4d03, 0xf5}, {0x4d04, 0x0c}, {0x4d05, 0xcc}, {0x4837, 0x0a},
    {0x5000, 0x06}, {0x5001, 0x01}, {0x5002, 0x00}, {0x5003, 0x20},
    {0x5046, 0x0a}, {0x5013, 0x00}, {0x5046, 0x0a}, {0x5780, 0x1c},
    {0x5786, 0x20}, {0x5787, 0x10}, {0x5788, 0x18}, {0x578a, 0x04},
    {0x578b, 0x02}, {0x578c, 0x02}, {0x578e, 0x06}, {0x578f, 0x02},
    {0x5790, 0x02}, {0x5791, 0xff}, {0x5842, 0x01}, {0x5843, 0x2b},
    {0x5844, 0x01}, {0x5845, 0x92}, {0x5846, 0x01}, {0x5847, 0x8f},
    {0x5848, 0x01}, {0x5849, 0x0c}, {0x5e00, 0x00}, {0x5e10, 0x0c},
    /* Mocha 1-lane CSI-E overrides */
    {0x3011, 0x11}, {0x3015, 0x28}, {0x380c, 0x0a}, {0x380d, 0x80},
    {0x380e, 0x03}, {0x380f, 0xe0},
    /* Start streaming */
    {0x0100, 0x01},
};

#ifndef LOG_TAG
#define LOG_TAG "MochaCameraHAL"
#endif

namespace mocha {

CameraPipeline::CameraPipeline()
    : mFd(-1),
      mSensorFd(-1),
      mState(PIPELINE_CLOSED),
      mRgbBuffer(nullptr),
      mRgbBufferSize(0),
      mStreaming(false),
      mBufferCount(0),
      mCurrentBuffer(0),
    mCurrentExposure(800),
    mCurrentGain(60),
      mHasAwbInit(false),
      mLastGamma(0.0f),
      mFocusPosition(0),
      mAfState(0) {
    for (int i = 0; i < 4; i++) {
        mBuffers[i].start = nullptr;
        mBuffers[i].length = 0;
        mBuffers[i].allocated = false;
    }
    mAwbGains[0] = 1.0f;
    mAwbGains[1] = 1.0f;
    mAwbGains[2] = 1.0f;
    mAwbGains[3] = 1.0f;
}

CameraPipeline::~CameraPipeline() {
    close();
}

int CameraPipeline::open(int cameraId) {
    ALOGI("CameraPipeline::open cameraId=%d", cameraId);

    if (mState != PIPELINE_CLOSED) {
        ALOGE("Pipeline already open");
        return -EBUSY;
    }

    mCameraId = cameraId;
    const char* devPath = (cameraId == 0) ? "/dev/video0" : "/dev/video1";

    mFd = ::open(devPath, O_RDWR | O_NONBLOCK);
    if (mFd < 0) {
        ALOGE("Failed to open V4L2 device: %s (error %d: %s)", devPath, errno, strerror(errno));
        return -ENODEV;
    }

    struct v4l2_capability cap;
    memset(&cap, 0, sizeof(cap));
    if (ioctl(mFd, VIDIOC_QUERYCAP, &cap) < 0) {
        ALOGE("VIDIOC_QUERYCAP failed: %s", strerror(errno));
        ::close(mFd);
        mFd = -1;
        return -ENODEV;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ||
        !(cap.capabilities & V4L2_CAP_STREAMING)) {
        ALOGE("Device does not support video capture or streaming");
        ::close(mFd);
        mFd = -1;
        return -ENODEV;
    }

    if (cameraId == 0) {
        initFocuser();
    } else {
        /* Open I2C bus for OV5693 register access (front camera).
           Use I2C_SLAVE_FORCE since kernel driver already has 0x36 bound. */
        mSensorFd = ::open("/dev/i2c-2", O_RDWR);
        if (mSensorFd < 0) {
            ALOGE("Failed to open /dev/i2c-2: %s", strerror(errno));
        } else {
            if (ioctl(mSensorFd, I2C_SLAVE_FORCE, 0x36) < 0) {
                ALOGE("I2C_SLAVE_FORCE 0x36 failed: %s", strerror(errno));
                ::close(mSensorFd);
                mSensorFd = -1;
            } else {
                ALOGI("I2C bus opened for OV5693 (addr 0x36, FORCE)");
            }
        }
    }

    mState = PIPELINE_OPENED;
    return 0;
}

int CameraPipeline::close() {
    stopStreaming();

    for (int i = 0; i < mBufferCount; i++) {
        if (mBuffers[i].start && mBuffers[i].allocated) {
            munmap(mBuffers[i].start, mBuffers[i].length);
            mBuffers[i].start = nullptr;
            mBuffers[i].allocated = false;
        }
    }
    mBufferCount = 0;

    if (mRgbBuffer) {
        delete[] mRgbBuffer;
        mRgbBuffer = nullptr;
        mRgbBufferSize = 0;
    }

    if (mFd >= 0) {
        ::close(mFd);
        mFd = -1;
    }

    if (mSensorFd >= 0) {
        ::close(mSensorFd);
        mSensorFd = -1;
    }

    deinitFocuser();

    mState = PIPELINE_CLOSED;
    return 0;
}

int CameraPipeline::setExposure(int exposure) {
    struct v4l2_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_EXPOSURE;
    ctrl.value = exposure;
    int ret = ioctl(mFd, VIDIOC_S_CTRL, &ctrl);
    if (ret == 0) {
        mCurrentExposure = exposure;
    } else {
        ALOGE("setExposure(%d) failed: ret=%d errno=%d (%s)",
              exposure, ret, errno, strerror(errno));
    }
    return ret;
}

int CameraPipeline::setGain(int gain) {
    struct v4l2_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_GAIN;
    ctrl.value = gain;
    int ret = ioctl(mFd, VIDIOC_S_CTRL, &ctrl);
    if (ret == 0) {
        mCurrentGain = gain;
    } else {
        ALOGE("setGain(%d) failed: ret=%d errno=%d (%s)",
              gain, ret, errno, strerror(errno));
    }
    return ret;
}

int CameraPipeline::getExposure() {
    struct v4l2_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_EXPOSURE;
    if (ioctl(mFd, VIDIOC_G_CTRL, &ctrl) == 0) {
        mCurrentExposure = ctrl.value;
        return ctrl.value;
    }
    return mCurrentExposure;
}

int CameraPipeline::getGain() {
    struct v4l2_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_GAIN;
    if (ioctl(mFd, VIDIOC_G_CTRL, &ctrl) == 0) {
        mCurrentGain = ctrl.value;
        return ctrl.value;
    }
    return mCurrentGain;
}

int CameraPipeline::configure(const PipelineConfig& config) {
    if (mState == PIPELINE_STREAMING) {
        return -EBUSY;
    }

    mConfig = config;

    if (mFd < 0) {
        return -ENODEV;
    }

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = config.width;
    fmt.fmt.pix.height = config.height;
    fmt.fmt.pix.pixelformat = config.pixelFormat;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    ALOGI("VIDIOC_S_FMT: requesting %dx%d fmt=0x%x", config.width, config.height, config.pixelFormat);

    if (ioctl(mFd, VIDIOC_S_FMT, &fmt) < 0) {
        ALOGE("VIDIOC_S_FMT failed: %s", strerror(errno));
        return -errno;
    }

    ALOGI("VIDIOC_S_FMT: got %dx%d fmt=0x%x sizeimage=%d bytesperline=%d",
          fmt.fmt.pix.width, fmt.fmt.pix.height,
          fmt.fmt.pix.pixelformat, fmt.fmt.pix.sizeimage,
          fmt.fmt.pix.bytesperline);


    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(mFd, VIDIOC_REQBUFS, &req) < 0) {
        ALOGE("VIDIOC_REQBUFS failed: %s", strerror(errno));
        return -errno;
    }

    if (req.count < 2) {
        ALOGE("VIDIOC_REQBUFS: only %d buffers available (need >=2)", req.count);
        return -ENOMEM;
    }
    ALOGI("VIDIOC_REQBUFS: got %d buffers", req.count);

    mBufferCount = req.count;

    uint32_t bufSize = fmt.fmt.pix.sizeimage;
    for (int i = 0; i < mBufferCount; i++) {
        struct v4l2_buffer qbuf;
        memset(&qbuf, 0, sizeof(qbuf));
        qbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        qbuf.memory = V4L2_MEMORY_MMAP;
        qbuf.index = i;
        if (ioctl(mFd, VIDIOC_QUERYBUF, &qbuf) < 0) {
            for (int j = 0; j < i; j++) {
                if (mBuffers[j].start) munmap(mBuffers[j].start, mBuffers[j].length);
                mBuffers[j].start = nullptr;
                mBuffers[j].allocated = false;
            }
            mBufferCount = 0;
            return -errno;
        }
        mBuffers[i].length = qbuf.length;
        mBuffers[i].start = mmap(NULL, qbuf.length, PROT_READ | PROT_WRITE, MAP_SHARED, mFd, qbuf.m.offset);
        if (mBuffers[i].start == MAP_FAILED) {
            mBuffers[i].start = nullptr;
            for (int j = 0; j < i; j++) {
                if (mBuffers[j].start) munmap(mBuffers[j].start, mBuffers[j].length);
                mBuffers[j].start = nullptr;
                mBuffers[j].allocated = false;
            }
            mBufferCount = 0;
            return -ENOMEM;
        }
        mBuffers[i].allocated = true;
    }

    if (config.enableISP) {
        DemosaicParams demosaicParams;
        demosaicParams.width = config.width;
        demosaicParams.height = config.height;
        demosaicParams.bayerPattern = config.bayerPattern;
        demosaicParams.offset_x = config.offset_x;
        demosaicParams.offset_y = config.offset_y;
        demosaicParams.blackLevel = config.blackLevel;

        mDemosaic = std::unique_ptr<DemosaicNEON>(new DemosaicNEON());
        int ret = mDemosaic->initialize(demosaicParams);
        if (ret != 0) return ret;

        mColorConv = std::unique_ptr<ColorConvNEON>(new ColorConvNEON());
        ret = mColorConv->initialize(config.width, config.height);
        if (ret != 0) return ret;

        if (mRgbBuffer) {
            delete[] mRgbBuffer;
            mRgbBuffer = nullptr;
        }
        mRgbBufferSize = config.width * config.height * 3;
        mRgbBuffer = new uint8_t[mRgbBufferSize];
    }

    mHasAwbInit = false;

    /* Rebuild gamma LUT if gamma changed */
    if (mLastGamma != config.gamma) {
        for (int i = 0; i < 256; i++)
            mGammaLut[i] = (uint8_t)(powf(i / 255.0f, config.gamma) * 255.0f + 0.5f);
        mLastGamma = config.gamma;
    }

    return 0;
}

/* Write full OV5693 register table via I2C to properly configure sensor.
 * Called AFTER VIDIOC_STREAMON (kernel s_stream already ran with 25 regs).
 * This overwrites with the complete 250+ register configuration. */
static void ov5693_init_via_i2c(int i2c_fd) {
    if (i2c_fd < 0) return;

    int count = sizeof(ov5693_regs_1280x720) / sizeof(ov5693_regs_1280x720[0]);
    int errors = 0;

    for (int i = 0; i < count; i++) {
        uint8_t buf[3];
        buf[0] = (ov5693_regs_1280x720[i].reg >> 8) & 0xff;
        buf[1] = ov5693_regs_1280x720[i].reg & 0xff;
        buf[2] = ov5693_regs_1280x720[i].val;

        if (write(i2c_fd, buf, 3) != 3) {
            if (errors < 5)
                ALOGE("OV5693 I2C write reg=0x%04x val=0x%02x failed: %s",
                      ov5693_regs_1280x720[i].reg, ov5693_regs_1280x720[i].val,
                      strerror(errno));
            errors++;
        }
    }

    if (errors == 0)
        ALOGI("OV5693: wrote %d registers via I2C successfully", count);
    else
        ALOGE("OV5693: %d/%d I2C writes failed", errors, count);
}

int CameraPipeline::startStreaming() {
    if (mState != PIPELINE_OPENED) return -EINVAL;

    /* Re-request buffers (idempotent: reuses existing if count matches) */
    for (int i = 0; i < mBufferCount; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(mFd, VIDIOC_QBUF, &buf) < 0) return -errno;
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(mFd, VIDIOC_STREAMON, &type) < 0) return -errno;

    if (mCameraId == 0) {
        setExposure(mCurrentExposure);
        setGain(mCurrentGain);
        ALOGI("Initial exposure=%d gain=%d", mCurrentExposure, mCurrentGain);
    } else {
        /* Front camera: write full OV5693 register table via I2C.
           Wait for kernel to power on sensor (s_power in capture thread). */
        usleep(300000); /* 300ms - wait for sensor power-on */
        ov5693_init_via_i2c(mSensorFd);
    }

    mStreaming = true;
    mState = PIPELINE_STREAMING;
    return 0;
}

int CameraPipeline::stopStreaming() {
    if (!mStreaming) return 0;

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(mFd, VIDIOC_STREAMOFF, &type);

    for (int i = 0; i < mBufferCount; i++) {
        if (mBuffers[i].start) {
            munmap(mBuffers[i].start, mBuffers[i].length);
            mBuffers[i].start = nullptr;
            mBuffers[i].allocated = false;
        }
    }
    mBufferCount = 0;

    mStreaming = false;
    mState = PIPELINE_OPENED;
    return 0;
}

void CameraPipeline::doAutoExposure(const uint8_t* rgbBuffer) {
    if (!rgbBuffer || !mConfig.enableAE) return;

    int w = mConfig.width;
    int h = mConfig.height;
    int step = 8;
    uint64_t sum = 0;
    int count = 0;
    for (int y = 0; y < h; y += step) {
        for (int x = 0; x < w; x += step) {
            int off = (y * w + x) * 3;
            uint8_t r = rgbBuffer[off], g = rgbBuffer[off+1], b = rgbBuffer[off+2];
            sum += (r * 77 + g * 150 + b * 29) >> 8;
            count++;
        }
    }
    if (count == 0) return;
    float avgLuma = (float)sum / count / 255.0f;
    float target = mConfig.targetLuma;

    float ratio = target / (avgLuma > 0.001f ? avgLuma : 0.001f);
    ratio = (ratio < 0.25f) ? 0.25f : (ratio > 8.0f) ? 8.0f : ratio;

    int newExp = (int)(mCurrentExposure * ratio);
    int newGain = mCurrentGain;

    /* Prefer longer exposure over higher gain to reduce noise */
    if (newExp > 2000) {
        newGain = (int)(mCurrentGain * (newExp / 2000.0f));
        newExp = 2000;
    } else if (newGain > 100 && newExp < 2000) {
        /* If gain is high, increase exposure instead */
        newExp = (int)(newExp * (newGain / 100.0f));
        newGain = 100;
        if (newExp > 2000) newExp = 2000;
    }
    if (newExp < 10) {
        newExp = 10;
    }
    if (newGain > 120) newGain = 120;
    if (newGain < 1) newGain = 1;

    if (newExp != mCurrentExposure || newGain != mCurrentGain) {
        if (newGain != mCurrentGain) setGain(newGain);
        if (newExp != mCurrentExposure) setExposure(newExp);
        ALOGI("AE: luma=%.2f target=%.2f exp=%d(%d) gain=%d(%d)",
              avgLuma, target, newExp, mCurrentExposure, newGain, mCurrentGain);
    }
}

void CameraPipeline::doAutoWhiteBalance(const uint8_t* rgbBuffer) {
    if (!rgbBuffer || !mConfig.enableAWB) return;

    int w = mConfig.width;
    int h = mConfig.height;
    int step = 16;
    uint64_t sumR = 0, sumG = 0, sumB = 0;
    int pixelCount = 0;

    for (int y = 0; y < h; y += step) {
        for (int x = 0; x < w; x += step) {
            int off = (y * w + x) * 3;
            sumR += rgbBuffer[off];
            sumG += rgbBuffer[off+1];
            sumB += rgbBuffer[off+2];
            pixelCount++;
        }
    }

    if (pixelCount < 100) return;
    float avgR = (float)sumR / pixelCount;
    float avgG = (float)sumG / pixelCount;
    float avgB = (float)sumB / pixelCount;

    if (avgR < 5.0f || avgG < 5.0f || avgB < 5.0f) return;

    float rGain = avgG / avgR;
    float bGain = avgG / avgB;

    rGain = (rGain < 0.5f) ? 0.5f : (rGain > 3.0f) ? 3.0f : rGain;
    bGain = (bGain < 0.5f) ? 0.5f : (bGain > 3.0f) ? 3.0f : bGain;

    float alpha = 0.3f;
    if (!mHasAwbInit) {
        mAwbGains[0] = rGain;
        mAwbGains[2] = bGain;
        mHasAwbInit = true;
    } else {
        mAwbGains[0] = mAwbGains[0] * (1.0f - alpha) + rGain * alpha;
        mAwbGains[2] = mAwbGains[2] * (1.0f - alpha) + bGain * alpha;
    }
    mAwbGains[1] = 1.0f;
    mAwbGains[3] = 1.0f;

    ALOGI("AWB: R/G=%.2f B/G=%.2f gains R=%.2f B=%.2f", avgR/avgG, avgB/avgG, mAwbGains[0], mAwbGains[2]);
}

static void applyGamma(uint8_t* rgb, int width, int height, float gamma) {
    uint8_t lut[256];
    for (int i = 0; i < 256; i++)
        lut[i] = (uint8_t)(powf(i / 255.0f, gamma) * 255.0f + 0.5f);
    int total = width * height;
    for (int i = 0; i < total * 3; i++)
        rgb[i] = lut[rgb[i]];
}

static void flipVertical(uint8_t* buf, int width, int height, int bpp) {
    int rowSize = width * bpp;
    uint8_t* tmp = new uint8_t[rowSize];
    for (int y = 0; y < height / 2; y++) {
        int topOff = y * rowSize;
        int botOff = (height - 1 - y) * rowSize;
        memcpy(tmp, buf + topOff, rowSize);
        memcpy(buf + topOff, buf + botOff, rowSize);
        memcpy(buf + botOff, tmp, rowSize);
    }
    delete[] tmp;
}

int CameraPipeline::captureFrame(uint8_t* outputBuffer, uint32_t outputFormat) {
    if (mState != PIPELINE_STREAMING) {
        ALOGE("captureFrame: state=%d (need STREAMING=%d)", mState, PIPELINE_STREAMING);
        return -EINVAL;
    }
    if (!outputBuffer) {
        ALOGE("captureFrame: null outputBuffer");
        return -EINVAL;
    }

    struct pollfd pfd;
    pfd.fd = mFd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    int poll_timeout_ms = 300;
    int max_retries = 6;
    int retry_count = 0;
    int pollRet = 0;

    do {
        pfd.revents = 0;
        pollRet = poll(&pfd, 1, poll_timeout_ms);
        if (pollRet < 0) {
            ALOGE("captureFrame: poll error: %s", strerror(errno));
            return -errno;
        }
        if (pollRet == 0) {
            retry_count++;
            if (retry_count >= max_retries) {
                ALOGE("captureFrame: poll timeout after %d retries", max_retries);
                return -EAGAIN;
            }
            continue;
        }
        if (!(pfd.revents & POLLIN)) {
            ALOGE("captureFrame: poll returned %d, revents=0x%x (no POLLIN)", pollRet, pfd.revents);
            return -EAGAIN;
        }
        break;
    } while (retry_count < max_retries);

    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    int ret = ioctl(mFd, VIDIOC_DQBUF, &buf);
    if (ret < 0) {
        if (errno == EAGAIN) {
            ALOGE("captureFrame: DQBUF EAGAIN (no buffer ready)");
            return -EAGAIN;
        }
        ALOGE("captureFrame: DQBUF error: %s", strerror(errno));
        return -errno;
    }

    if (buf.index >= mBufferCount) {
        ALOGE("captureFrame: buf.index=%u >= mBufferCount=%d", buf.index, mBufferCount);
        return -EINVAL;
    }

    uint8_t* frameBuffer = (uint8_t*)mBuffers[buf.index].start;
    uint32_t frameSize = buf.bytesused;

    /* DEBUG: log raw Bayer stats every 100 frames */
    static int rawDebugCount = 0;
    if (rawDebugCount++ % 100 == 0) {
        int minV = 255, maxV = 0;
        int sampleCount = frameSize < 4000 ? (int)frameSize : 4000;
        for (int i = 0; i < sampleCount; i++) {
            if (frameBuffer[i] < minV) minV = frameBuffer[i];
            if (frameBuffer[i] > maxV) maxV = frameBuffer[i];
        }
        /* Log first 32 bytes (16 pixels) as hex + 10-bit extracted values */
        ALOGI("RAW Bayer debug: min=%d max=%d size=%u",
              minV, maxV, frameSize);
        ALOGI("  raw32: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
              frameBuffer[0], frameBuffer[1], frameBuffer[2], frameBuffer[3],
              frameBuffer[4], frameBuffer[5], frameBuffer[6], frameBuffer[7],
              frameBuffer[8], frameBuffer[9], frameBuffer[10], frameBuffer[11],
              frameBuffer[12], frameBuffer[13], frameBuffer[14], frameBuffer[15]);
        ALOGI("  raw32b: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
              frameBuffer[16], frameBuffer[17], frameBuffer[18], frameBuffer[19],
              frameBuffer[20], frameBuffer[21], frameBuffer[22], frameBuffer[23],
              frameBuffer[24], frameBuffer[25], frameBuffer[26], frameBuffer[27],
              frameBuffer[28], frameBuffer[29], frameBuffer[30], frameBuffer[31]);
        /* Log 4x4 grid of 10-bit values (v>>8) to see spatial pattern */
        int w = 1280;
        uint16_t g[4][4];
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++)
                memcpy(&g[r][c], frameBuffer + (r * w + c) * 2, 2);
        ALOGI("  grid r0: %d %d %d %d", g[0][0]>>8, g[0][1]>>8, g[0][2]>>8, g[0][3]>>8);
        ALOGI("  grid r1: %d %d %d %d", g[1][0]>>8, g[1][1]>>8, g[1][2]>>8, g[1][3]>>8);
        ALOGI("  grid r2: %d %d %d %d", g[2][0]>>8, g[2][1]>>8, g[2][2]>>8, g[2][3]>>8);
        ALOGI("  grid r3: %d %d %d %d", g[3][0]>>8, g[3][1]>>8, g[3][2]>>8, g[3][3]>>8);
        /* Middle of image */
        int midY = 360, midX = 640;
        uint16_t mg[4][4];
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++)
                memcpy(&mg[r][c], frameBuffer + ((midY+r) * w + (midX+c)) * 2, 2);
        ALOGI("  mid r0: %d %d %d %d", mg[0][0]>>8, mg[0][1]>>8, mg[0][2]>>8, mg[0][3]>>8);
        ALOGI("  mid r1: %d %d %d %d", mg[1][0]>>8, mg[1][1]>>8, mg[1][2]>>8, mg[1][3]>>8);
        ALOGI("  mid r2: %d %d %d %d", mg[2][0]>>8, mg[2][1]>>8, mg[2][2]>>8, mg[2][3]>>8);
        ALOGI("  mid r3: %d %d %d %d", mg[3][0]>>8, mg[3][1]>>8, mg[3][2]>>8, mg[3][3]>>8);
    }

    if (mConfig.enableISP && mDemosaic && mColorConv) {
        ret = processBayerToYuv(frameBuffer, outputBuffer, outputFormat);
    } else {
        memcpy(outputBuffer, frameBuffer, frameSize);
        ret = 0;
    }

    if (ioctl(mFd, VIDIOC_QBUF, &buf) < 0) return -errno;

    return ret;
}

int CameraPipeline::processBayerToYuv(const uint8_t* bayerData, uint8_t* output, uint32_t outputFormat) {
    if (!bayerData || !output || !mDemosaic || !mColorConv) return -EINVAL;

    mDemosaic->process(bayerData, mRgbBuffer);

    /* AE update before applying gains */
    if (mConfig.enableAE)
        doAutoExposure(mRgbBuffer);

    /* AWB update before applying gains */
    if (mConfig.enableAWB)
        doAutoWhiteBalance(mRgbBuffer);

    float rG = (mConfig.enableAWB ? mAwbGains[0] : mConfig.wbGain[0]) * mConfig.digitalGain;
    float gG = (mConfig.enableAWB ? mAwbGains[1] : mConfig.wbGain[1]) * mConfig.digitalGain;
    float bG = (mConfig.enableAWB ? mAwbGains[2] : mConfig.wbGain[2]) * mConfig.digitalGain;

    if (outputFormat == HAL_PIXEL_FORMAT_YCBCR_420_888) {
        /* For YUV: apply WB gains in-place first, then gamma, then convert */
        {
            int total = mConfig.width * mConfig.height;
            if (rG != 1.0f || gG != 1.0f || bG != 1.0f) {
                for (int i = 0; i < total; i++) {
                    int off = i * 3;
                    int r = (int)(mRgbBuffer[off]   * rG);
                    int g = (int)(mRgbBuffer[off+1] * gG);
                    int b = (int)(mRgbBuffer[off+2] * bG);
                    mRgbBuffer[off]   = r > 255 ? 255 : (uint8_t)r;
                    mRgbBuffer[off+1] = g > 255 ? 255 : (uint8_t)g;
                    mRgbBuffer[off+2] = b > 255 ? 255 : (uint8_t)b;
                }
            }
        }
        applyGamma(mRgbBuffer, mConfig.width, mConfig.height, mConfig.gamma);
        uint8_t* yPlane = output;
        uint8_t* uvPlane = output + mConfig.width * mConfig.height;
        mColorConv->rgbToNv12(mRgbBuffer, yPlane, uvPlane);
    } else if (outputFormat == HAL_PIXEL_FORMAT_RGBA_8888 || outputFormat == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED) {
        /* Merged WB + gamma + RGB→RGBA + flip in one pass */
        mColorConv->rgbToRgbaWbGamma(mRgbBuffer, output,
                                     mConfig.width, mConfig.height,
                                     rG, gG, bG, mGammaLut, mConfig.flipV);
    } else {
        /* For other formats: apply WB and gamma, then raw copy */
        {
            int total = mConfig.width * mConfig.height;
            if (rG != 1.0f || gG != 1.0f || bG != 1.0f) {
                for (int i = 0; i < total; i++) {
                    int off = i * 3;
                    int r = (int)(mRgbBuffer[off]   * rG);
                    int g = (int)(mRgbBuffer[off+1] * gG);
                    int b = (int)(mRgbBuffer[off+2] * bG);
                    mRgbBuffer[off]   = r > 255 ? 255 : (uint8_t)r;
                    mRgbBuffer[off+1] = g > 255 ? 255 : (uint8_t)g;
                    mRgbBuffer[off+2] = b > 255 ? 255 : (uint8_t)b;
                }
            }
        }
        applyGamma(mRgbBuffer, mConfig.width, mConfig.height, mConfig.gamma);
        memcpy(output, mRgbBuffer, mRgbBufferSize);
    }

    return 0;
}

int CameraPipeline::initFocuser() {
    mFocusPosition = -1;
    ALOGI("Focuser: kernel controls AD5823 power/I2C; position 400 set at stream start");
    return 0;
}

void CameraPipeline::deinitFocuser() {
}

int CameraPipeline::setFocus(int position) {
    if (position < 0) position = 0;
    if (position > 1023) position = 1023;

    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_FOCUS_ABSOLUTE;
    ctrl.value = position;

    int ret = ioctl(mFd, VIDIOC_S_CTRL, &ctrl);
    if (ret == 0) {
        mFocusPosition = position;
        ALOGI("Focuser: set position %d via V4L2", position);
    } else {
        ALOGW("Focuser: ioctl V4L2_CID_FOCUS_ABSOLUTE=%d failed: %s",
              position, strerror(errno));
    }
    return ret;
}

/*
 * Horizontal Sobel energy on the green channel of an RGB buffer.
 * Uses 3×3 Sobel kernel for horizontal edges:
 *   Gx = | -1 0 +1 |
 *        | -2 0 +2 |
 *        | -1 0 +1 |
 * Only green channel (byte-offset 1) is used — fast approximation.
 */
int CameraPipeline::sobelEnergy(const uint8_t* rgb, int w, int h) const {
    if (!rgb || w < 3 || h < 3) return 0;
    int total = 0;
    int stride = w * 3;
    for (int y = 1; y < h - 1; y++) {
        const uint8_t* row = rgb + y * stride;
        for (int x = 1; x < w - 1; x++) {
            int g = abs((int)row[(x+1)*3+1] - (int)row[(x-1)*3+1]); // horizontal gradient
            total += g;
        }
    }
    return total;
}

/*
 * 8-pixel-wide horizontal sobel using green channel.
 * Processes 8 adjacent output pixels at a time, summing partial gradients.
 * Same algorithm as sobelEnergy() but loop-unrolled for 8x regions.
 */
int CameraPipeline::sobelEnergyNw(const uint8_t* rgb, int w, int h) const {
    if (!rgb || w < 3 || h < 3) return 0;
    int total = 0;
    int stride = w * 3;
    int x = 1;
    /* Process 8-wide chunks */
    for (int y = 1; y < h - 1; y++) {
        const uint8_t* row = rgb + y * stride;
        x = 1;
        while (x + 8 < w - 1) {
            int g0 = abs((int)row[(x+1)*3+1] - (int)row[(x-1)*3+1]);
            int g1 = abs((int)row[(x+2)*3+1] - (int)row[(x+0)*3+1]);
            int g2 = abs((int)row[(x+3)*3+1] - (int)row[(x+1)*3+1]);
            int g3 = abs((int)row[(x+4)*3+1] - (int)row[(x+2)*3+1]);
            int g4 = abs((int)row[(x+5)*3+1] - (int)row[(x+3)*3+1]);
            int g5 = abs((int)row[(x+6)*3+1] - (int)row[(x+4)*3+1]);
            int g6 = abs((int)row[(x+7)*3+1] - (int)row[(x+5)*3+1]);
            int g7 = abs((int)row[(x+8)*3+1] - (int)row[(x+6)*3+1]);
            total += g0 + g1 + g2 + g3 + g4 + g5 + g6 + g7;
            x += 8;
        }
        /* Remainder */
        for (; x < w - 1; x++) {
            total += abs((int)row[(x+1)*3+1] - (int)row[(x-1)*3+1]);
        }
    }
    return total;
}

/*
 * Capture one V4L2 frame and return Sobel energy from the green channel.
 * Used during AF sweep — captures a frame, demosaics to RGB, measures contrast.
 */
int CameraPipeline::captureForAf() {
    if (mState != PIPELINE_STREAMING) return -EINVAL;

    struct pollfd pfd;
    pfd.fd = mFd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    int pollRet = poll(&pfd, 1, 500);
    if (pollRet <= 0) return -EAGAIN;
    if (!(pfd.revents & POLLIN)) return -EAGAIN;

    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_USERPTR;

    int ret = ioctl(mFd, VIDIOC_DQBUF, &buf);
    if (ret < 0) return -errno;
    if (buf.index >= mBufferCount) return -EINVAL;

    uint8_t* frameBuffer = (uint8_t*)mBuffers[buf.index].start;
    int energy = 0;

    if (mConfig.enableISP && mDemosaic && mColorConv) {
        /* Demosaic to mRgbBuffer */
        mDemosaic->process(frameBuffer, mRgbBuffer);
        /* Apply AE — doAutoExposure reads green from mRgbBuffer */
        if (mConfig.enableAE)
            doAutoExposure(mRgbBuffer);
        /* Apply AWB gains in-place */
        if (mConfig.enableAWB) {
            doAutoWhiteBalance(mRgbBuffer);
        }
        float rG = mConfig.enableAWB ? mAwbGains[0] : mConfig.wbGain[0];
        float gG = mConfig.enableAWB ? mAwbGains[1] : mConfig.wbGain[1];
        float bG = mConfig.enableAWB ? mAwbGains[2] : mConfig.wbGain[2];
        int total = mConfig.width * mConfig.height;
        for (int i = 0; i < total; i++) {
            int off = i * 3;
            int r = (int)(mRgbBuffer[off]   * rG);
            int g = (int)(mRgbBuffer[off+1] * gG);
            int b = (int)(mRgbBuffer[off+2] * bG);
            mRgbBuffer[off]   = r > 255 ? 255 : (uint8_t)r;
            mRgbBuffer[off+1] = g > 255 ? 255 : (uint8_t)g;
            mRgbBuffer[off+2] = b > 255 ? 255 : (uint8_t)b;
        }
        applyGamma(mRgbBuffer, mConfig.width, mConfig.height, mConfig.gamma);

        /* Measure Sobel energy on the green channel */
        energy = sobelEnergyNw(mRgbBuffer, mConfig.width, mConfig.height);
    }

    /* Return buffer to queue */
    buf.m.userptr = (unsigned long)mBuffers[buf.index].start;
    buf.length = mBuffers[buf.index].length;
    if (ioctl(mFd, VIDIOC_QBUF, &buf) < 0) return -errno;

    return energy;
}

void CameraPipeline::startAfScan() {
    if (mState != PIPELINE_STREAMING) {
        ALOGW("AF: cannot scan, not streaming");
        mAfState = 0;
        return;
    }

    mAfState = 1; // MOVING
    ALOGI("AF: starting contrast-detection scan");

    int bestEnergy = 0;
    int bestPos = 400;
    const int coarseStep = 50;
    const int fineStep = 10;
    const int settleMs = 60;

    /* ---- Coarse sweep: 140 → 640 ---- */
    for (int pos = 140; pos <= 640; pos += coarseStep) {
        setFocus(pos);
        usleep(settleMs * 1000);
        int energy = captureForAf();
        if (energy < 0) {
            ALOGW("AF: capture error at pos %d: %d", pos, energy);
            continue;
        }
        ALOGI("AF: coarse pos=%d energy=%d", pos, energy);
        if (energy > bestEnergy) {
            bestEnergy = energy;
            bestPos = pos;
        }
    }

    /* ---- Fine sweep around best ---- */
    int fineStart = bestPos - coarseStep;
    if (fineStart < 140) fineStart = 140;
    int fineEnd = bestPos + coarseStep;
    if (fineEnd > 640) fineEnd = 640;

    for (int pos = fineStart; pos <= fineEnd; pos += fineStep) {
        if (pos == bestPos) continue; // already measured
        setFocus(pos);
        usleep(settleMs * 1000);
        int energy = captureForAf();
        if (energy < 0) continue;
        ALOGI("AF: fine pos=%d energy=%d", pos, energy);
        if (energy > bestEnergy) {
            bestEnergy = energy;
            bestPos = pos;
        }
    }

    /* ---- Lock to best position ---- */
    setFocus(bestPos);
    usleep(settleMs * 1000);
    mAfState = 4; // FOCUSED_LOCKED
    ALOGI("AF: scan complete, lock at position=%d energy=%d", bestPos, bestEnergy);
}

void CameraPipeline::cancelAf() {
    setFocus(0);
    mAfState = 0; // INACTIVE
    ALOGI("AF: cancelled, focus at infinity");
}

} // namespace mocha
