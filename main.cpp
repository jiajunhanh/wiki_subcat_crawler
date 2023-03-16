#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
#include <mysqlx/xdevapi.h>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

constexpr auto subcategory_links_cache_path = "./data/subcategory_links_cache";

void fetch_subcategory_links_from_database(mysqlx::Session &session) {
    std::ofstream ofs(subcategory_links_cache_path);
    auto rows =
        session
            .sql("SELECT cl_to, page_title FROM categorylinks JOIN page WHERE "
                 "cl_from = page_id AND cl_type = \"subcat\"")
            .execute();
    unsigned int cnt = 0;
    while (auto row = rows.fetchOne()) {
        auto cl_to = std::string(row[0]);
        auto page_title = std::string(row[1]);
        ofs << std::format("{} {}\n", cl_to, page_title);
        if (++cnt % 100000 == 0) {
            std::cout << std::format("Fetched {} links ...", cnt) << std::endl;
        }
    }
    std::cout << std::format("Fetched {} links ...\nDone", cnt) << std::endl;
}

mysqlx::Session connect_to_database() {
    std::ifstream config_ifs("./config.ini");
    if (!config_ifs) {
        throw std::exception("config.ini not found");
    }
    std::string host;
    std::string user;
    std::string password;
    std::string db;
    config_ifs >> host >> user >> password >> db;
    std::cout << std::format("Creating session on {} ...", host) << std::endl;
    mysqlx::Session session{mysqlx::SessionSettings{host, user, password, db}};
    std::cout << "Session accepted" << std::endl;
    return session;
}

std::unordered_multimap<std::string, std::string>
read_subcategory_links(mysqlx::Session &session) {
    std::cout << "Try to read subcategory links from cache" << std::endl;
    std::ifstream subcat_ifs(subcategory_links_cache_path);
    if (!subcat_ifs) {
        std::cout
            << "Cache not found\nFetching subcategory links from database ..."
            << std::endl;
        auto start_time = std::chrono::high_resolution_clock::now();
        fetch_subcategory_links_from_database(session);
        auto end_time = std::chrono::high_resolution_clock::now();
        std::cout << std::format("Elapsed time: {} s",
                                 (end_time - start_time) /
                                     std::chrono::seconds(1))
                  << std::endl;
        subcat_ifs.open(subcategory_links_cache_path);
        if (!subcat_ifs) {
            throw std::exception(
                std::format("{} not found", subcategory_links_cache_path)
                    .c_str());
        }
    }
    std::cout << "Cache found\nReading subcategory links from cache ..."
              << std::endl;
    std::unordered_multimap<std::string, std::string> subcategory_links;
    std::string cl_from;
    std::string cl_to;
    while (subcat_ifs >> cl_to >> cl_from) {
        subcategory_links.emplace(cl_to, cl_from);
    }
    std::cout << std::format("Read {} links from cache",
                             subcategory_links.size())
              << std::endl;
    return subcategory_links;
}

std::unordered_map<std::string, unsigned int>
access_subcategories(const std::vector<std::string> &categories,
                     const std::unordered_multimap<std::string, std::string>
                         &subcategory_links) {

    std::unordered_map<std::string, unsigned int> depths;
    std::queue<std::string> q;
    for (const auto &cat : categories) {
        q.push(cat);
    }
    std::cout << std::format("Accessing subcategories recursively ...")
              << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();
    // SPFA
    while (!q.empty()) {
        auto cat = q.front();
        q.pop();
        auto range = subcategory_links.equal_range(cat);
        for (auto itr = range.first; itr != range.second; ++itr) {
            const auto &subcat = itr->second;
            if (depths.count(subcat) && depths[subcat] <= depths[cat] + 1) {
                continue;
            }
            depths[subcat] = depths[cat] + 1;
            q.emplace(subcat);
        }
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    std::cout << std::format(
                     "Done\nAccessed {} subcategories\nElapsed time: {} s",
                     depths.size(),
                     (end_time - start_time) / std::chrono::seconds(1))
              << std::endl;
    return depths;
}

void dump_subcategories(
    const std::unordered_map<std::string, unsigned int> &subcategory_depths,
    const std::string &out_path,
    const std::unordered_multimap<std::string, std::string>
        &subcategory_links) {
    std::cout << std::format("Writing subcategories to {} ...", out_path)
              << std::endl;
    std::ofstream ofs(out_path);
    if (!ofs) {
        throw std::exception(std::format("Cannot open {}", out_path).c_str());
    }
    for (const auto &[subcat, depth] : subcategory_depths) {
        ofs << subcat << ' ' << depth;
        auto range = subcategory_links.equal_range(subcat);
        for (auto itr = range.first; itr != range.second; ++itr) {
            ofs << ' ' << itr->second;
        }
        ofs << '\n';
    }
    std::cout << "Done" << std::endl;
}

void insert_page(std::unordered_map<unsigned int, unsigned int> &pages,
                 unsigned int cl_from, unsigned int depth) {
    auto itr = pages.find(cl_from);
    if (itr != pages.end()) {
        itr->second = std::min(itr->second, depth);
        return;
    }
    pages.emplace(cl_from, depth);
}

std::unordered_map<unsigned int, unsigned int>
access_pages(const std::unordered_map<std::string, unsigned int> &subcategories,
             mysqlx::Session &session) {
    std::unordered_map<unsigned int, unsigned int> page_depths;
    auto start_time = std::chrono::high_resolution_clock::now();
    std::cout << "Accessing pages of subcategories ..." << std::endl;
    auto total = subcategories.size();
    size_t i = 0;
    for (const auto &[subcat, depth] : subcategories) {
        mysqlx::SqlResult rows;
        try {
            rows = session
                       .sql("SELECT cl_from FROM categorylinks WHERE "
                            "cl_to = ? AND cl_type = \"page\"")
                       .bind(subcat)
                       .execute();
        } catch (std::exception &e) {
            std::cerr << "SQL query failed: " << e.what() << std::endl;
            continue;
        }
        while (auto row = rows.fetchOne()) {
            auto cl_from = static_cast<unsigned int>(row[0]);
            insert_page(page_depths, cl_from, depth);
        }
        if (++i % 10000 == 0) {
            std::cout << std::format(
                             "Accessed {}/{} subcategories, {} pages ...", i,
                             total, page_depths.size())
                      << std::endl;
        }
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    std::cout << std::format("Accessed {}/{} subcategories, {} pages "
                             "...\nDone\nElapsed time: {} s",
                             i, total, page_depths.size(),
                             (end_time - start_time) / std::chrono::seconds(1))
              << std::endl;
    return page_depths;
}

std::string get_page_title_from_id(unsigned int page_id,
                                   mysqlx::Session &session) {
    auto rows = session.sql("SELECT page_title FROM page WHERE page_id = ?")
                    .bind(page_id)
                    .execute();
    auto row = rows.fetchOne();
    if (!row) {
        throw std::exception("Page id not found in the database");
    }
    return std::string(row[0]);
}

void dump_pages(const std::unordered_map<unsigned int, unsigned int> &pages,
                const std::string &out_path, mysqlx::Session &session) {
    auto start_time = std::chrono::high_resolution_clock::now();
    std::cout << std::format("Writing pages to {} ...", out_path) << std::endl;
    std::ofstream ofs(out_path);
    if (!ofs) {
        throw std::exception(std::format("Cannot open {}", out_path).c_str());
    }
    auto total = pages.size();
    size_t i = 0;
    for (auto [page_id, depth] : pages) {
        std::string page_title;
        mysqlx::SqlResult rows;
        try {
            page_title = get_page_title_from_id(page_id, session);
            rows =
                session.sql("SELECT cl_to FROM categorylinks WHERE cl_from = ?")
                    .bind(page_id)
                    .execute();
        } catch (std::exception &e) {
            std::cerr << std::format("Failed to dump page {}: {}", page_id,
                                     e.what())
                      << std::endl;
            continue;
        }
        ofs << page_title << ' ' << page_id << ' ' << depth;
        while (auto row = rows.fetchOne()) {
            auto cat = std::string(row[0]);
            ofs << ' ' << cat;
        }
        ofs << '\n';
        if (++i % 10000 == 0) {
            std::cout << std::format("Wrote {}/{} pages ...", i, total)
                      << std::endl;
        }
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    std::cout << std::format("Wrote {}/{} pages ...\nDone\nElapsed time: {} s",
                             i, total,
                             (end_time - start_time) / std::chrono::seconds(1))
              << std::endl;
}

} // namespace

int main(int argc, char *argv[]) try {
    if (argc < 2) {
        throw std::exception(
            "Usage: ./wiki_crawler <category> [<category> ...]");
    }
    std::vector<std::string> categories;
    for (int i = 1; i < argc; ++i) {
        categories.emplace_back(argv[i]);
    }
    std::string out_dir = "./data/";
    auto subcat_path = out_dir + "subcategories";
    auto page_path = out_dir + "pages";
    for (const auto &category : categories) {
        subcat_path += "__";
        subcat_path += category;
        page_path += "__";
        page_path += category;
    }
    auto session = connect_to_database();
    auto subcategory_links = read_subcategory_links(session);
    auto subcategory_depths =
        access_subcategories(categories, subcategory_links);
    dump_subcategories(subcategory_depths, subcat_path, subcategory_links);
    auto page_depths = access_pages(subcategory_depths, session);
    dump_pages(page_depths, page_path, session);
    return EXIT_SUCCESS;
} catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    return EXIT_FAILURE;
} catch (...) {
    std::cerr << "UNKNOWN EXCEPTION" << std::endl;
    return EXIT_FAILURE;
}