#include <iostream>
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <string>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <curl/curl.h>
#include <chrono>
#include <regex>
#include <iomanip>

using namespace std;

struct task {
    string url;
    int depth;
};

queue<task> url_queue;
unordered_set<string> visited_urls;
unordered_map<string, vector<string>> keyword_index;

pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t visited_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t stats_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t index_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;
sem_t thread_limit;

int max_threads = 4;
int max_depth = 2;
int pages_crawled = 0;
int urls_found = 0;
int fetch_errors = 0;
int active_threads = 0;
bool crawl_active = true;

chrono::steady_clock::time_point start_time;

string resolve_url(string base_url, string relative_url) {
    if (relative_url.find("http") == 0)
        return relative_url;

    if (relative_url.empty() || relative_url[0] == '#')
        return "";

    size_t pos = base_url.find("/", 8);
    string domain = (pos == string::npos) ? base_url : base_url.substr(0, pos);

    if (relative_url[0] == '/')
        return domain + relative_url;

    return domain + "/" + relative_url;
}

size_t write_callback(void* contents, size_t size, size_t nmemb, string* output) {
    output->append((char*)contents, size * nmemb);
    return size * nmemb;
}

string fetch_page(string url) {
    CURL* curl = curl_easy_init();
    string html;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &html);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT,
                         "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");

        CURLcode result = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (result != CURLE_OK)
            return "";
    }

    return html;
}

void process_content(string html, string base_url, int depth) {
    regex link_pattern("href=['\"]([^'\"]+)['\"]");
    auto link_begin = sregex_iterator(html.begin(), html.end(), link_pattern);
    auto link_end = sregex_iterator();

    for (sregex_iterator i = link_begin; i != link_end; ++i) {
        string full_url = resolve_url(base_url, (*i)[1].str());

        if (!full_url.empty()) {
            pthread_mutex_lock(&queue_lock);
            pthread_mutex_lock(&visited_lock);

            if (visited_urls.find(full_url) == visited_urls.end() &&
                depth < max_depth) {
                url_queue.push({full_url, depth + 1});
                visited_urls.insert(full_url);

                pthread_cond_signal(&queue_cond);

                pthread_mutex_lock(&stats_lock);
                urls_found++;
                pthread_mutex_unlock(&stats_lock);
            }

            pthread_mutex_unlock(&visited_lock);
            pthread_mutex_unlock(&queue_lock);
        }
    }

    regex word_pattern("\\b[a-zA-Z]{4,15}\\b");
    auto word_begin = sregex_iterator(html.begin(), html.end(), word_pattern);

    for (sregex_iterator i = word_begin; i != link_end; ++i) {
        pthread_mutex_lock(&index_lock);
        keyword_index[(*i).str()].push_back(base_url);
        pthread_mutex_unlock(&index_lock);
    }
}

void* monitor_worker(void* arg) {
    (void)arg;

    while (crawl_active) {
        sleep(1);

        pthread_mutex_lock(&stats_lock);

        auto now = chrono::steady_clock::now();
        chrono::duration<double> elapsed = now - start_time;

        cout << "\r[Monitor] Pages: " << pages_crawled
             << " | Active Threads: " << active_threads
             << " | Time: " << (int)elapsed.count() << "s"
             << flush;

        pthread_mutex_unlock(&stats_lock);
    }

    return NULL;
}

void* crawler_worker(void* arg) {
    (void)arg;

    while (true) {
        pthread_mutex_lock(&queue_lock);

        while (url_queue.empty() && crawl_active) {
            pthread_cond_wait(&queue_cond, &queue_lock);
        }

        if (!crawl_active && url_queue.empty()) {
            pthread_mutex_unlock(&queue_lock);
            break;
        }

        task current_task = url_queue.front();
        url_queue.pop();

        pthread_mutex_unlock(&queue_lock);

        sem_wait(&thread_limit);

        pthread_mutex_lock(&stats_lock);
        active_threads++;
        pthread_mutex_unlock(&stats_lock);

        string page_content = fetch_page(current_task.url);

        pthread_mutex_lock(&stats_lock);

        if (page_content.empty())
            fetch_errors++;
        else
            pages_crawled++;

        active_threads--;

        pthread_mutex_unlock(&stats_lock);

        if (!page_content.empty()) {
            process_content(page_content,
                            current_task.url,
                            current_task.depth);
        }

        sem_post(&thread_limit);
    }

    return NULL;
}

void reset_crawler(int threads) {
    pthread_mutex_lock(&queue_lock);

    while (!url_queue.empty())
        url_queue.pop();

    pthread_mutex_unlock(&queue_lock);

    pthread_mutex_lock(&visited_lock);
    visited_urls.clear();
    pthread_mutex_unlock(&visited_lock);

    pthread_mutex_lock(&index_lock);
    keyword_index.clear();
    pthread_mutex_unlock(&index_lock);

    pthread_mutex_lock(&stats_lock);

    pages_crawled = 0;
    urls_found = 0;
    fetch_errors = 0;
    active_threads = 0;
    crawl_active = true;
    max_threads = threads;
    start_time = chrono::steady_clock::now();

    pthread_mutex_unlock(&stats_lock);

    sem_destroy(&thread_limit);
    sem_init(&thread_limit, 0, max_threads);
}

int main(int argc, char* argv[]) {
    int requested_threads = (argc > 1) ? atoi(argv[1]) : 4;

    curl_global_init(CURL_GLOBAL_ALL);

    cout << "\n--- Phase 1: Single-Threaded Test (5s) ---" << endl;

    reset_crawler(1);

    url_queue.push({"http://example.com", 0});

    pthread_t single_thread;
    pthread_create(&single_thread, NULL, crawler_worker, NULL);

    sleep(5);

    pthread_mutex_lock(&stats_lock);
    crawl_active = false;
    pthread_mutex_unlock(&stats_lock);

    pthread_cond_broadcast(&queue_cond);

    pthread_join(single_thread, NULL);

    int single_pages = pages_crawled;

    cout << "\n--- Phase 2: Multi-Threaded Test ("
         << requested_threads
         << " threads, 5s) ---"
         << endl;

    reset_crawler(requested_threads);

    url_queue.push({"http://example.com", 0});

    pthread_t monitor_thread;
    pthread_create(&monitor_thread, NULL, monitor_worker, NULL);

    pthread_t worker_threads[requested_threads];

    for (int i = 0; i < requested_threads; i++) {
        pthread_create(&worker_threads[i],
                       NULL,
                       crawler_worker,
                       NULL);
    }

    sleep(5);

    pthread_mutex_lock(&stats_lock);
    crawl_active = false;
    pthread_mutex_unlock(&stats_lock);

    pthread_cond_broadcast(&queue_cond);

    for (int i = 0; i < requested_threads; i++) {
        pthread_join(worker_threads[i], NULL);
    }

    pthread_join(monitor_thread, NULL);

    auto end_time = chrono::steady_clock::now();
    chrono::duration<double> total_time = end_time - start_time;

    cout << "\n\nFINAL PERFORMANCE TABLE" << endl;
    cout << "----------------------------------------" << endl;
    cout << left << setw(20)
         << "Mode"
         << setw(10)
         << "Pages"
         << "Speedup"
         << endl;

    cout << left << setw(20)
         << "Single-Thread"
         << setw(10)
         << single_pages
         << "1.00x"
         << endl;

    double speedup =
        (single_pages == 0)
            ? 0
            : (double)pages_crawled / single_pages;

    cout << left << setw(20)
         << "Multi-Thread"
         << setw(10)
         << pages_crawled
         << fixed
         << setprecision(2)
         << speedup
         << "x"
         << endl;

    cout << "----------------------------------------" << endl;

    cout << "\n--- FINAL CRAWLER SUMMARY ---" << endl;
    cout << left << setw(25)
         << "Total Pages Crawled"
         << ": "
         << pages_crawled
         << endl;

    cout << left << setw(25)
         << "Unique URLs Found"
         << ": "
         << urls_found
         << endl;

    cout << left << setw(25)
         << "Keywords Indexed"
         << ": "
         << keyword_index.size()
         << endl;

    cout << left << setw(25)
         << "Threads Used"
         << ": "
         << requested_threads
         << endl;

    cout << left << setw(25)
         << "Time Elapsed"
         << ": "
         << fixed
         << setprecision(2)
         << total_time.count()
         << "s"
         << endl;

    cout << "----------------------------------------" << endl;

    curl_global_cleanup();

    return 0;
}
