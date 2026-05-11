#include <iostream>
#include <queue>
#include <set>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <curl/curl.h>

using namespace std;

struct Task {
    string url;
    int depth;
};

queue<Task> urls;
set<string> visited;

pthread_mutex_t lock;
sem_t sem;

int MAX_DEPTH = 2;
int totalProcessed = 0;
int MAX_TASKS = 6; 

size_t writeCallback(void* contents, size_t size, size_t nmemb, string* output) {
    output->append((char*)contents, size * nmemb);
    return size * nmemb;
}

string fetchPage(string url) {
    CURL* curl;
    CURLcode res;
    string buffer;

    curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            buffer = "ERROR";
        }

        curl_easy_cleanup(curl);
    }

    return buffer;
}

void* worker(void* arg) {
    int id = *((int*)arg);

    sem_wait(&sem);

    while (true) {
        pthread_mutex_lock(&lock);

        //  EXIT CONDITION (clean termination)
        if (totalProcessed >= MAX_TASKS && urls.empty()) {
            pthread_mutex_unlock(&lock);
            break;
        }

        // If queue empty → exit
        if (urls.empty()) {
            pthread_mutex_unlock(&lock);
            break;
        }

        Task current = urls.front();
        urls.pop();

        if (visited.find(current.url) != visited.end()) {
            pthread_mutex_unlock(&lock);
            continue;
        }

        visited.insert(current.url);
        totalProcessed++;

        pthread_mutex_unlock(&lock);

        cout << "[Thread " << id << "] Fetching: " << current.url << endl;

        string html = fetchPage(current.url);

        if (html != "ERROR") {
            cout << "Downloaded " << html.size() << " bytes\n";
        } else {
            cout << "Failed to fetch\n";
        }

        sleep(1);

        // Add new URLs
        if (current.depth < MAX_DEPTH) {
            pthread_mutex_lock(&lock);

            urls.push({"https://example.com/page1", current.depth + 1});
            urls.push({"https://example.org/page2", current.depth + 1});

            pthread_mutex_unlock(&lock);
        }
    }

    sem_post(&sem);
    return NULL;
}

int main() {
    pthread_t t1, t2;

    int id1 = 1, id2 = 2;

    pthread_mutex_init(&lock, NULL);
    sem_init(&sem, 0, 2);

    // Seed URLs
    urls.push({"https://example.com", 0});
    urls.push({"https://example.org", 0});

    pthread_create(&t1, NULL, worker, &id1);
    pthread_create(&t2, NULL, worker, &id2);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    pthread_mutex_destroy(&lock);
    sem_destroy(&sem);

    cout << "\n===== STATS =====" << endl;
    cout << "Total processed: " << totalProcessed << endl;

    return 0;
}
