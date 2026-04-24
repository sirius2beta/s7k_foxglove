#ifndef FOXGLOVEWRAPPER_H
#define FOXGLOVEWRAPPER_H
#include <string>
#include <memory>
#include <thread>
#include <vector>
#include "7k.h"

namespace foxglove {
    class Server;
    using ChannelId = uint32_t; // ChannelId 通常是整數型別
}

class FoxgloveWrapper
{
public:
    FoxgloveWrapper();
    ~FoxgloveWrapper();
    void startServer(int port);
    //void sendData(const std::string& topic, const std::string& jsonPayload);
    void sendExamplePointCloud();
    void worker();
    void start_worker();
    void setPointCloud(std::vector<float> points){_points= points;}
private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
    std::thread workerThread;
    R7027 _r7027;
    std::vector<float> _points;
};

#endif // FOXGLOVEWRAPPER_H
