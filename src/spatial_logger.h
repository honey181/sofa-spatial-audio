#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <cstdint>

class SpatialLogger {
public:
    static SpatialLogger& Get();

    void Init(const std::string& logFilePath = "C:\\ProgramData\\SofaSpatialAudio\\spatial_debug.log");
    void Log(const std::string& message);
    void LogFormat(const char* fmt, ...);

    // Rate-limited telemetry for real-time object coordinates
    void LogObjectActivity(uint32_t activeCount, float posX, float posY, float posZ, float gain);

private:
    SpatialLogger();
    ~SpatialLogger();

    std::mutex m_mutex;
    std::ofstream m_file;
    std::string m_path;
    uint32_t m_lastLogTimeMs = 0;
    bool m_initialized = false;
};
