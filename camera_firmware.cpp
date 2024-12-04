#include <opencv2/opencv.hpp>
#include <curl/curl.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <string>
#include <vector>
#include <filesystem>
#include <ctime>
#include <chrono>
#include <nlohmann/json.hpp>


#define AUTHTOKEN_reachabilitystatus "NULL"
#define AUTHTOKEN_QOD "NULL"

#ifdef DEBUG
    #define DEBUG_MODE
#endif

namespace fs = std::filesystem;

std::string server_url = "URL";
std::string database_file = "animal_database.csv";
int capture_interval = 5;

#ifdef DEBUG_MODE
    std::string debug_folder = "your_folder";
#endif


#ifdef CAMARA

    // POST request to init a QoD session
    std::string createQoDSession(const std::string& apiUrl, const std::string& authToken, const std::string& jsonData) {
        CURL *curl = curl_easy_init();
        if(curl) {
            curl_easy_setopt(curl, CURLOPT_URL, apiUrl.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());
            
            // auth header
            std::string authHeader = "Authorization: Bearer " + authToken;
            struct curl_slist *headers = NULL;
            headers = curl_slist_append(headers, authHeader.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            std::string response;
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, [](void *ptr, size_t size, size_t nmemb, void *userdata) {
                ((std::string*)userdata)->append((char*)ptr, size * nmemb);
                return size * nmemb;
            });
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

            CURLcode res = curl_easy_perform(curl);
            if(res != CURLE_OK) {
                std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
            }

            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);
            return response;
        } else {
            return "";
        }
    }


    void closeQoDSession(const std::string& apiUrl, const std::string& authToken, const std::string& sessionId) {
        CURL *curl = curl_easy_init();
        if(curl) {
            std::string fullUrl = apiUrl + "/" + sessionId;
            curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");

            // auth header
            std::string authHeader = "Authorization: Bearer " + authToken;
            struct curl_slist *headers = NULL;
            headers = curl_slist_append(headers, authHeader.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            CURLcode res = curl_easy_perform(curl);
            if(res != CURLE_OK) {
                std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
            } else {
                std::cout << "QoD Session " << sessionId << " closed successfully." << std::endl;
            }

            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);
        }
    }
    size_t WriteCallback2(void *contents, size_t size, size_t nmemb, std::string *output) {
        size_t totalSize = size * nmemb;
        output->append(static_cast<char*>(contents), totalSize);
        return totalSize;
    }

    bool checkDeviceReachability(const std::string& url, const std::string& phoneNumber,
                                const std::string& networkAccessIdentifier, const std::string& ipv4Address,
                                const std::string& ipv6Address) {

        CURL *curl = curl_easy_init();
        if (!curl) {
            std::cerr << "cURL initialization failed!" << std::endl;
            return false;
        }

        nlohmann::json requestBody = {
            {"device", {
                {"phoneNumber", phoneNumber},
                {"networkAccessIdentifier", networkAccessIdentifier},
                {"ipv4Address", {
                    {"publicAddress", ipv4Address},
                    {"publicPort", 59765}
                }},
                {"ipv6Address", ipv6Address}
            }}
        };

        // Convert JSON object to string
        std::string jsonData = requestBody.dump();

        // Set up cURL options
        std::string response;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback2);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_slist_append(NULL, "Content-Type: application/json"));
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        std::string authHeader = "Authorization: Basic " + AUTHTOKEN_reachabilitystatus;
        headers = curl_slist_append(headers, authHeader.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "cURL request failed: " << curl_easy_strerror(res) << std::endl;
            curl_easy_cleanup(curl);
            return false;
        }

        // Parse JSON response
        try {
            auto jsonResponse = nlohmann::json::parse(response);
            std::string reachabilityStatus = jsonResponse["reachabilityStatus"];
            std::string lastStatusTime = jsonResponse["lastStatusTime"];

            std::cout << "Last Status Time: " << lastStatusTime << std::endl;
            std::cout << "Reachability Status: " << reachabilityStatus << std::endl;

            // Check reachability status
            if (reachabilityStatus == "CONNECTED_SMS" || reachabilityStatus == "CONNECTED_DATA") {
                curl_easy_cleanup(curl);
                return true;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error parsing JSON response: " << e.what() << std::endl;
        }

        curl_easy_cleanup(curl);
        return false;
    }
#endif

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    std::string* response = static_cast<std::string*>(userp);
    size_t total_size = size * nmemb;
    response->append(static_cast<char*>(contents), total_size);
    return total_size;
}

bool sendImage(const cv::Mat& frame, std::string& server_response) {
    std::vector<uchar> buf;
    cv::imencode(".jpg", frame, buf);

    CURL* curl = curl_easy_init();
    if (!curl) return false;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: multipart/form-data");

    std::string response;
    std::string filename = "image.jpg";

    // Temp. file to write the image
    FILE* tmpfile = fopen(filename.c_str(), "wb");
    if (!tmpfile) {
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        return false;
    }
    fwrite(buf.data(), 1, buf.size(), tmpfile);
    fclose(tmpfile);

    curl_mime* form = curl_mime_init(curl);
    curl_mimepart* field = curl_mime_addpart(form);

    // Add image file to the form
    curl_mime_name(field, "image");
    curl_mime_filedata(field, filename.c_str());

    curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
    curl_easy_setopt(curl, CURLOPT_URL, server_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        server_response = curl_easy_strerror(res);
        curl_mime_free(form);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        remove(filename.c_str()); // Clean up temporary file
        return false;
    }

    server_response = response;

    // Cleanup
    curl_mime_free(form);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    remove(filename.c_str()); // Clean up temporary file

    return true;
}


std::unordered_map<std::string, int> loadDatabase(const std::string& file) {
    std::unordered_map<std::string, int> database;
    std::ifstream infile(file);
    if (infile.is_open()) {
        std::string line;
        while (std::getline(infile, line)) {
            std::istringstream iss(line);
            std::string name;
            int count;
            if (std::getline(iss, name, ',') && iss >> count) {
                database[name] = count;
            }
        }
        infile.close();
    }
    return database;
}

void saveDatabase(const std::unordered_map<std::string, int>& database, const std::string& file) {
    std::ofstream outfile(file, std::ios::trunc);
    if (outfile.is_open()) {
        for (const auto& [name, count] : database) {
            outfile << name << "," << count << "\n";
        }
        outfile.close();
    }
}

#ifdef DEBUG_MODE
    std::vector<std::string> getImagesFromFolder(const std::string& folder) {
        std::vector<std::string> image_paths;
        for (const auto& entry : fs::directory_iterator(folder)) {
            if (entry.is_regular_file() && 
                (entry.path().extension() == ".jpg" || entry.path().extension() == ".png")) {
                image_paths.push_back(entry.path().string());
            }
        }
        return image_paths;
    }
#endif

int main() {
        #ifdef CAMARA
            std::string apiUrl = "https://api.orange.com/camara/orange-lab/device-reachability-status/v0/retrieve";
            std::string phoneNumber = "+123456789";
            std::string networkAccessIdentifier = "123456789@domain.com";
            std::string ipv4Address = "84.125.93.10";
            std::string ipv6Address = "2001:db8:85a3:8d3:1319:8a2e:370:7344";

            if (checkDeviceReachability(apiUrl, phoneNumber, networkAccessIdentifier, ipv4Address, ipv6Address)) {
                std::cout << "Device is reachable!" << std::endl;
            } else {
                std::cout << "Device is not reachable." << std::endl;
            // if not reachable, init QoD session

            apiUrl = "https://api.orange.com/camara/quality-on-demand/orange-lab/v0/sessions";
            std::string jsonData = R"({
                "duration": 3600,
                "device": {
                    "ipv4Address": {
                        "publicAddress": "84.125.93.10",
                        "publicPort": 59765
                    }
                },
                "applicationServer": {
                    "ipv4Address": "192.168.0.1/24"
                },
                "qosProfile": "QCI_1_voice",
                "webhook": {
                    "notificationUrl": "https://application-server.com",
                    "notificationAuthToken": "c8974e592c2fa383d4a3960714"
                }
            })";

            std::string response = createQoDSession(apiUrl, AUTHTOKEN_QOD, jsonData);

            if (!response.empty()) {
                std::cout << "QoD Session Creation Response: " << response << std::endl;
            }
        }
    #endif

    std::unordered_map<std::string, int> database = loadDatabase(database_file);

#ifdef DEBUG_MODE
    // Debug mode: Send images from defined folder
    std::vector<std::string> images = getImagesFromFolder(debug_folder);
    for (const auto& image_path : images) {
        cv::Mat frame = cv::imread(image_path);
        if (frame.empty()) {
            std::cerr << "Error: Could not load image: " << image_path << "\n";
            continue;
        }

        std::string server_response;
        if (sendImage(frame, server_response)) {
            // increment the counter for the animal
            database[server_response]++;

            saveDatabase(database, database_file);
            std::cout << "Sighted: " << server_response 
                      << ", Total: " << database[server_response] << "\n";
        } else {
            std::cerr << "Error: Failed to send image to the server.\n";
        }

        cv::waitKey(capture_interval * 1000);
    }
#else
    // Normal mode: Capture images from the camera
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open camera.\n";
        return -1;
    }

    while (true) {
        cv::Mat frame;
        cap >> frame;
        if (frame.empty()) {
            std::cerr << "Error: Could not capture frame.\n";
            break;
        }

        // Display the frame to the user
        cv::imshow("Camera Feed", frame);

        // Send frame to server
        std::string server_response;
        if (sendImage(frame, server_response)) {
            // Increment the counter for the animal
            database[server_response]++;

            saveDatabase(database, database_file);

            std::cout << "Sighted: " << server_response 
                      << ", Total: " << database[server_response] << "\n";
        } else {
            std::cerr << "Error: Failed to send image to the server.\n";
        }

        // Wait and check if the user presses 'q' to quit
        if (cv::waitKey(capture_interval * 1000) == 'q') {
            break;
        }
    }
    #ifdef CAMARA
        closeQoDSession(apiUrl, authToken, sessionId); // close session when closed, so you don't overbill customer
    #endif
    cv::destroyAllWindows(); // close the camera feed window
#endif

    return 0;
}
