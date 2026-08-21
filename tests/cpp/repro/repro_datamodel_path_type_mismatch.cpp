// Reproducer: RISK46/RISK47 — updateDataModel/appendDataModel path 字段
// 类型不匹配导致 nlohmann::json::type_error → SIGABRT.
//
// 场景:
//   发送 updateDataModel 协议，其中 path 字段为 number（应为 string）。
//   parseUpdateDataModelData 在 parser 层成功（surfaceId 是有效 string），
//   但 Surface::updateDataModel() 中直接 dataModelData["path"].get<std::string>()
//   无 is_string() 检查 → 抛出 type_error → 无 catch → SIGABRT.
//
// 预期结果（未修复代码）:
//   libc++abi: terminating due to uncaught exception of type
//     nlohmann::detail::type_error:
//     [json.exception.type_error.302] type must be string, but is number
//   进程以 SIGABRT (exit 134) 终止.
//
// 预期结果（已修复代码）:
//   打印 "repro_datamodel_path_type_mismatch: PASS" 并以 exit 0 退出.

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

#include "agenui_engine.h"
#include "agenui_engine_entry.h"
#include "agenui_surface_manager_interface.h"

// 辅助函数：发送 A2UI 协议数据并等待工作线程处理完毕。
static void sendAndSync(agenui::ISurfaceManager* sm, const std::string& data) {
    sm->beginTextStream();
    sm->receiveTextChunk(data);
    sm->endTextStream();
    // 等待工作线程处理完毕（同步用 dummy surface）
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}

int main() {
    agenui::IAGenUIEngine* engine = agenui::initAGenUIEngine();
    if (!engine) {
        std::fprintf(stderr, "initAGenUIEngine failed\n");
        return 2;
    }

    agenui::ISurfaceManager* sm = engine->createSurfaceManager();
    if (!sm) {
        std::fprintf(stderr, "createSurfaceManager failed\n");
        agenui::destroyAGenUIEngine();
        return 2;
    }

    // 等待 SurfaceManager 初始化完成（工作线程 init）
    sm->beginTextStream();
    sm->endTextStream();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 第一步：创建 surface（有效的 createSurface 协议）
    sendAndSync(sm,
        R"({"version":"v0.9",)"
        R"("createSurface":{"surfaceId":"risk46_surface","catalogId":"test",)"
        R"("theme":{},"sendDataModel":false,"animated":true}})");

    // 第二步：发送 updateDataModel，path 字段为 number（非法类型）
    //
    // parseUpdateDataModelData 阶段：
    // - JSON 解析成功
    // - surfaceId 为有效 string → 解析通过
    // - 将 dataModelJsonNode（含 path=12345）传入 Surface::updateDataModel
    //
    // Surface::updateDataModel() 中：
    //   dataModelData.contains("path") → true
    //   dataModelData["path"].get<std::string>() → type_error（crash）
    sm->beginTextStream();
    sm->receiveTextChunk(
        R"({"updateDataModel":{)"
        R"("surfaceId":"risk46_surface",)"
        R"("path":12345,)"            // ← 非字符串！本应触发 SIGABRT
        R"("value":"risk46_value"})"
        R"(})");
    sm->endTextStream();

    // 如果执行到这里，说明 type_error 被捕获或者没有触发崩溃。
    // 这可能是由于：
    // 1. RISK46 已修复（path 字段加了 is_string 检查）
    // 2. 异常在传播路径上被某些中间层捕获
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::printf("repro_datamodel_path_type_mismatch: PASS (path type mismatch did not crash)\n");

    engine->destroySurfaceManager(sm);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    agenui::destroyAGenUIEngine();
    return 0;
}