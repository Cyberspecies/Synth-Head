/*****************************************************************
 * @file DnsServer.hpp
 * @brief Captive Portal DNS Server
 * 
 * Implements a DNS server that redirects all queries to the
 * portal IP, enabling captive portal functionality.
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "SystemAPI/Web/WebTypes.hpp"

#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "esp_log.h"

namespace SystemAPI {
namespace Web {

static const char* DNS_TAG = "DnsServer";

/**
 * @brief DNS Server for Captive Portal
 * 
 * Redirects all DNS queries to the portal IP address,
 * enabling captive portal detection on mobile devices.
 */
class DnsServer {
public:
    /**
     * @brief Get singleton instance
     */
    static DnsServer& instance() {
        static DnsServer inst;
        return inst;
    }
    
    /**
     * @brief Start the DNS server
     * @return true on success
     */
    bool start() {
        if (running_) return true;
        
        // Create task
        BaseType_t result = xTaskCreate(
            &DnsServer::serverTask,
            "dns_server",
            4096,
            this,
            5,
            &task_handle_
        );
        
        if (result != pdPASS) {
            ESP_LOGE(DNS_TAG, "Failed to create DNS server task");
            return false;
        }
        
        running_ = true;
        ESP_LOGI(DNS_TAG, "DNS server started on port %d", DNS_PORT);
        return true;
    }
    
    /**
     * @brief Stop the DNS server
     */
    void stop() {
        if (!running_) return;
        
        running_ = false;
        
        if (task_handle_) {
            vTaskDelete(task_handle_);
            task_handle_ = nullptr;
        }
        
        if (socket_ >= 0) {
            close(socket_);
            socket_ = -1;
        }
        
        ESP_LOGI(DNS_TAG, "DNS server stopped");
    }
    
    /**
     * @brief Check if server is running
     */
    bool isRunning() const { return running_; }

private:
    DnsServer() = default;
    ~DnsServer() { stop(); }
    
    // Prevent copying
    DnsServer(const DnsServer&) = delete;
    DnsServer& operator=(const DnsServer&) = delete;
    
    /**
     * @brief Main server task
     */
    static void serverTask(void* param) {
        DnsServer* self = static_cast<DnsServer*>(param);
        self->runServer();
    }
    
    /**
     * @brief Server main loop
     */
    void runServer() {
        socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket_ < 0) {
            ESP_LOGE(DNS_TAG, "Failed to create DNS socket");
            running_ = false;
            vTaskDelete(nullptr);
            return;
        }
        
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_addr.sin_port = htons(DNS_PORT);
        
        if (bind(socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            ESP_LOGE(DNS_TAG, "Failed to bind DNS socket");
            close(socket_);
            socket_ = -1;
            running_ = false;
            vTaskDelete(nullptr);
            return;
        }
        
        uint8_t buffer[DNS_BUFFER_SIZE];
        struct sockaddr_in client_addr;
        socklen_t client_len;
        
        while (running_) {
            client_len = sizeof(client_addr);
            int len = recvfrom(socket_, buffer, sizeof(buffer), 0, 
                              (struct sockaddr*)&client_addr, &client_len);
            
            if (len > 12) {
                // Log the queried domain
                logDnsQuery(buffer, len);
                
                // Build and send response
                uint8_t response[DNS_BUFFER_SIZE];
                int resp_len = buildDnsResponse(buffer, len, response, sizeof(response));
                
                if (resp_len > 0) {
                    sendto(socket_, response, resp_len, 0, 
                          (struct sockaddr*)&client_addr, client_len);
                }
            }
            
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        
        vTaskDelete(nullptr);
    }
    
    /**
     * @brief Extract and log the domain name from DNS query
     */
    void logDnsQuery(uint8_t* buffer, int len) {
        char domain[128] = {0};
        int pos = 12;
        int domain_pos = 0;
        
        while (pos < len && buffer[pos] != 0 && domain_pos < 126) {
            int label_len = buffer[pos++];
            if (domain_pos > 0) domain[domain_pos++] = '.';
            for (int i = 0; i < label_len && pos < len && domain_pos < 126; i++) {
                domain[domain_pos++] = buffer[pos++];
            }
        }
        
        ESP_LOGI(DNS_TAG, "DNS query: %s -> %s", domain, PORTAL_IP);
    }
    
    /**
     * @brief Build DNS response redirecting to portal IP
     */
    int buildDnsResponse(uint8_t* query, int query_len, uint8_t* response, int max_len) {
        if (query_len < 12) return 0;
        
        // Copy query header
        memcpy(response, query, query_len);
        
        // Set response flags
        response[2] = 0x81;  // QR=1, Opcode=0, AA=0, TC=0, RD=1
        response[3] = 0x80;  // RA=1, Z=0, RCODE=0
        
        // Set answer count = 1
        response[6] = 0x00;
        response[7] = 0x01;
        
        // Find end of question section
        int qname_end = 12;
        while (qname_end < query_len && query[qname_end] != 0) {
            qname_end += query[qname_end] + 1;
        }
        qname_end++;  // Skip null terminator
        qname_end += 4;  // Skip QTYPE and QCLASS
        
        int pos = qname_end;
        
        // Add answer section
        // Name pointer to question
        response[pos++] = 0xC0;
        response[pos++] = 0x0C;
        
        // Type A (1)
        response[pos++] = 0x00;
        response[pos++] = 0x01;
        
        // Class IN (1)
        response[pos++] = 0x00;
        response[pos++] = 0x01;
        
        // TTL (60 seconds)
        response[pos++] = 0x00;
        response[pos++] = 0x00;
        response[pos++] = 0x00;
        response[pos++] = 0x3C;
        
        // RDLENGTH (4 bytes for IP)
        response[pos++] = 0x00;
        response[pos++] = 0x04;
        
        // RDATA (portal IP)
        response[pos++] = PORTAL_IP_BYTES[0];
        response[pos++] = PORTAL_IP_BYTES[1];
        response[pos++] = PORTAL_IP_BYTES[2];
        response[pos++] = PORTAL_IP_BYTES[3];
        
        return pos;
    }
    
    // State
    bool running_ = false;
    int socket_ = -1;
    TaskHandle_t task_handle_ = nullptr;
};

// Convenience macro
#define DNS_SERVER DnsServer::instance()

} // namespace Web
} // namespace SystemAPI
