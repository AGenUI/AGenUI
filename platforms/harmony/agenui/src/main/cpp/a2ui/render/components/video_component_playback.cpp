#include "video_component.h"
#include "../a2ui_node.h"
#include "a2ui/utils/a2ui_thread_utils.h"
#include "log/a2ui_capi_log.h"

#include <cmath>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace a2ui {

void VideoComponent::onPlayerInfoCallback(OH_AVPlayer* player, AVPlayerOnInfoType type,
                                          OH_AVFormat* infoBody, void* userData) {
    VideoComponent* self = reinterpret_cast<VideoComponent*>(userData);
    if (!self || !player) return;
    if (!self->m_playerCallbackValid.load()) return;

    auto alive = self->m_alive;

    switch (type) {
    case AV_INFO_TYPE_STATE_CHANGE: {
        int32_t state = -1;
        int32_t stateChangeReason = -1;
        OH_AVFormat_GetIntValue(infoBody, OH_PLAYER_STATE, &state);
        OH_AVFormat_GetIntValue(infoBody, OH_PLAYER_STATE_CHANGE_REASON, &stateChangeReason);
        AVPlayerState capturedState = static_cast<AVPlayerState>(state);
        int32_t capturedReason = stateChangeReason;
        a2ui::postToMainThread([self, alive, capturedState, capturedReason]() {
            if (!alive->load()) return;
            if (!self->m_uiTasksEnabled.load()) return;
            self->handleStateChange(capturedState, capturedReason);
        });
        break;
    }
    case AV_INFO_TYPE_POSITION_UPDATE: {
        int32_t currentPosition = 0;
        OH_AVFormat_GetIntValue(infoBody, OH_PLAYER_CURRENT_POSITION, &currentPosition);
        int64_t capturedPos = static_cast<int64_t>(currentPosition);
        a2ui::postToMainThread([self, alive, capturedPos]() {
            if (!alive->load()) return;
            if (!self->m_uiTasksEnabled.load()) return;
            self->m_currentPosition = capturedPos;

            if (self->m_controlsBarVisible) {
                self->updateProgressDisplay();

                if (!self->m_isSeeking) {
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - self->m_controlsShowTime).count();
                    if (elapsed >= kControlsAutoHideMs) {
                        self->hideControlsBar();
                    }
                }
            }
        });
        break;
    }
    case AV_INFO_TYPE_DURATION_UPDATE: {
        int64_t duration = -1;
        OH_AVFormat_GetLongValue(infoBody, OH_PLAYER_DURATION, &duration);
        int64_t capturedDuration = duration;
        a2ui::postToMainThread([self, alive, capturedDuration]() {
            if (!alive->load()) return;
            if (!self->m_uiTasksEnabled.load()) return;
            self->m_duration = capturedDuration;

            if (self->m_sliderHandle && capturedDuration > 0) {
                A2UISliderNode(self->m_sliderHandle).setMaxValue(static_cast<float>(capturedDuration));
            }

            if (self->m_totalTimeHandle) {
                A2UITextNode(self->m_totalTimeHandle).setTextContent(formatTime(capturedDuration));
            }
        });
        break;
    }
    case AV_INFO_TYPE_RESOLUTION_CHANGE: {
        int32_t videoWidth = 0, videoHeight = 0;
        OH_AVFormat_GetIntValue(infoBody, OH_PLAYER_VIDEO_WIDTH, &videoWidth);
        OH_AVFormat_GetIntValue(infoBody, OH_PLAYER_VIDEO_HEIGHT, &videoHeight);
        float capturedW = static_cast<float>(videoWidth);
        float capturedH = static_cast<float>(videoHeight);
        HM_LOGI("VideoComponent - Resolution: %dx%d, id=%s",
                    videoWidth, videoHeight, self->m_id.c_str());
        a2ui::postToMainThread([self, alive, capturedW, capturedH]() {
            if (!alive->load()) return;
            if (!self->m_uiTasksEnabled.load()) return;
            self->m_videoIntrinsicWidth = capturedW;
            self->m_videoIntrinsicHeight = capturedH;
            self->notifyIntrinsicSizeIfNeeded();
        });
        break;
    }
    case AV_INFO_TYPE_MESSAGE: {
        int32_t messageType = -1;
        OH_AVFormat_GetIntValue(infoBody, OH_PLAYER_MESSAGE_TYPE, &messageType);
        if (messageType == 1) {
            HM_LOGI("VideoComponent - First frame rendered, id=%s", self->m_id.c_str());
        }
        break;
    }
    default:
        break;
    }
}

void VideoComponent::onPlayerErrorCallback(OH_AVPlayer* player, int32_t errorCode,
                                           const char* errorMsg, void* userData) {
    VideoComponent* self = reinterpret_cast<VideoComponent*>(userData);
    if (!self) return;
    if (!self->m_playerCallbackValid.load()) return;

    auto alive = self->m_alive;
    std::string capturedMsg = errorMsg ? errorMsg : "unknown";
    HM_LOGE("VideoComponent - Player error: code=%d, msg=%s, id=%s",
                 errorCode, capturedMsg.c_str(), self->m_id.c_str());
    a2ui::postToMainThread([self, alive, capturedMsg]() {
        if (!alive->load()) return;
        if (!self->m_uiTasksEnabled.load()) return;
        self->m_videoCurrentState = "error";
        self->m_isPlaying = false;
        self->updatePlayPauseButton();
    });
}

void VideoComponent::notifyIntrinsicSizeIfNeeded() {
    if (m_videoIntrinsicWidth <= 0.0f || m_videoIntrinsicHeight <= 0.0f) {
        return;
    }

    float currentWidth = getWidth();
    float currentHeight = getHeight();
    if (currentWidth > 2.0f && currentHeight > 2.0f) {
        HM_LOGI("Skip, layout already resolved: %.1fx%.1f, id=%s",
            currentWidth, currentHeight, m_id.c_str());
        return;
    }

    float reportWidth = m_videoIntrinsicWidth;
    float reportHeight = m_videoIntrinsicHeight;

    constexpr float kNotifyEpsilon = 0.01f;
    if (std::fabs(reportWidth - m_lastNotifiedWidth) <= kNotifyEpsilon
        && std::fabs(reportHeight - m_lastNotifiedHeight) <= kNotifyEpsilon) {
        return;
    }

    m_lastNotifiedWidth = reportWidth;
    m_lastNotifiedHeight = reportHeight;

    agenui::IComponentRenderObservable* observable = getComponentRenderObservable();
    if (!observable) {
        HM_LOGW("ComponentRenderObservable not set, id=%s", m_id.c_str());
        return;
    }

    agenui::ComponentRenderInfo info;
    info.surfaceId = getSurfaceId();
    info.componentId = getId();
    info.type = getComponentType();
    info.width = UnitConverter::pxToA2ui(reportWidth);
    info.height = UnitConverter::pxToA2ui(reportHeight);

    HM_LOGI("Notify layout with intrinsic size: %.1fx%.1f (current=%.1fx%.1f), id=%s",
        reportWidth, reportHeight, currentWidth, currentHeight, m_id.c_str());
    observable->notifyRenderFinish(info);
}

void VideoComponent::setVideoUrl(const std::string& url) {
    HM_LOGI("url=%s, state=%s, id=%s",
                url.c_str(), m_videoCurrentState.c_str(), m_id.c_str());

    if (m_videoUrl == url && !m_videoCurrentState.empty() && m_videoCurrentState != "error") {
        return;
    }

    m_videoUrl = url;
    m_currentPosition = 0;
    m_duration = 0;
    m_pendingPrepare = false;

    if (!m_avPlayer || url.empty()) return;

    if (!m_videoCurrentState.empty()
        && m_videoCurrentState != "idle"
        && m_videoCurrentState != "destroyed") {
        m_pendingPrepare = true;
        HM_LOGI("Resetting player (async), pendingPrepare=true, id=%s", m_id.c_str());
        OH_AVPlayer_Reset(m_avPlayer);
        return;
    }

    if (m_window) {
        handleVideoPrepare(url);
    }
}

OH_AVErrCode VideoComponent::handleVideoPrepare(const std::string& url) {
    OH_AVErrCode errCode = AV_ERR_OK;

    if (!m_window || !m_avPlayer || url.empty()) {
        HM_LOGW("Not ready: window=%p, avPlayer=%p, url=%s",
                    m_window, m_avPlayer, url.c_str());
        return AV_ERR_INVALID_VAL;
    }

    HM_LOGI("Starting: url=%s, id=%s", url.c_str(), m_id.c_str());

    if (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0) {
        errCode = OH_AVPlayer_SetURLSource(m_avPlayer, url.c_str());
        if (errCode != AV_ERR_OK) {
            HM_LOGE("SetURLSource failed: %d", errCode);
            return errCode;
        }
    } else {
        const char* filePath = url.c_str();
        if (url.rfind("file://", 0) == 0) {
            filePath = url.c_str() + strlen("file://");
        }
        int fd = open(filePath, O_RDONLY);
        if (fd < 0) {
            HM_LOGE("open file failed: %s, errno=%d", filePath, errno);
            return AV_ERR_IO;
        }
        struct stat fileStat;
        if (fstat(fd, &fileStat) != 0) {
            HM_LOGE("fstat failed, errno=%d", errno);
            close(fd);
            return AV_ERR_IO;
        }
        int64_t fileSize = fileStat.st_size;
        errCode = OH_AVPlayer_SetFDSource(m_avPlayer, fd, 0, fileSize);
        close(fd);
        if (errCode != AV_ERR_OK) {
            HM_LOGE("SetFDSource failed: %d", errCode);
            return errCode;
        }
    }

    errCode = OH_AVPlayer_SetVideoSurface(m_avPlayer, m_window);
    if (errCode != AV_ERR_OK) {
        HM_LOGE("SetVideoSurface failed: %d", errCode);
        return errCode;
    }

    errCode = OH_AVPlayer_Prepare(m_avPlayer);
    if (errCode != AV_ERR_OK) {
        HM_LOGE("Prepare failed: %d", errCode);
        return errCode;
    }

    HM_LOGI("Success, id=%s", m_id.c_str());
    return errCode;
}

void VideoComponent::handleStateChange(AVPlayerState state, int32_t stateChangeReason) {
    (void)stateChangeReason;

    switch (state) {
    case AV_IDLE:
        m_videoCurrentState = "idle";
        m_isPlaying = false;
        HM_LOGI("VideoComponent - State: IDLE, pendingPrepare=%d, window=%p, id=%s",
                    m_pendingPrepare ? 1 : 0, m_window, m_id.c_str());
        if (m_pendingPrepare && !m_videoUrl.empty() && m_window) {
            m_pendingPrepare = false;
            handleVideoPrepare(m_videoUrl);
        }
        break;

    case AV_INITIALIZED:
        m_videoCurrentState = "preparing";
        HM_LOGI("VideoComponent - State: INITIALIZED, id=%s", m_id.c_str());
        break;

    case AV_PREPARED:
        m_videoCurrentState = "prepared";
        HM_LOGI("VideoComponent - State: PREPARED, autoPlay=%d, id=%s",
                    m_autoPlay ? 1 : 0, m_id.c_str());
        if (m_autoPlay) {
            OH_AVErrCode playErr = play();
            if (playErr != AV_ERR_OK) {
                HM_LOGE("VideoComponent - AutoPlay failed: %d, id=%s",
                             playErr, m_id.c_str());
            }
        }
        break;

    case AV_PLAYING:
        m_videoCurrentState = "playing";
        m_isPlaying = true;
        HM_LOGI("VideoComponent - State: PLAYING, id=%s", m_id.c_str());
        updatePlayPauseButton();
        break;

    case AV_PAUSED:
        m_videoCurrentState = "paused";
        m_isPlaying = false;
        HM_LOGI("VideoComponent - State: PAUSED, id=%s", m_id.c_str());
        updatePlayPauseButton();
        break;

    case AV_STOPPED:
        m_videoCurrentState = "stopped";
        m_isPlaying = false;
        HM_LOGI("VideoComponent - State: STOPPED, id=%s", m_id.c_str());
        updatePlayPauseButton();
        break;

    case AV_COMPLETED:
        m_videoCurrentState = "completed";
        m_isPlaying = false;
        HM_LOGI("VideoComponent - State: COMPLETED, id=%s", m_id.c_str());
        updatePlayPauseButton();
        if (m_controls) {
            showControlsBar();
        }
        break;

    case AV_RELEASED:
        m_videoCurrentState = "destroyed";
        m_isPlaying = false;
        HM_LOGI("VideoComponent - State: RELEASED, id=%s", m_id.c_str());
        break;

    case AV_ERROR:
        m_videoCurrentState = "error";
        m_isPlaying = false;
        HM_LOGE("VideoComponent - State: ERROR, id=%s", m_id.c_str());
        break;

    default:
        break;
    }
}

OH_AVErrCode VideoComponent::play() {
    if (!m_avPlayer) return AV_ERR_INVALID_VAL;
    OH_AVErrCode code = OH_AVPlayer_Play(m_avPlayer);
    HM_LOGI("result=%d, id=%s", code, m_id.c_str());
    return code;
}

OH_AVErrCode VideoComponent::pause() {
    if (!m_avPlayer) return AV_ERR_INVALID_VAL;
    OH_AVErrCode code = OH_AVPlayer_Pause(m_avPlayer);
    HM_LOGI("result=%d, id=%s", code, m_id.c_str());
    return code;
}

OH_AVErrCode VideoComponent::stop() {
    if (!m_avPlayer) return AV_ERR_INVALID_VAL;
    OH_AVErrCode code = OH_AVPlayer_Stop(m_avPlayer);
    HM_LOGI("result=%d, id=%s", code, m_id.c_str());
    return code;
}

void VideoComponent::releasePlayer() {
    m_alive->store(false);

    if (m_avPlayer) {
        m_playerCallbackValid.store(false);
        m_uiTasksEnabled.store(false);
        OH_AVPlayer_SetOnInfoCallback(m_avPlayer, nullptr, nullptr);
        OH_AVPlayer_SetOnErrorCallback(m_avPlayer, nullptr, nullptr);

        OH_AVErrCode code = OH_AVPlayer_Release(m_avPlayer);
        HM_LOGI("result=%d, id=%s", code, m_id.c_str());
        m_avPlayer = nullptr;
    }
    m_isPlaying = false;
    m_videoCurrentState = "destroyed";
}

} // namespace a2ui
