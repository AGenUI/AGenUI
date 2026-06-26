#include "audio_player_component.h"
#include "a2ui/utils/a2ui_thread_utils.h"
#include "log/a2ui_capi_log.h"

namespace a2ui {

void AudioPlayerComponent::onPlayerInfoCallback(OH_AVPlayer* player, AVPlayerOnInfoType type,
                                                OH_AVFormat* infoBody, void* userData) {
    AudioPlayerComponent* self = reinterpret_cast<AudioPlayerComponent*>(userData);
    if (!self || !player) return;

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
            if (self->m_audioCurrentState == "playing") {
                self->updateProgressRing();
            }
        });
        break;
    }
    case AV_INFO_TYPE_DURATION_UPDATE: {
        int64_t duration = -1;
        OH_AVFormat_GetLongValue(infoBody, OH_PLAYER_DURATION, &duration);
        int64_t capturedDuration = duration;
        HM_LOGI("AudioPlayerComponent - Duration: %lld ms, id=%s",
                duration, self->m_id.c_str());
        a2ui::postToMainThread([self, alive, capturedDuration]() {
            if (!alive->load()) return;
            if (!self->m_uiTasksEnabled.load()) return;
            self->m_duration = capturedDuration;
            if (self->m_audioCurrentState == "playing") {
                self->updateProgressRing();
            }
        });
        break;
    }
    default:
        break;
    }
}

void AudioPlayerComponent::onPlayerErrorCallback(OH_AVPlayer* player, int32_t errorCode,
                                                 const char* errorMsg, void* userData) {
    AudioPlayerComponent* self = reinterpret_cast<AudioPlayerComponent*>(userData);
    if (!self) return;

    auto alive = self->m_alive;
    std::string capturedMsg = errorMsg ? errorMsg : "unknown";
    HM_LOGE("AudioPlayerComponent - Player error: code=%d, msg=%s, id=%s",
            errorCode, capturedMsg.c_str(), self->m_id.c_str());
    a2ui::postToMainThread([self, alive, capturedMsg]() {
        if (!alive->load()) return;
        if (!self->m_uiTasksEnabled.load()) return;
        self->m_audioCurrentState = "error";
        self->m_isPlaying = false;
        self->m_pendingPlay = false;
        self->applyVisualState();
    });
}

void AudioPlayerComponent::setAudioUrl(const std::string& url) {
    HM_LOGI("url=%s, id=%s", url.c_str(), m_id.c_str());

    if (m_audioUrl == url) {
        return;
    }

    m_audioUrl = url;
    m_currentPosition = 0;
    m_duration = 0;
    m_isPlaying = false;
    m_pendingPlay = false;

    if (!m_avPlayer) {
        return;
    }

    if (m_audioCurrentState == "playing" || m_audioCurrentState == "paused" ||
        m_audioCurrentState == "prepared" || m_audioCurrentState == "completed" ||
        m_audioCurrentState == "error") {
        OH_AVPlayer_Reset(m_avPlayer);
    }

    m_audioCurrentState = "preparing";
    applyVisualState();

    OH_AVErrCode errCode = handleAudioPrepare(url);
    if (errCode != AV_ERR_OK) {
        m_audioCurrentState = "error";
        applyVisualState();
    }
}

OH_AVErrCode AudioPlayerComponent::handleAudioPrepare(const std::string& url) {
    if (!m_avPlayer || url.empty()) {
        HM_LOGW("Not ready: avPlayer=%p, url=%s",
                m_avPlayer, url.c_str());
        return AV_ERR_INVALID_VAL;
    }

    HM_LOGI("Starting: url=%s, id=%s",
            url.c_str(), m_id.c_str());

    OH_AVErrCode errCode = OH_AVPlayer_SetURLSource(m_avPlayer, url.c_str());
    if (errCode != AV_ERR_OK) {
        HM_LOGE("SetURLSource failed: %d", errCode);
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

void AudioPlayerComponent::handleStateChange(AVPlayerState state, int32_t stateChangeReason) {
    (void)stateChangeReason;

    switch (state) {
    case AV_IDLE:
        m_audioCurrentState = "idle";
        m_isPlaying = false;
        m_pendingPlay = false;
        HM_LOGI("AudioPlayerComponent - State: IDLE, id=%s", m_id.c_str());
        break;

    case AV_INITIALIZED:
        m_audioCurrentState = "preparing";
        m_isPlaying = false;
        HM_LOGI("AudioPlayerComponent - State: INITIALIZED, id=%s", m_id.c_str());
        break;

    case AV_PREPARED:
        m_audioCurrentState = "prepared";
        m_isPlaying = false;
        HM_LOGI("AudioPlayerComponent - State: PREPARED, pendingPlay=%d, id=%s",
                m_pendingPlay ? 1 : 0, m_id.c_str());
        if (m_pendingPlay) {
            m_pendingPlay = false;
            OH_AVErrCode playErr = play();
            if (playErr != AV_ERR_OK) {
                HM_LOGE("AudioPlayerComponent - PendingPlay failed: %d, id=%s",
                        playErr, m_id.c_str());
                m_audioCurrentState = "error";
            }
        }
        break;

    case AV_PLAYING:
        m_audioCurrentState = "playing";
        m_isPlaying = true;
        HM_LOGI("AudioPlayerComponent - State: PLAYING, id=%s", m_id.c_str());
        break;

    case AV_PAUSED:
        m_audioCurrentState = "paused";
        m_isPlaying = false;
        HM_LOGI("AudioPlayerComponent - State: PAUSED, id=%s", m_id.c_str());
        break;

    case AV_STOPPED:
        m_audioCurrentState = "stopped";
        m_isPlaying = false;
        m_currentPosition = 0;
        HM_LOGI("AudioPlayerComponent - State: STOPPED, id=%s", m_id.c_str());
        break;

    case AV_COMPLETED:
        m_audioCurrentState = "completed";
        m_isPlaying = false;
        m_currentPosition = 0;
        HM_LOGI("AudioPlayerComponent - State: COMPLETED, id=%s", m_id.c_str());
        break;

    case AV_RELEASED:
        m_audioCurrentState = "destroyed";
        m_isPlaying = false;
        HM_LOGI("AudioPlayerComponent - State: RELEASED, id=%s", m_id.c_str());
        break;

    case AV_ERROR:
        m_audioCurrentState = "error";
        m_isPlaying = false;
        m_pendingPlay = false;
        HM_LOGE("AudioPlayerComponent - State: ERROR, id=%s", m_id.c_str());
        break;

    default:
        break;
    }

    applyVisualState();
}

OH_AVErrCode AudioPlayerComponent::play() {
    if (!m_avPlayer) return AV_ERR_INVALID_VAL;
    OH_AVErrCode code = OH_AVPlayer_Play(m_avPlayer);
    HM_LOGI("result=%d, id=%s", code, m_id.c_str());
    return code;
}

OH_AVErrCode AudioPlayerComponent::pause() {
    if (!m_avPlayer) return AV_ERR_INVALID_VAL;
    OH_AVErrCode code = OH_AVPlayer_Pause(m_avPlayer);
    HM_LOGI("result=%d, id=%s", code, m_id.c_str());
    return code;
}

OH_AVErrCode AudioPlayerComponent::stop() {
    if (!m_avPlayer) return AV_ERR_INVALID_VAL;
    OH_AVErrCode code = OH_AVPlayer_Stop(m_avPlayer);
    HM_LOGI("result=%d, id=%s", code, m_id.c_str());
    return code;
}

void AudioPlayerComponent::releasePlayer() {
    m_alive->store(false);

    if (m_avPlayer) {
        m_uiTasksEnabled.store(false);
        OH_AVPlayer_SetOnInfoCallback(m_avPlayer, nullptr, nullptr);
        OH_AVPlayer_SetOnErrorCallback(m_avPlayer, nullptr, nullptr);

        OH_AVErrCode code = OH_AVPlayer_Release(m_avPlayer);
        HM_LOGI("result=%d, id=%s", code, m_id.c_str());
        m_avPlayer = nullptr;
    }

    m_isPlaying = false;
    m_pendingPlay = false;
    m_audioCurrentState = "destroyed";
}

} // namespace a2ui
