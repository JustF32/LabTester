#include "test_common.h"
#include "services/ExecutionSettingsService.h"
#include "database/DatabaseManager.h"

using namespace labtester::services;
using namespace labtester::database;
using namespace labtester::test;

class ExecutionSettingsServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_testDb = std::make_unique<TestDatabase>();
        m_service = std::make_unique<ExecutionSettingsService>(*m_testDb);
    }
    
    std::unique_ptr<TestDatabase> m_testDb;
    std::unique_ptr<ExecutionSettingsService> m_service;
};

TEST_F(ExecutionSettingsServiceTest, DefaultExecutionMode) {
    // По умолчанию должен быть server режим
    QString mode = m_service->executionMode();
    EXPECT_EQ(mode, "server");
}

TEST_F(ExecutionSettingsServiceTest, SetExecutionMode) {
    bool result = m_service->setExecutionMode("local", nullptr);
    ASSERT_TRUE(result);
    EXPECT_EQ(m_service->executionMode(), "local");
    
    result = m_service->setExecutionMode("hybrid", nullptr);
    ASSERT_TRUE(result);
    EXPECT_EQ(m_service->executionMode(), "hybrid");
}

TEST_F(ExecutionSettingsServiceTest, SetInvalidExecutionModeFallsBackToLocal) {
    bool result = m_service->setExecutionMode("invalid_mode", nullptr);
    ASSERT_TRUE(result);
    EXPECT_EQ(m_service->executionMode(), "local");
}

TEST_F(ExecutionSettingsServiceTest, RemoteEndpointDefaultEmpty) {
    EXPECT_TRUE(m_service->remoteEndpoint().isEmpty());
}

TEST_F(ExecutionSettingsServiceTest, SetRemoteEndpoint) {
    const QString endpoint = "http://localhost:20000";
    bool result = m_service->setRemoteEndpoint(endpoint, nullptr);
    ASSERT_TRUE(result);
    EXPECT_EQ(m_service->remoteEndpoint(), endpoint);
}

TEST_F(ExecutionSettingsServiceTest, SetRemoteEndpointNormalizesUrl) {
    bool result = m_service->setRemoteEndpoint("localhost:20000/", nullptr);
    ASSERT_TRUE(result);
    EXPECT_EQ(m_service->remoteEndpoint(), "http://localhost:20000");
}

TEST_F(ExecutionSettingsServiceTest, RemoteTokenDefaultEmpty) {
    EXPECT_TRUE(m_service->remoteToken().isEmpty());
}

TEST_F(ExecutionSettingsServiceTest, SetRemoteToken) {
    const QString token = "secret123";
    bool result = m_service->setRemoteToken(token, nullptr);
    ASSERT_TRUE(result);
    EXPECT_EQ(m_service->remoteToken(), token);
}

TEST_F(ExecutionSettingsServiceTest, RemoteNgrokModeDefaultFalse) {
    EXPECT_FALSE(m_service->remoteNgrokMode());
}

TEST_F(ExecutionSettingsServiceTest, SetRemoteNgrokMode) {
    bool result = m_service->setRemoteNgrokMode(true, nullptr);
    ASSERT_TRUE(result);
    EXPECT_TRUE(m_service->remoteNgrokMode());
    
    result = m_service->setRemoteNgrokMode(false, nullptr);
    ASSERT_TRUE(result);
    EXPECT_FALSE(m_service->remoteNgrokMode());
}

TEST_F(ExecutionSettingsServiceTest, RemoteConfigReturnsAllSettings) {
    m_service->setRemoteEndpoint("http://test:20000", nullptr);
    m_service->setRemoteToken("abc123", nullptr);
    m_service->setRemoteNgrokMode(true, nullptr);
    
    auto config = m_service->remoteConfig();
    EXPECT_EQ(config.endpoint, "http://test:20000");
    EXPECT_EQ(config.token, "abc123");
    EXPECT_TRUE(config.ngrokMode);
}

TEST_F(ExecutionSettingsServiceTest, SettingsPersistAcrossServiceInstances) {
    // Устанавливаем значения
    m_service->setExecutionMode("hybrid", nullptr);
    m_service->setRemoteEndpoint("http://test:20000", nullptr);
    m_service->setRemoteToken("abc123", nullptr);
    
    // Создаем новый сервис с той же БД
    auto newService = std::make_unique<ExecutionSettingsService>(*m_testDb);
    
    EXPECT_EQ(newService->executionMode(), "hybrid");
    EXPECT_EQ(newService->remoteEndpoint(), "http://test:20000");
    EXPECT_EQ(newService->remoteToken(), "abc123");
}