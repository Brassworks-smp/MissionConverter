#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <regex>
#include <chrono>
#include <iomanip> // For std::setprecision

// --- Header-Only Dependencies ---
// Make sure these files are in the same directory or in your include path
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include "json.hpp"
#include "csv.h"

// Use nlohmann/json library
using json = nlohmann::json;

// --- Constants ---
const std::string GOOGLE_SHEET_URL_TO_PARSE = "https://docs.google.com/spreadsheets/d/1g_Fn5qVjEgfV0PsRR6tH91PJeQkH-wexDphUM6nU804/edit?usp=sharing";
const std::string ITEM_LIST_PATH = "itemlist_dump.txt";
const std::string OUTPUT_JSON_PATH = "missions.json";

// CATEGORY_MAP equivalent using nlohmann::json for easy structure matching
const json CATEGORY_MAP = {
    {"Small", {
        {"weight", 10.0},
        {"reward", {{"minAmount", 2}, {"maxAmount", 6}}}
    }},
    {"Medium", {
        {"weight", 8.0},
        {"reward", {{"minAmount", 4}, {"maxAmount", 8}}}
    }},
    {"Large", {
        {"weight", 6.0},
        {"reward", {{"minAmount", 6}, {"maxAmount", 9}}}
    }},
    {"Extremely Rare", {
        {"weight", 1.0},
        {"reward", {{"minAmount", 10}, {"maxAmount", 15}}}
    }}
};

// --- Helper Functions ---

// Trim whitespace from start (left)
static inline std::string& ltrim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    return s;
}

// Trim whitespace from end (right)
static inline std::string& rtrim(std::string& s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
    return s;
}

// Trim from both ends
static inline std::string& trim(std::string& s) {
    return ltrim(rtrim(s));
}

// Splits a string by delimiter and trims each part
std::vector<std::string> split_and_trim(const std::string& str, char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        if (!trim(token).empty()) {
            result.push_back(token);
        }
    }
    return result;
}


// --- Core Logic ---

// Loads valid item IDs from the specified file into a set.
std::set<std::string> load_valid_items(const std::string& filepath) {
    std::cout << "Loading valid items from '" << filepath << "'..." << std::endl;
    std::set<std::string> valid_items;
    std::ifstream file(filepath);

    if (!file.is_open()) {
        std::cerr << "ERROR: Item list file not found at '" << filepath << "'." << std::endl;
        std::cerr << "Please create 'itemlist_dump.txt' in the same directory." << std::endl;
        return valid_items;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (!trim(line).empty()) {
            valid_items.insert(line);
        }
    }

    std::cout << "Loaded " << valid_items.size() << " valid item IDs." << std::endl;
    return valid_items;
}

// Downloads and processes the Google Sheet CSV.
std::pair<json, std::vector<std::string>> process_csv(const std::string& url, const std::set<std::string>& valid_items) {
    std::cout << "Processing Google Sheet from URL..." << std::endl;
    json missions = json::array();
    std::vector<std::string> errors;

    // 1. Parse Google Sheet URL
    std::smatch sheet_id_match;
    std::regex sheet_id_regex("spreadsheets/d/([a-zA-Z0-9_-]+)");
    if (!std::regex_search(url, sheet_id_match, sheet_id_regex) || sheet_id_match.size() < 2) {
        errors.push_back("Invalid Google Sheets URL format. Must contain '/d/SHEET_ID/'.");
        errors.push_back("URL provided: " + url);
        return {missions, errors};
    }
    std::string sheet_id = sheet_id_match[1];

    std::smatch gid_match;
    std::regex gid_regex("gid=([0-9]+)");
    std::string gid = "0";
    if (std::regex_search(url, gid_match, gid_regex) && gid_match.size() >= 2) {
        gid = gid_match[1];
    } else {
        std::cout << "No 'gid' found in URL, defaulting to first sheet (gid=0)." << std::endl;
    }

    std::string host = "https://docs.google.com";
    std::string path = "/spreadsheets/d/" + sheet_id + "/export?format=csv&gid=" + gid;

    std::cout << "Downloading CSV from: " << host << path << std::endl;

    // 2. Download CSV content
    httplib::Client cli(host);

    // FIX: The function is set_follow_location, not set_follow_redirect
    cli.set_follow_location(true);

    auto res = cli.Get(path.c_str());

    if (!res || res->status != 200) {
        errors.push_back("Failed to download Google Sheet. Status: " + (res ? std::to_string(res->status) : "Error") + ". Check URL and permissions.");
        return {missions, errors};
    }

    std::cout << "Google Sheet downloaded successfully." << std::endl;

    // 3. Process CSV content
    // std::stringstream content_stream(res->body); // This was incorrect

    try {
        // FIX: The csv::parse function takes the raw string content (or string_view)
        // and returns an iterable CSVReader object.
        std::string csv_content = res->body;
        auto parser = csv::parse(csv_content);

        int row_num = 1; // Start at 1 for header
        bool is_header = true;

        for (auto& row : parser) {
            row_num++;
            if (is_header) {
                is_header = false; // Skip header row
                continue;
            }

            if (row.empty()) {
                continue;
            }

            if (row.size() != 7) {
                errors.push_back("Row " + std::to_string(row_num) + ": Invalid row format. Expected 7 columns, got " + std::to_string(row.size()) + ".");
                continue;
            }

            std::string mission_id = row[0].get<std::string>();
            std::string names_str = row[1].get<std::string>();
            std::string category = row[2].get<std::string>();
            std::string mission_type = row[3].get<std::string>();
            std::string items_str = row[4].get<std::string>();
            std::string min_req_str = row[5].get<std::string>();
            std::string max_req_str = row[6].get<std::string>();

            bool row_has_error = false;

            if (CATEGORY_MAP.find(category) == CATEGORY_MAP.end()) {
                errors.push_back("Row " + std::to_string(row_num) + " (Mission: " + mission_id + "): Invalid category '" + category + "'.");
                row_has_error = true;
            }

            std::vector<std::string> items_list = split_and_trim(items_str, ',');
            if (items_list.empty()) {
                errors.push_back("Row " + std::to_string(row_num) + " (Mission: " + mission_id + "): 'Items/Blocks/Entities' column is empty.");
                row_has_error = true;
            }

            for (const auto& item : items_list) {
                if (valid_items.find(item) == valid_items.end()) {
                    errors.push_back("Row " + std::to_string(row_num) + " (Mission: " + mission_id + "): Invalid item ID '" + item + "' (not found in " + ITEM_LIST_PATH + ").");
                    row_has_error = true;
                }
            }

            int min_amount_val = 0;
            int max_amount_val = 0;
            try {
                min_amount_val = std::stoi(min_req_str);
                max_amount_val = std::stoi(max_req_str);
            } catch (const std::exception& e) {
                errors.push_back("Row " + std::to_string(row_num) + " (Mission: " + mission_id + "): Invalid min/max amount ('" + min_req_str + "', '" + max_req_str + "'). Must be integers.");
                row_has_error = true;
            }

            if (row_has_error) {
                continue;
            }

            // 4. Build JSON object
            json category_data = CATEGORY_MAP[category];
            std::vector<std::string> titles_list = split_and_trim(names_str, ',');

            json mission = {
                {"id", mission_id},
                {"weight", category_data["weight"]},
                {"titles", titles_list},
                {"requirement", {
                    {"requirementType", mission_type},
                    {"item", items_list},
                    {"minAmount", min_amount_val},
                    {"maxAmount", max_amount_val}
                }},
                {"reward", category_data["reward"]}
            };
            missions.push_back(mission);

        }
    } catch (const std::exception& e) {
        errors.push_back("An unexpected error occurred during CSV parsing: " + std::string(e.what()));
    }

    return {missions, errors};
}

// Writes the JSON data to the output file.
void write_json(const std::string& filepath, const json& data) {
    std::cout << "Writing JSON to '" << filepath << "'..." << std::endl;
    try {
        std::ofstream file(filepath);
        file << data.dump(2); // .dump(2) for 2-space indentation
        file.close();
        std::cout << "JSON file written successfully." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Failed to write JSON file: " << e.what() << std::endl;
    }
}

// --- Main Execution ---

int main() {
    auto start_time = std::chrono::high_resolution_clock::now();
    std::cout << "--- Starting Mission Conversion Script ---" << std::endl;

    std::set<std::string> valid_items = load_valid_items(ITEM_LIST_PATH);

    if (valid_items.empty() && !ITEM_LIST_PATH.empty()) {
        std::cerr << "Cannot proceed without a valid item list. Halting." << std::endl;
        return 1;
    }

    auto [missions, errors] = process_csv(GOOGLE_SHEET_URL_TO_PARSE, valid_items);

    if (!errors.empty()) {
        std::cerr << "\n--- Validation Failed ---" << std::endl;
        std::cerr << errors.size() << " error(s) found. '" << OUTPUT_JSON_PATH << "' was NOT generated." << std::endl;
        std::cerr << "Please fix these issues in your Google Sheet or item list and try again:\n" << std::endl;
        for (const auto& error : errors) {
            std::cerr << "- " << error << std::endl;
        }
    } else if (missions.empty()) {
        std::cout << "\n--- No Missions Processed ---" << std::endl;
        std::cout << "No missions were successfully processed. No output file generated." << std::endl;
    } else {
        write_json(OUTPUT_JSON_PATH, missions);
        std::cout << "\n--- Success ---" << std::endl;
        std::cout << "Successfully processed " << missions.size() << " missions." << std::endl;
        std::cout << "Output file created: '" << OUTPUT_JSON_PATH << "'" << std::endl;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "\nTotal script time: " << elapsed.count() << " seconds." << std::endl;

    return 0;
}


