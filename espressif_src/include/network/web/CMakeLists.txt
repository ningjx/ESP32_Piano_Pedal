set(WEB_SRCS "web_server.c")
add_library(web_server ${WEB_SRCS})
target_include_directories(web_server PUBLIC . PRIVATE ../../common)
target_link_libraries(web_server PRIVATE esp_http_server cjson wifi_manager)
