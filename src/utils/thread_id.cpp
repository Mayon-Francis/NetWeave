#pragma once

#define this_thread_id std::hash<std::thread::id>{}(std::this_thread::get_id())