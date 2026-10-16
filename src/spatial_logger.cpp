#include "spatial_logger.h"
#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <chrono>
#include <iomanip>
#include <sstream>

SpatialLogger& SpatialLogger::Get() {
    static SpatialLogger instance;
    return instance;
}

SpatialLogger::SpatialLogger() {
    Init();
}

SpatialLogger::~SpatialLogger() {
    if (m_file.is_open()) {
        m_file.flush();
        m_file.close();
    }
}

void SpatialLogger::Init(const std::string& logFilePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_path = logFilePath;
    m_file.open(m_path, std::ios::out | std::ios::app);
    m_initialized = m_file.is_open();

    if (m_initialized) {
        m_file << "\n=======================================================\n";
        m_file << "[SOFA-SPATIAL] Session Started | PID: " << GetCurrentProcessId() << "\n";
        m_file << "=======================================================\n";
        m_file.flush();
    }
}

void SpatialLogger::Log(const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Timestamp
    SYSTEMTIME st;
    GetLocalTime(&st);
    char timeBuf[64];
    snprintf(timeBuf, sizeof(timeBuf), "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    std::string fullMsg = std::string(timeBuf) + message + "\n";

    if (m_file.is_open()) {
        m_file << fullMsg;
        m_file.flush();
    }
    OutputDebugStringA(fullMsg.c_str());
}

void SpatialLogger::LogFormat(const char* fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    Log(std::string(buffer));
}

void SpatialLogger::LogObjectActivity(uint32_t activeCount, float posX, float posY, float posZ, float gain) {
    uint32_t now = GetTickCount();
    if (now - m_lastLogTimeMs < 1000) { // Rate limit: at most 1 log per second during continuous playback
        return;
    }
    m_lastLogTimeMs = now;

    LogFormat("[STREAM TELEMETRY] Active Dynamic Objects: %u | Primary Obj: (X=%.2f, Y=%.2f, Z=%.2f) Gain=%.2f",
              activeCount, posX, posY, posZ, gain);
}
