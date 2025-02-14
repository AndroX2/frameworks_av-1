/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FRAME_DECODER_H_
#define FRAME_DECODER_H_

#include <memory>
#include <vector>

#include <media/stagefright/foundation/AString.h>
#include <media/stagefright/foundation/ABase.h>
#include <media/stagefright/MediaSource.h>
#include <media/openmax/OMX_Video.h>
#include <ui/GraphicTypes.h>
#include <utils/threads.h>

namespace android {

struct AMessage;
struct MediaCodec;
class IMediaSource;
class MediaCodecBuffer;
class Surface;
class VideoFrame;

struct FrameRect {
    int32_t left, top, right, bottom;
};

struct FrameDecoder : public RefBase {
    FrameDecoder(
            const AString &componentName,
            const sp<MetaData> &trackMeta,
            const sp<IMediaSource> &source);

    status_t init(int64_t frameTimeUs, int option, int colorFormat);

    sp<IMemory> extractFrame(FrameRect *rect = NULL);

    static sp<IMemory> getMetadataOnly(
            const sp<MetaData> &trackMeta, int colorFormat, bool thumbnail = false);

protected:
    virtual ~FrameDecoder();

    virtual sp<AMessage> onGetFormatAndSeekOptions(
            int64_t frameTimeUs,
            int seekMode,
            MediaSource::ReadOptions *options,
            sp<Surface> *window) = 0;

    virtual status_t onExtractRect(FrameRect *rect) = 0;

    virtual status_t onInputReceived(
            const sp<MediaCodecBuffer> &codecBuffer,
            MetaDataBase &sampleMeta,
            bool firstSample,
            uint32_t *flags) = 0;

    virtual status_t onOutputReceived(
            const sp<MediaCodecBuffer> &videoFrameBuffer,
            const sp<AMessage> &outputFormat,
            int64_t timeUs,
            bool *done) = 0;

    virtual bool shouldDropOutput(int64_t ptsUs __unused) {
        return false;
    }

    virtual status_t extractInternal();

    sp<MetaData> trackMeta()     const      { return mTrackMeta; }
    OMX_COLOR_FORMATTYPE dstFormat() const  { return mDstFormat; }
    ui::PixelFormat captureFormat() const   { return mCaptureFormat; }
    int32_t dstBpp()             const      { return mDstBpp; }
    void setFrame(const sp<IMemory> &frameMem) { mFrameMemory = frameMem; }
    bool mIDRSent;

    bool mHaveMoreInputs;
    bool mFirstSample;
    MediaSource::ReadOptions mReadOptions;
    sp<IMediaSource> mSource;
    sp<MediaCodec> mDecoder;
    sp<AMessage> mOutputFormat;
    sp<Surface> mSurface;

private:
    AString mComponentName;
    sp<MetaData> mTrackMeta;
    OMX_COLOR_FORMATTYPE mDstFormat;
    ui::PixelFormat mCaptureFormat;
    int32_t mDstBpp;
    sp<IMemory> mFrameMemory;

    DISALLOW_EVIL_CONSTRUCTORS(FrameDecoder);
};
struct FrameCaptureLayer;

struct VideoFrameDecoder : public FrameDecoder {
    VideoFrameDecoder(
            const AString &componentName,
            const sp<MetaData> &trackMeta,
            const sp<IMediaSource> &source);

protected:
    virtual sp<AMessage> onGetFormatAndSeekOptions(
            int64_t frameTimeUs,
            int seekMode,
            MediaSource::ReadOptions *options,
            sp<Surface> *window) override;

    virtual status_t onExtractRect(FrameRect *rect) override {
        // Rect extraction for sequences is not supported for now.
        return (rect == NULL) ? OK : ERROR_UNSUPPORTED;
    }

    virtual status_t onInputReceived(
            const sp<MediaCodecBuffer> &codecBuffer,
            MetaDataBase &sampleMeta,
            bool firstSample,
            uint32_t *flags) override;

    virtual status_t onOutputReceived(
            const sp<MediaCodecBuffer> &videoFrameBuffer,
            const sp<AMessage> &outputFormat,
            int64_t timeUs,
            bool *done) override;

    virtual bool shouldDropOutput(int64_t ptsUs) override {
        return !((mTargetTimeUs < 0LL) || (ptsUs >= mTargetTimeUs));
    }

private:
    sp<FrameCaptureLayer> mCaptureLayer;
    VideoFrame *mFrame;
    bool mIsAvc;
    bool mIsHevc;
    MediaSource::ReadOptions::SeekMode mSeekMode;
    int64_t mTargetTimeUs;
    List<int64_t> mSampleDurations;
    int64_t mDefaultSampleDurationUs;

    sp<Surface> initSurface();
    status_t captureSurface();
};

struct MediaImageDecoder : public FrameDecoder {
   MediaImageDecoder(
            const AString &componentName,
            const sp<MetaData> &trackMeta,
            const sp<IMediaSource> &source);

protected:
    virtual ~MediaImageDecoder();

    virtual sp<AMessage> onGetFormatAndSeekOptions(
            int64_t frameTimeUs,
            int seekMode,
            MediaSource::ReadOptions *options,
            sp<Surface> *window) override;

    virtual status_t onExtractRect(FrameRect *rect) override;

    virtual status_t onInputReceived(
            const sp<MediaCodecBuffer> &codecBuffer __unused,
            MetaDataBase &sampleMeta __unused,
            bool firstSample __unused,
            uint32_t *flags __unused) override { return OK; }

    virtual status_t onOutputReceived(
            const sp<MediaCodecBuffer> &videoFrameBuffer,
            const sp<AMessage> &outputFormat,
            int64_t timeUs,
            bool *done) override;

    virtual status_t extractInternal() override;

private:
    VideoFrame *mFrame;
    int32_t mWidth;
    int32_t mHeight;
    int32_t mGridRows;
    int32_t mGridCols;
    int32_t mTileWidth;
    int32_t mTileHeight;
    int32_t mTilesDecoded;
    int32_t mTargetTiles;

    struct ImageInputThread;
    sp<ImageInputThread> mThread;
    bool mUseMultiThread;

    bool inputLoop();
};

}  // namespace android

#endif  // FRAME_DECODER_H_
