#include <iostream>
#include <pthread.h>
using namespace std;

void* worker(void* arg) {
    int id = *((int*)arg);
    cout << "Thread " << id << " is working" << endl;
    return NULL;
}

int main() {
    pthread_t t1, t2, t3;

    int id1 = 1, id2 = 2, id3 = 3;

    pthread_create(&t1, NULL, worker, &id1);
    pthread_create(&t2, NULL, worker, &id2);
    pthread_create(&t3, NULL, worker, &id3);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);

    return 0;
}
