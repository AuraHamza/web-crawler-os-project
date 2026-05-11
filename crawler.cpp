#include <iostream>
#include <queue>
using namespace std;

int main() {
    queue<string> urls;
	    urls.push("google.com");
    urls.push("example.com");

    while (!urls.empty()) {
        string current = urls.front();
        urls.pop();

        cout << "Processing: " << current << endl;

        if (current == "google.com") {
            urls.push("maps.google.com");
            urls.push("mail.google.com");
        }

        if (current == "example.com") {
            urls.push("blog.example.com");
        }
    }

    return 0;
}
