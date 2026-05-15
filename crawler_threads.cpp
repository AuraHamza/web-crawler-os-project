/**
 * Project: Parallel Web Search Crawler
 * Final Stats Summary Block Added
 */

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

// Task Structure
struct Task {
    string url;
    int depth;
};

// --- Shared Resources ---
queue<Task> urlFrontier;
unordered_set<string> visitedURLs;
unordered_map<string, vector<string>> keywordIndex;

// --- Synchronization Primitives ---
pthread_mutex_t queueLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t visitedLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t statsLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t indexLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queueCond = PTHREAD_COND_INITIALIZER; 
sem_t threadLimitSem;

// --- Stats & State ---
int MAX_THREADS = 4;
int MAX_DEPTH = 2;
int pagesCrawled = 0;
int urlsDiscovered = 0;
int fetchErrors = 0;
int activeThreads = 0;
bool crawlActive = true;
chrono::steady_clock::time_point startTime;

// --- URL Resolution ---
string resolveURL(string baseUrl, string relUrl) {
    if (relUrl.find("http") == 0) return relUrl;
    if (relUrl.empty() || relUrl[0] == '#') return "";
    size_t pos = baseUrl.find("/", 8);
    string domain = (pos == string::npos) ? baseUrl : baseUrl.substr(0, pos);
    if (relUrl[0] == '/') return domain + relUrl;
    return domain + "/" + relUrl;
}

size_t writeCallback(void* contents, size_t size, size_t nmemb, string* output) {
    output->append((char*)contents, size * nmemb);
    return size * nmemb;
}

string fetchPage(string url) {
    CURL* curl = curl_easy_init();
    string buffer;
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        if (res != CURLE_OK) return "";
    }
    return buffer;
}

void processContent(string html, string baseUrl, int depth) {
    regex urlRegex("href=['\"]([^'\"]+)['\"]");
    auto urlBegin = sregex_iterator(html.begin(), html.end(), urlRegex);
    auto urlEnd = sregex_iterator();
    for (sregex_iterator i = urlBegin; i != urlEnd; ++i) {
        string fullUrl = resolveURL(baseUrl, (*i)[1].str());
        if (!fullUrl.empty()) {
            pthread_mutex_lock(&queueLock);
            pthread_mutex_lock(&visitedLock);
            if (visitedURLs.find(fullUrl) == visitedURLs.end() && depth < MAX_DEPTH) {
                urlFrontier.push({fullUrl, depth + 1});
                visitedURLs.insert(fullUrl);
                pthread_cond_signal(&queueCond);
                pthread_mutex_lock(&statsLock);
                urlsDiscovered++;
                pthread_mutex_unlock(&statsLock);
            }
            pthread_mutex_unlock(&visitedLock);
            pthread_mutex_unlock(&queueLock);
        }
    }
    regex wordRegex("\\b[a-zA-Z]{4,15}\\b");
    auto wordBegin = sregex_iterator(html.begin(), html.end(), wordRegex);
    for (sregex_iterator i = wordBegin; i != urlEnd; ++i) {
        pthread_mutex_lock(&indexLock);
        keywordIndex[(*i).str()].push_back(baseUrl);
        pthread_mutex_unlock(&indexLock);
    }
}

void* monitorWorker(void* arg) {
    (void)arg;
    while (crawlActive) {
        sleep(1);
        pthread_mutex_lock(&statsLock);
        auto now = chrono::steady_clock::now();
        chrono::duration<double> diff = now - startTime;
        cout << "\r[Monitor] Pages: " << pagesCrawled << " | Active Threads: " << activeThreads << " | Time: " << (int)diff.count() << "s" << flush;
        pthread_mutex_unlock(&statsLock);
    }
    return NULL;
}

void* crawlerWorker(void* arg) {
    (void)arg;
    while (true) {
        pthread_mutex_lock(&queueLock);
        while (urlFrontier.empty() && crawlActive) {
            pthread_cond_wait(&queueCond, &queueLock);
        }
        if (!crawlActive && urlFrontier.empty()) {
            pthread_mutex_unlock(&queueLock);
            break;
        }
        Task task = urlFrontier.front();
        urlFrontier.pop();
        pthread_mutex_unlock(&queueLock);

        sem_wait(&threadLimitSem);
        pthread_mutex_lock(&statsLock);
        activeThreads++;
        pthread_mutex_unlock(&statsLock);

        string content = fetchPage(task.url);

        pthread_mutex_lock(&statsLock);
        if (content.empty()) fetchErrors++;
        else pagesCrawled++;
        activeThreads--;
        pthread_mutex_unlock(&statsLock);

        if (!content.empty()) processContent(content, task.url, task.depth);
        sem_post(&threadLimitSem);
    }
    return NULL;
}

void resetCrawlerState(int threads) {
    pthread_mutex_lock(&queueLock);
    while(!urlFrontier.empty()) urlFrontier.pop();
    pthread_mutex_unlock(&queueLock);
    
    pthread_mutex_lock(&visitedLock);
    visitedURLs.clear();
    pthread_mutex_unlock(&visitedLock);
    
    pthread_mutex_lock(&indexLock);
    keywordIndex.clear();
    pthread_mutex_unlock(&indexLock);
    
    pthread_mutex_lock(&statsLock);
    pagesCrawled = 0; urlsDiscovered = 0; fetchErrors = 0; activeThreads = 0;
    crawlActive = true;
    MAX_THREADS = threads;
    startTime = chrono::steady_clock::now();
    pthread_mutex_unlock(&statsLock);

    sem_destroy(&threadLimitSem);
    sem_init(&threadLimitSem, 0, MAX_THREADS);
}

int main(int argc, char* argv[]) {
    int requestedThreads = (argc > 1) ? atoi(argv[1]) : 4;
    curl_global_init(CURL_GLOBAL_ALL);

    // PHASE 1
    cout << "\n--- Phase 1: Single-Threaded Test (5s) ---" << endl;
    resetCrawlerState(1);
    urlFrontier.push({"http://example.com", 0}); 
    pthread_t st;
    pthread_create(&st, NULL, crawlerWorker, NULL);
    sleep(5);
    pthread_mutex_lock(&statsLock);
    crawlActive = false;
    pthread_mutex_unlock(&statsLock);
    pthread_cond_broadcast(&queueCond);
    pthread_join(st, NULL);
    int pSingle = pagesCrawled;

    // PHASE 2
    cout << "\n--- Phase 2: Multi-Threaded Test (" << requestedThreads << " threads, 5s) ---" << endl;
    resetCrawlerState(requestedThreads);
    urlFrontier.push({"http://example.com", 0});
    pthread_t mon; 
    pthread_create(&mon, NULL, monitorWorker, NULL);
    pthread_t mt[requestedThreads];
    for(int i=0; i<requestedThreads; i++) pthread_create(&mt[i], NULL, crawlerWorker, NULL);
    
    sleep(5); // Run for 5 seconds
    
    pthread_mutex_lock(&statsLock);
    crawlActive = false;
    pthread_mutex_unlock(&statsLock);
    pthread_cond_broadcast(&queueCond);
    for(int i=0; i<requestedThreads; i++) pthread_join(mt[i], NULL);
    pthread_join(mon, NULL);

    auto endTime = chrono::steady_clock::now();
    chrono::duration<double> totalElapsed = endTime - startTime;

    // PERFORMANCE TABLE
    cout << "\n\nFINAL PERFORMANCE TABLE" << endl;
    cout << "----------------------------------------" << endl;
    cout << left << setw(20) << "Mode" << setw(10) << "Pages" << "Speedup" << endl;
    cout << left << setw(20) << "Single-Thread" << setw(10) << pSingle << "1.00x" << endl;
    double speedup = (pSingle == 0) ? 0 : (double)pagesCrawled / pSingle;
    cout << left << setw(20) << "Multi-Thread" << setw(10) << pagesCrawled << fixed << setprecision(2) << speedup << "x" << endl;
    cout << "----------------------------------------" << endl;

    // --- YOUR REQUESTED SUMMARY BLOCK ---
    cout << "\n--- FINAL CRAWLER SUMMARY ---" << endl;
    cout << left << setw(25) << "Total Pages Crawled" << ": " << pagesCrawled << endl;
    cout << left << setw(25) << "Unique URLs Found"    << ": " << urlsDiscovered << endl;
    cout << left << setw(25) << "Keywords Indexed"     << ": " << keywordIndex.size() << endl;
    cout << left << setw(25) << "Threads Used"         << ": " << requestedThreads << endl;
    cout << left << setw(25) << "Time Elapsed"         << ": " << fixed << setprecision(2) << totalElapsed.count() << "s" << endl;
    cout << "----------------------------------------" << endl;

    curl_global_cleanup();
    return 0;
}
