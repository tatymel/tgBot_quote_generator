#include <stdio.h>
#include <string>
#include <vector>
#include <tgbot/tgbot.h>
#include <curl/curl.h>
#include "json.hpp"
#include <opencv2/opencv.hpp>

using json = nlohmann::json;

std::string make_url_quote(std::string& category){
    return "https://api.api-ninjas.com/v1/quotes?category=" + category;
}

size_t write_quote(void *contents, size_t size, size_t nmemb, std::string *data) {
    data->append((char*)contents, size * nmemb);
    return size * nmemb;
}
size_t write_image(void *contents, size_t size, size_t nmemb, FILE *stream) {
    return fwrite(contents, size, nmemb, stream);
}
bool download_image(std::string& api_key, std::string& url_image, std::string& image_path) {
    CURL *curl = curl_easy_init();
    CURLcode res;
    FILE *fp;

    if (curl) {
        fp = fopen(image_path.c_str(), "wb");
        if (!fp) {
            std::cerr << "Failed to open file for writing" << std::endl;
            return false;
        }

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, api_key.c_str());
        headers = curl_slist_append(headers, "Accept: image/jpg");

        curl_easy_setopt(curl, CURLOPT_URL, url_image.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_image);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L); // Handle HTTP errors

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            fclose(fp);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            return false;
        }

        fclose(fp);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return true;
    }
    return false;
}
void add_text_to_image(const std::string& image_path, const std::string& text) {
    cv::Mat image = cv::imread(image_path); 
    if (image.empty()) {
        std::cerr << "Could not open or find the image" << std::endl;
        return;
    }

    int fontFace = cv::FONT_HERSHEY_SIMPLEX;
    double fontScale = 1;
    int thickness = 2;
    cv::Scalar color(255, 255, 255);
    int baseline = 0;
    int lineSpacing = 20;

    std::istringstream iss(text);
    std::vector<std::string> words{std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{}};
    std::vector<std::string> lines;
    std::string curLine = "   ";
    int maxWidth = image.cols - 50;

   for(const std::string& word : words){
    std::string tempLine = curLine + " " + word;
    cv::Size textSize = cv::getTextSize(tempLine, fontFace, fontScale, thickness, &baseline);

    if(textSize.width > maxWidth){
        lines.push_back(curLine);
        curLine = word;
    }else{
        curLine = tempLine;
    }
   }

   if(!curLine.empty()){
    lines.push_back(curLine);
   }

    int textHeight = lines.size() * (cv::getTextSize("Gg", fontFace, fontScale, thickness, &baseline).height + lineSpacing);
    int startY = (image.rows - textHeight) / 2;

    for(int i = 0; i < lines.size(); ++i){
        cv::Point textOrg(10,  startY + (lineSpacing + baseline) * i);
        cv::putText(image, lines[i], textOrg, fontFace, fontScale, color, thickness, 8);
    }
    cv::imwrite(image_path, image);
}

std::string escapeMarkdown(const std::string& text) {
    std::string escapedText;
    for (char c : text) {
        if (c == '_' || c == '*' || c == '[' || c == ']' || c == '(' || c == ')' || c == '~' || 
            c == '`' || c == '>' || c == '#' || c == '+' || c == '-' || c == '=' || c == '|' || 
            c == '{' || c == '}' || c == '.' || c == '!') {
            escapedText += '\\';
        }
        escapedText += c;
    }
    return escapedText;
}
std::string makeQuote(const std::string& response_data, bool forImage) {
    auto jData = json::parse(response_data);
    if (!jData.is_array() || jData.empty()) {
        return "Error: Unexpected JSON format";
    }
    auto entry = jData[0];
    if(forImage){
        return "\"" + entry["quote"].get<std::string>() + "\"\n  " + entry["author"].get<std::string>();
    }
    std::string quote = escapeMarkdown(entry["quote"].get<std::string>());
    std::string author = escapeMarkdown(entry["author"].get<std::string>());
    return "\"_" + quote + "_\"\n\n \\- " + author;
}
std::string response_data;
void makeMessageResponse(TgBot::Bot& bot, TgBot::Message::Ptr& message, std::string& api_key){
    CURL *curl = curl_easy_init();
    if (!curl) {
        bot.getApi().sendMessage(message->chat->id, "Something wrong with our tg ((");
        return;
    }
    CURLcode res;
    response_data.clear();
    curl_easy_setopt(curl, CURLOPT_URL, make_url_quote(message->text).c_str());
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, api_key.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_quote);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    
    res = curl_easy_perform(curl);
    if(res == CURLE_OK){
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

        if(response_code == 200) {
            bot.getApi().sendMessage(message->chat->id, makeQuote(response_data, false), nullptr, nullptr, nullptr, "MarkdownV2");
        } else {
            std::cout << "Error: " << response_code << " " << response_data << std::endl;
        }
    } else {
        std::cerr << "Failed to perform request: " << curl_easy_strerror(res) << std::endl;
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
}

void makePhotoResponse(TgBot::Bot& bot, TgBot::Message::Ptr& message, std::string& api_key, std::string& url_image, std::string& image_path){
    if(download_image(api_key, url_image, image_path)){
        std::cout << "Image successfully downloaded." << std::endl;
        add_text_to_image(image_path, std::move(makeQuote(response_data, true)));
        bot.getApi().sendPhoto(message->chat->id, TgBot::InputFile::fromFile(image_path, "image/jpg"));
    }else{
        std::cout << "Something go wrong with donloading image\n";
    }
}
int main() {
    curl_global_init(CURL_GLOBAL_ALL);

    std::string token_bot = "7266517999:AAGht9a1vzyQclhfrwKHf5aYGN-wQUaW9m4";
    std::string api_key = "X-Api-Key: PS/IDfIm0llwcMedS3Jf9g==YT8UDx1PfdgOZ1A8";
    std::string url_image = "https://api.api-ninjas.com/v1/randomimage?category=nature";
    std::string image_path = "img.jpg";

    TgBot::Bot bot(token_bot);
    
    bot.getEvents().onCommand("start", [&bot](TgBot::Message::Ptr message){
        bot.getApi().sendMessage(message->chat->id, "Hi, " + message->chat->firstName);
        bot.getApi().sendMessage(message->chat->id, "Choose and send me one of the category:\n\tage\n\talone\n\tamazing");
    });

    bot.getEvents().onAnyMessage([&](TgBot::Message::Ptr message){
        printf("User wrote %s\n", message->text.c_str());
        
        if(StringTools::startsWith(message->text, "/start")){
            return;
        }
        if(message->text == "age"){
            bot.getApi().sendMessage(message->chat->id, "Ok, let's make a quote with \"age\"");
        }else if(message->text == "alone"){
            bot.getApi().sendMessage(message->chat->id, "Ok, let's make a quote with \"alone\"");
        }else if(message->text == "amazing"){
            bot.getApi().sendMessage(message->chat->id, "Ok, let's make a quote with \"amazing\"");
        }else{
            bot.getApi().sendMessage(message->chat->id, "I don't understand you .. ");
            return;
        }

        makeMessageResponse(bot, message, api_key);
        makePhotoResponse(bot, message, api_key, url_image, image_path);
    });

    try{
        printf("Bot username: %s\n", bot.getApi().getMe()->username.c_str());
        TgBot::TgLongPoll longPoll(bot);
        while(true){
            printf("Long poll started\n");
            longPoll.start();
        }
    }catch(TgBot::TgException& e){
        printf("error: %s\n", e.what());
    }

    curl_global_cleanup();
    return 0;
}