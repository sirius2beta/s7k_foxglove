#include "foxglovewrapper.h"

#include <foxglove/foxglove.hpp>
#include <foxglove/server.hpp>
#include <foxglove/messages.hpp>

#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>
struct FoxgloveWrapper::Impl {
    // 假設這是 Foxglove 啟動後的結果型別
    // 我們通常會儲存「解開後」的 server 本體，或者儲存整個 Result
    std::unique_ptr<foxglove::RawChannel> channel;
    std::unique_ptr<foxglove::messages::PointCloudChannel> pointcloudChannel;
};

FoxgloveWrapper::FoxgloveWrapper() : pImpl(std::make_unique<Impl>()) {
    // 建構子暫時不初始化 server，等 startServer 再做

}

FoxgloveWrapper::~FoxgloveWrapper()
{
    if (workerThread.joinable()) {
        workerThread.join(); // 等待執行緒安全結束
    }
}


void FoxgloveWrapper::worker()
{
    foxglove::WebSocketServerOptions options;
    auto serverResult = foxglove::WebSocketServer::create(std::move(options));
    if (!serverResult.has_value()) {
        std::cerr << foxglove::strerror(serverResult.error()) << '\n';
        return;
    }

    auto server = std::move(serverResult.value());
    auto result = foxglove::RawChannel::create("/hello", "json");
    auto result2 = foxglove::messages::PointCloudChannel::create("/pointcloud");
    std::cout<<"foxglove wrapper started";
    if (result) {
        // 使用 std::move 將建立好的 Channel 轉移給 smart pointer
        pImpl->channel = std::make_unique<foxglove::RawChannel>(std::move(result.value()));
    } else {
        // 處理錯誤，例如：printf("Failed to create channel: %s\n", result.error().c_str());
    }
    if (result2) {
        // 使用 std::move 將建立好的 Channel 轉移給 smart pointer
        pImpl->pointcloudChannel = std::make_unique<foxglove::messages::PointCloudChannel>(std::move(result2.value()));
    } else {
        // 處理錯誤，例如：printf("Failed to create channel: %s\n", result.error().c_str());
    }
    while (true) {
        // 處理你的聲納資料、Foxglove 通訊...
        sendExamplePointCloud();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}


void FoxgloveWrapper::start_worker()
{
    workerThread = std::thread(&FoxgloveWrapper::worker, this);
}

void FoxgloveWrapper::sendExamplePointCloud() {

    // 1. 建立 PointCloud 實例
    foxglove::messages::PointCloud pc;
    pc.frame_id = "sensor_frame";
    foxglove::messages::Timestamp ts;
    ts.sec = 1000;
    pc.timestamp = ts; // 填入當前時間戳
    // 2. 定義資料結構 (Fields)
    // 假設我們只傳 XYZ (每個點 12 bytes)
    pc.fields = {
        { "x", 0, foxglove::messages::PackedElementField::NumericType::FLOAT32 },
        { "y", 4, foxglove::messages::PackedElementField::NumericType::FLOAT32 },
        { "z", 8, foxglove::messages::PackedElementField::NumericType::FLOAT32 },
        { "intensities", 12, foxglove::messages::PackedElementField::NumericType::FLOAT32 }
    };
    pc.point_stride = 16;



    pc.data.resize(_points.size() * sizeof(float));
    std::memcpy(pc.data.data(), _points.data(), pc.data.size());
    // 在 sendExamplePointCloud 內：
    auto now = std::chrono::system_clock::now().time_since_epoch();
    uint64_t ns = 1000;

    pImpl->pointcloudChannel->log(pc);
}
