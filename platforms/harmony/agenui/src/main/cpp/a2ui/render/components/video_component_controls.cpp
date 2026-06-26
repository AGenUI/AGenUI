#include "video_component.h"
#include "../a2ui_node.h"
#include "a2ui/utils/a2ui_color_palette.h"
#include "log/a2ui_capi_log.h"

#include <algorithm>
#include <sstream>
#include <iomanip>

namespace a2ui {

using colors::kColorWhite;

void VideoComponent::createControlsBar() {
    m_controlsBarHandle = g_nodeAPI->createNode(ARKUI_NODE_COLUMN);

    {
        A2UIColumnNode bar(m_controlsBarHandle);
        bar.setPercentWidth(1.0f);
        bar.setPercentHeight(1.0f);
        bar.setJustifyContent(ARKUI_FLEX_ALIGNMENT_END);
        bar.setHitTestBehavior(ARKUI_HIT_TEST_MODE_TRANSPARENT);
    }

    m_buttonsRowHandle = g_nodeAPI->createNode(ARKUI_NODE_ROW);

    {
        A2UIRowNode btnRow(m_buttonsRowHandle);
        btnRow.setPercentWidth(1.0f);
        btnRow.setHeight(96.0f);
        btnRow.setBackgroundColor(0xAA000000);
        btnRow.setJustifyContent(ARKUI_FLEX_ALIGNMENT_CENTER);
        btnRow.setAlignItems(ARKUI_VERTICAL_ALIGNMENT_CENTER);
    }

    m_rewindBtnHandle = g_nodeAPI->createNode(ARKUI_NODE_TEXT);
    {
        A2UITextNode btn(m_rewindBtnHandle);
        btn.setTextContent("\xe2\x8f\xaa");
        btn.setFontSize(48.0f);
        btn.setFontColor(kColorWhite);
        btn.setWidth(96.0f);
        btn.setHeight(96.0f);
        btn.setTextAlign(ARKUI_TEXT_ALIGNMENT_CENTER);
        btn.setMargin(0.0f, 32.0f, 0.0f, 32.0f);
    }
    g_nodeAPI->addNodeEventReceiver(m_rewindBtnHandle, onRewindBtnClickEvent);
    g_nodeAPI->registerNodeEvent(m_rewindBtnHandle, NODE_ON_CLICK, 0, this);

    m_playPauseBtnHandle = g_nodeAPI->createNode(ARKUI_NODE_TEXT);
    {
        A2UITextNode btn(m_playPauseBtnHandle);
        btn.setTextContent("\xe2\x96\xb6");
        btn.setFontSize(48.0f);
        btn.setFontColor(kColorWhite);
        btn.setWidth(96.0f);
        btn.setHeight(96.0f);
        btn.setTextAlign(ARKUI_TEXT_ALIGNMENT_CENTER);
        btn.setMargin(0.0f, 32.0f, 0.0f, 32.0f);
    }
    g_nodeAPI->addNodeEventReceiver(m_playPauseBtnHandle, onPlayPauseBtnClickEvent);
    g_nodeAPI->registerNodeEvent(m_playPauseBtnHandle, NODE_ON_CLICK, 0, this);

    m_forwardBtnHandle = g_nodeAPI->createNode(ARKUI_NODE_TEXT);
    {
        A2UITextNode btn(m_forwardBtnHandle);
        btn.setTextContent("\xe2\x8f\xa9");
        btn.setFontSize(48.0f);
        btn.setFontColor(kColorWhite);
        btn.setWidth(96.0f);
        btn.setHeight(96.0f);
        btn.setTextAlign(ARKUI_TEXT_ALIGNMENT_CENTER);
        btn.setMargin(0.0f, 32.0f, 0.0f, 32.0f);
    }
    g_nodeAPI->addNodeEventReceiver(m_forwardBtnHandle, onForwardBtnClickEvent);
    g_nodeAPI->registerNodeEvent(m_forwardBtnHandle, NODE_ON_CLICK, 0, this);

    g_nodeAPI->addChild(m_buttonsRowHandle, m_rewindBtnHandle);
    g_nodeAPI->addChild(m_buttonsRowHandle, m_playPauseBtnHandle);
    g_nodeAPI->addChild(m_buttonsRowHandle, m_forwardBtnHandle);

    m_controlsRowHandle = g_nodeAPI->createNode(ARKUI_NODE_ROW);

    {
        A2UIRowNode row(m_controlsRowHandle);
        row.setPercentWidth(1.0f);
        row.setHeight(64.0f);
        row.setBackgroundColor(0xAA000000);
        row.setPadding(0.0f, 16.0f, 8.0f, 16.0f);
        row.setAlignItems(ARKUI_VERTICAL_ALIGNMENT_CENTER);
    }

    m_currentTimeHandle = g_nodeAPI->createNode(ARKUI_NODE_TEXT);
    {
        A2UITextNode t(m_currentTimeHandle);
        t.setTextContent("00:00");
        t.setFontSize(24.0f);
        t.setFontColor(kColorWhite);
        t.setMargin(0.0f, 8.0f, 0.0f, 0.0f);
    }

    m_sliderHandle = g_nodeAPI->createNode(ARKUI_NODE_SLIDER);
    {
        A2UISliderNode slider(m_sliderHandle);
        slider.setMinValue(0.0f);
        slider.setMaxValue(100.0f);
        slider.setValue(0.0f);
        slider.setLayoutWeight(1.0f);
        slider.setHeight(48.0f);
    }
    g_nodeAPI->addNodeEventReceiver(m_sliderHandle, onSliderChangeEvent);
    g_nodeAPI->registerNodeEvent(m_sliderHandle, NODE_SLIDER_EVENT_ON_CHANGE, 0, this);

    m_totalTimeHandle = g_nodeAPI->createNode(ARKUI_NODE_TEXT);
    {
        A2UITextNode t(m_totalTimeHandle);
        t.setTextContent("00:00");
        t.setFontSize(24.0f);
        t.setFontColor(kColorWhite);
        t.setMargin(0.0f, 0.0f, 0.0f, 8.0f);
    }

    g_nodeAPI->addChild(m_controlsRowHandle, m_currentTimeHandle);
    g_nodeAPI->addChild(m_controlsRowHandle, m_sliderHandle);
    g_nodeAPI->addChild(m_controlsRowHandle, m_totalTimeHandle);

    g_nodeAPI->addChild(m_controlsBarHandle, m_buttonsRowHandle);
    g_nodeAPI->addChild(m_controlsBarHandle, m_controlsRowHandle);

    g_nodeAPI->addChild(m_nodeHandle, m_controlsBarHandle);

    hideControlsBar();

    HM_LOGI("Controls bar created (2-row layout), id=%s", m_id.c_str());
}

void VideoComponent::destroyControlsBar() {
    if (m_rewindBtnHandle) {
        g_nodeAPI->unregisterNodeEvent(m_rewindBtnHandle, NODE_ON_CLICK);
    }
    if (m_playPauseBtnHandle) {
        g_nodeAPI->unregisterNodeEvent(m_playPauseBtnHandle, NODE_ON_CLICK);
    }
    if (m_forwardBtnHandle) {
        g_nodeAPI->unregisterNodeEvent(m_forwardBtnHandle, NODE_ON_CLICK);
    }
    if (m_sliderHandle) {
        g_nodeAPI->unregisterNodeEvent(m_sliderHandle, NODE_SLIDER_EVENT_ON_CHANGE);
    }

    if (m_buttonsRowHandle) {
        if (m_rewindBtnHandle) {
            g_nodeAPI->removeChild(m_buttonsRowHandle, m_rewindBtnHandle);
            g_nodeAPI->disposeNode(m_rewindBtnHandle);
            m_rewindBtnHandle = nullptr;
        }
        if (m_playPauseBtnHandle) {
            g_nodeAPI->removeChild(m_buttonsRowHandle, m_playPauseBtnHandle);
            g_nodeAPI->disposeNode(m_playPauseBtnHandle);
            m_playPauseBtnHandle = nullptr;
        }
        if (m_forwardBtnHandle) {
            g_nodeAPI->removeChild(m_buttonsRowHandle, m_forwardBtnHandle);
            g_nodeAPI->disposeNode(m_forwardBtnHandle);
            m_forwardBtnHandle = nullptr;
        }
    }

    if (m_controlsRowHandle) {
        if (m_currentTimeHandle) {
            g_nodeAPI->removeChild(m_controlsRowHandle, m_currentTimeHandle);
            g_nodeAPI->disposeNode(m_currentTimeHandle);
            m_currentTimeHandle = nullptr;
        }
        if (m_sliderHandle) {
            g_nodeAPI->removeChild(m_controlsRowHandle, m_sliderHandle);
            g_nodeAPI->disposeNode(m_sliderHandle);
            m_sliderHandle = nullptr;
        }
        if (m_totalTimeHandle) {
            g_nodeAPI->removeChild(m_controlsRowHandle, m_totalTimeHandle);
            g_nodeAPI->disposeNode(m_totalTimeHandle);
            m_totalTimeHandle = nullptr;
        }
    }

    if (m_controlsBarHandle) {
        if (m_buttonsRowHandle) {
            g_nodeAPI->removeChild(m_controlsBarHandle, m_buttonsRowHandle);
            g_nodeAPI->disposeNode(m_buttonsRowHandle);
            m_buttonsRowHandle = nullptr;
        }
        if (m_controlsRowHandle) {
            g_nodeAPI->removeChild(m_controlsBarHandle, m_controlsRowHandle);
            g_nodeAPI->disposeNode(m_controlsRowHandle);
            m_controlsRowHandle = nullptr;
        }
        if (m_nodeHandle) {
            g_nodeAPI->removeChild(m_nodeHandle, m_controlsBarHandle);
        }
        g_nodeAPI->disposeNode(m_controlsBarHandle);
        m_controlsBarHandle = nullptr;
    }
}

void VideoComponent::toggleControlsBar() {
    if (m_controlsBarVisible) {
        hideControlsBar();
    } else {
        showControlsBar();
    }
}

void VideoComponent::showControlsBar() {
    if (!m_controls || !m_controlsBarHandle) return;

    updateControlsBarLayout();
    A2UINode(m_controlsBarHandle).setVisibility(ARKUI_VISIBILITY_VISIBLE);

    m_controlsBarVisible = true;
    m_controlsShowTime = std::chrono::steady_clock::now();

    updatePlayPauseButton();
    updateProgressDisplay();

    HM_LOGI("id=%s", m_id.c_str());
}

void VideoComponent::hideControlsBar() {
    if (!m_controlsBarHandle) return;

    A2UINode(m_controlsBarHandle).setVisibility(ARKUI_VISIBILITY_HIDDEN);

    m_controlsBarVisible = false;
}

void VideoComponent::updateControlsBarLayout() {
    if (!m_controlsBarHandle || !m_buttonsRowHandle || !m_controlsRowHandle ||
        !m_rewindBtnHandle || !m_playPauseBtnHandle || !m_forwardBtnHandle ||
        !m_currentTimeHandle || !m_sliderHandle || !m_totalTimeHandle) {
        return;
    }

    const float componentWidth = getWidth();
    const float componentHeight = getHeight();
    if (componentWidth <= 0.0f || componentHeight <= 0.0f) {
        return;
    }

    constexpr float kBaseButtonsTotalWidth = 480.0f;
    constexpr float kBaseControlsTotalHeight = 160.0f;
    const float scale = std::max(
        0.2f,
        std::min({1.0f,
                  componentWidth / kBaseButtonsTotalWidth,
                  componentHeight / kBaseControlsTotalHeight}));

    const float buttonSize = 96.0f * scale;
    const float buttonFontSize = 48.0f * scale;
    const float buttonHorizontalMargin = 32.0f * scale;
    const float buttonRowHeight = 96.0f * scale;

    const float progressRowHeight = 64.0f * scale;
    const float progressPaddingRight = 16.0f * scale;
    const float progressPaddingBottom = 8.0f * scale;
    const float progressPaddingLeft = 16.0f * scale;
    const float timeFontSize = 24.0f * scale;
    const float currentTimeMarginRight = 8.0f * scale;
    const float totalTimeMarginLeft = 8.0f * scale;
    const float sliderHeight = 48.0f * scale;

    {
        A2UIRowNode buttonsRow(m_buttonsRowHandle);
        buttonsRow.setHeight(buttonRowHeight);
    }

    auto updateButton = [&](ArkUI_NodeHandle handle) {
        A2UITextNode btn(handle);
        btn.setFontSize(buttonFontSize);
        btn.setWidth(buttonSize);
        btn.setHeight(buttonSize);
        btn.setMargin(0.0f, buttonHorizontalMargin, 0.0f, buttonHorizontalMargin);
    };
    updateButton(m_rewindBtnHandle);
    updateButton(m_playPauseBtnHandle);
    updateButton(m_forwardBtnHandle);

    {
        A2UIRowNode progressRow(m_controlsRowHandle);
        progressRow.setHeight(progressRowHeight);
        progressRow.setPadding(0.0f, progressPaddingRight, progressPaddingBottom, progressPaddingLeft);
    }

    {
        A2UITextNode currentTime(m_currentTimeHandle);
        currentTime.setFontSize(timeFontSize);
        currentTime.setMargin(0.0f, currentTimeMarginRight, 0.0f, 0.0f);
    }

    {
        A2UISliderNode slider(m_sliderHandle);
        slider.setHeight(sliderHeight);
    }

    {
        A2UITextNode totalTime(m_totalTimeHandle);
        totalTime.setFontSize(timeFontSize);
        totalTime.setMargin(0.0f, 0.0f, 0.0f, totalTimeMarginLeft);
    }
}

void VideoComponent::updatePlayPauseButton() {
    if (!m_playPauseBtnHandle) return;

    const char* btnText = m_isPlaying ? "\xe2\x8f\xb8" : "\xe2\x96\xb6";
    A2UITextNode(m_playPauseBtnHandle).setTextContent(btnText);
}

void VideoComponent::updateProgressDisplay() {
    if (!m_currentTimeHandle || !m_totalTimeHandle || !m_sliderHandle) return;

    A2UITextNode(m_currentTimeHandle).setTextContent(formatTime(m_currentPosition));
    A2UITextNode(m_totalTimeHandle).setTextContent(formatTime(m_duration));

    if (!m_isSeeking) {
        if (m_duration > 0) {
            A2UISliderNode(m_sliderHandle).setMaxValue(static_cast<float>(m_duration));
        }
        A2UISliderNode(m_sliderHandle).setValue(static_cast<float>(m_currentPosition));
    }
}

std::string VideoComponent::formatTime(int64_t milliseconds) {
    if (milliseconds <= 0) return "00:00";

    int64_t totalSeconds = milliseconds / 1000;
    int64_t minutes = totalSeconds / 60;
    int64_t seconds = totalSeconds % 60;

    std::ostringstream stream;
    if (minutes >= 60) {
        int64_t hours = minutes / 60;
        minutes = minutes % 60;
        stream << std::setw(2) << std::setfill('0') << hours << ":";
    }
    stream << std::setw(2) << std::setfill('0') << minutes << ":"
           << std::setw(2) << std::setfill('0') << seconds;
    return stream.str();
}

void VideoComponent::onVideoAreaClickEvent(ArkUI_NodeEvent* event) {
    void* userData = OH_ArkUI_NodeEvent_GetUserData(event);
    if (!userData) return;

    VideoComponent* self = static_cast<VideoComponent*>(userData);
    HM_LOGI("id=%s", self->m_id.c_str());
    self->toggleControlsBar();
}

void VideoComponent::onRewindBtnClickEvent(ArkUI_NodeEvent* event) {
    void* userData = OH_ArkUI_NodeEvent_GetUserData(event);
    if (!userData) return;

    VideoComponent* self = static_cast<VideoComponent*>(userData);
    if (!self->m_avPlayer) return;

    int32_t targetPosition = static_cast<int32_t>(self->m_currentPosition) - kSeekStepMs;
    if (targetPosition < 0) targetPosition = 0;

    OH_AVPlayer_Seek(self->m_avPlayer, targetPosition, AV_SEEK_NEXT_SYNC);
    self->m_currentPosition = targetPosition;
    self->updateProgressDisplay();

    self->m_controlsShowTime = std::chrono::steady_clock::now();

    HM_LOGI("Rewind to %d ms, id=%s", targetPosition, self->m_id.c_str());
}

void VideoComponent::onPlayPauseBtnClickEvent(ArkUI_NodeEvent* event) {
    void* userData = OH_ArkUI_NodeEvent_GetUserData(event);
    if (!userData) return;

    VideoComponent* self = static_cast<VideoComponent*>(userData);
    HM_LOGI("isPlaying=%d, id=%s", self->m_isPlaying ? 1 : 0, self->m_id.c_str());

    if (self->m_isPlaying) {
        self->pause();
    } else {
        if (self->m_videoCurrentState == "completed") {
            OH_AVPlayer_Seek(self->m_avPlayer, 0, AV_SEEK_NEXT_SYNC);
        }
        self->play();
    }

    self->updatePlayPauseButton();
    self->m_controlsShowTime = std::chrono::steady_clock::now();
}

void VideoComponent::onForwardBtnClickEvent(ArkUI_NodeEvent* event) {
    void* userData = OH_ArkUI_NodeEvent_GetUserData(event);
    if (!userData) return;

    VideoComponent* self = static_cast<VideoComponent*>(userData);
    if (!self->m_avPlayer) return;

    int32_t targetPosition = static_cast<int32_t>(self->m_currentPosition) + kSeekStepMs;
    if (self->m_duration > 0 && targetPosition > static_cast<int32_t>(self->m_duration)) {
        targetPosition = static_cast<int32_t>(self->m_duration);
    }

    OH_AVPlayer_Seek(self->m_avPlayer, targetPosition, AV_SEEK_NEXT_SYNC);
    self->m_currentPosition = targetPosition;
    self->updateProgressDisplay();

    self->m_controlsShowTime = std::chrono::steady_clock::now();

    HM_LOGI("Forward to %d ms, id=%s", targetPosition, self->m_id.c_str());
}

void VideoComponent::onSliderChangeEvent(ArkUI_NodeEvent* event) {
    void* userData = OH_ArkUI_NodeEvent_GetUserData(event);
    if (!userData) return;

    VideoComponent* self = static_cast<VideoComponent*>(userData);

    ArkUI_NodeComponentEvent* nodeEvent = OH_ArkUI_NodeEvent_GetNodeComponentEvent(event);
    if (!nodeEvent) return;

    float sliderValue = nodeEvent->data[0].f32;
    int32_t dragState = nodeEvent->data[1].i32;

    if (dragState == 0) {
        self->m_isSeeking = true;
        HM_LOGI("Seek BEGIN, value=%f", sliderValue);
    } else if (dragState == 1) {
        self->m_currentPosition = static_cast<int64_t>(sliderValue);
        A2UITextNode(self->m_currentTimeHandle).setTextContent(formatTime(self->m_currentPosition));
    } else if (dragState == 2 || dragState == 3) {
        self->m_isSeeking = false;
        int32_t seekPosition = static_cast<int32_t>(sliderValue);
        if (self->m_avPlayer) {
            OH_AVPlayer_Seek(self->m_avPlayer, seekPosition, AV_SEEK_NEXT_SYNC);
            HM_LOGI("Seek to %d ms, id=%s", seekPosition, self->m_id.c_str());
        }
        self->m_currentPosition = static_cast<int64_t>(sliderValue);
        self->updateProgressDisplay();
    }

    self->m_controlsShowTime = std::chrono::steady_clock::now();
}

} // namespace a2ui
