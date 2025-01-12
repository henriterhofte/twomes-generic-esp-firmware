#include <util/http_util.hpp>

#include <util/delay.hpp>

constexpr int HTTPS_CONNECTION_RETRIES = 10;
constexpr int HTTPS_RETRY_WAIT_MS = 1 * 1000; // 1 second.

namespace HTTPUtil
{
    namespace
    {
        const char *TAG = "HTTPUtil";

        headers_t bufferedHeaders;

        esp_err_t HTTPEventHandler(esp_http_client_event_t *evt)
        {
            switch (evt->event_id)
            {
            case HTTP_EVENT_ERROR:
                ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
                break;
            case HTTP_EVENT_ON_CONNECTED:
                ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
                break;
            case HTTP_EVENT_HEADER_SENT:
                ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
                break;
            case HTTP_EVENT_ON_HEADER:
                ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
                bufferedHeaders[evt->header_key] = evt->header_value;
                break;
            case HTTP_EVENT_ON_DATA:
                ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
                break;
            case HTTP_EVENT_ON_FINISH:
                ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
                break;
            case HTTP_EVENT_DISCONNECTED:
                ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
                break;
            }
            return ESP_OK;
        }

        int Cleanup(esp_http_client_handle_t &client)
        {
            esp_http_client_cleanup(client);
            return 400;
        }
    }

    int HTTPRequest(esp_http_client_config_t config, headers_t &headersReceive, buffer_t &dataReceive, headers_t headersSend, buffer_t dataSend)
    {
        esp_err_t err = ESP_OK;

        // Make sure there are no headers buffered.
        bufferedHeaders.clear();

        // Attach HTTPEventHandler. Only used to capture received headers.
        config.event_handler = HTTPEventHandler;

        auto client = esp_http_client_init(&config);

        for (auto const &header : headersSend)
        {
            err = esp_http_client_set_header(client, header.first.c_str(), header.second.c_str());
            Error::CheckAppendName(err, TAG, "An error occured when setting header");
        }

        for (int tries = 1; tries <= HTTPS_CONNECTION_RETRIES; tries++)
        {
            err = esp_http_client_open(client, dataSend.size());
            if (err == ESP_OK)
                break;

            ESP_LOGE(TAG,
                     "Failed to open HTTP(S) connection %s (%d/%d) at %s",
                     esp_err_to_name(err),
                     tries,
                     HTTPS_CONNECTION_RETRIES,
                     esp_log_system_timestamp());

            vTaskDelay(Delay::MilliSeconds(HTTPS_RETRY_WAIT_MS));
        }

        if (err != ESP_OK)
        {
            // We were not able to connect after HTTPS_CONNECTION_RETRIES.
            // Reboot to avoid worse.
            esp_restart();
        }

        if (dataSend.size() > 0)
        {
            auto bytesSent = esp_http_client_write(client, dataSend.data(), dataSend.size());
            if (bytesSent < dataSend.size())
            {
                ESP_LOGE(TAG, "Not all data was written.");
                return Cleanup(client);
            }
        }

        auto contentLength = esp_http_client_fetch_headers(client);
        if (contentLength < 0)
        {
            ESP_LOGE(TAG, "An error occured when fetching headers: %s", esp_err_to_name(err));
            return Cleanup(client);
        }

        // Reserve memory for response + null-termination char.
        dataReceive.resize(contentLength + 1);

        auto bytesReceived = esp_http_client_read_response(client, &dataReceive[0], contentLength);

        // Append null-termination char.
        // dataReceive.emplace_back('\0');

        if (bytesReceived != contentLength)
        {
            ESP_LOGE(TAG, "An error occured when reading response. Expected %d but received %d", contentLength, bytesReceived);
            return Cleanup(client);
        }

        // copy buffered headers from HTTPEventHandler.
        headersReceive = bufferedHeaders;

        auto statusCode = esp_http_client_get_status_code(client);

        err = esp_http_client_cleanup(client);
        Error::CheckAppendName(err, TAG, "An error occured when cleaning up HTTP client");

        return statusCode;
    }

    int HTTPRequest(esp_http_client_config_t config, headers_t &headersReceive, buffer_t &dataReceive, headers_t headersSend)
    {
        buffer_t data;

        return HTTPRequest(config, headersReceive, dataReceive, headersSend, data);
    }

    int HTTPRequest(esp_http_client_config_t config, headers_t &headersReceive, buffer_t &dataReceive)
    {
        buffer_t data;
        headers_t headers;

        return HTTPRequest(config, headersReceive, dataReceive, headers, data);
    }
}
