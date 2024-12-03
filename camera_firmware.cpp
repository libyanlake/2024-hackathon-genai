#include <opencv2/opencv.hpp>
#include <curl/curl.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <string>
#include <vector>
#include <filesystem>

#ifdef DEBUG
    #define DEBUG_MODE
#endif

namespace fs = std::filesystem;

std::string server_url = "http://yourserver.com/identify"; // Replace with EfficientNet endpoint
std::string database_file = "animal_database.txt";
int capture_interval = 5; // Interval in seconds

#ifdef DEBUG_MODE
std::string debug_folder = "images/"; // Folder with images for debug mode
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
    headers = curl_slist_append(headers, "Content-Type: image/jpeg");

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, server_url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buf.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, buf.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        return false;
    }

    server_response = response;

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
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
    std::unordered_map<std::string, int> database = loadDatabase(database_file);

    #ifdef DEBUG_MODE
        // Debug mode: Send images from the folder
        std::vector<std::string> images = getImagesFromFolder(debug_folder);
        for (const auto& image_path : images) {
            cv::Mat frame = cv::imread(image_path);
            if (frame.empty()) {
                std::cerr << "Error: Could not load image: " << image_path << "\n";
                continue;
            }

            std::string server_response;
            if (sendImage(frame, server_response)) {
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

        std::string server_response;
        if (sendImage(frame, server_response)) {
            database[server_response]++;
            saveDatabase(database, database_file);

            std::cout << "Sighted: " << server_response 
                      << ", Total: " << database[server_response] << "\n";
        } else {
            std::cerr << "Error: Failed to send image to the server.\n";
        }

        cv::waitKey(capture_interval * 1000);
    }
#endif

    return 0;
}
